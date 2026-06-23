/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#include "flux/compiler/ast.h"
#include "flux/compiler/parser.h"

namespace Flux {

std::unique_ptr<ExprAST> Parser::ParseAboveExpr()
{
    getNextToken(); // eat above

    if (CurTok != '(') {
        ReportError("expected '(' after 'above'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Expr = ParseExpression();
    if (!Expr)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' in above expression");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto Threshold = ParseExpression();
    if (!Threshold)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after above expression");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<AboveExprAST>(std::move(Expr), std::move(Threshold));
}

// Parse timer()
std::unique_ptr<ExprAST> Parser::ParseTimerExpr()
{
    getNextToken(); // eat timer

    if (CurTok != '(') {
        ReportError("expected '(' after 'timer'");
        return nullptr;
    }
    getNextToken(); // eat (

    if (CurTok != ')') {
        ReportError("expected ')' after timer");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<TimerExprAST>();
}

// Parse fsm(initial_state, transitions, "output_fn")
std::unique_ptr<ExprAST> Parser::ParseFSMExpr()
{
    getNextToken(); // eat fsm

    if (CurTok != '(') {
        ReportError("expected '(' after 'fsm'");
        return nullptr;
    }
    getNextToken(); // eat (

    // Parse initial state
    if (CurTok != static_cast<int>(TokenType::tok_number)) {
        ReportError("expected numeric initial state");
        return nullptr;
    }
    int InitialState = static_cast<int>(m_lexer.NumVal);
    getNextToken();

    auto FSM = std::make_unique<FSMExprAST>(InitialState, "moore");

    // Parse transitions array
    if (CurTok != ',') {
        ReportError("expected ',' after initial state");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != '[') {
        ReportError("expected '[' for transitions array");
        return nullptr;
    }
    getNextToken(); // eat [

    while (CurTok != ']' && CurTok != static_cast<int>(TokenType::tok_eof)) {
        // Parse transition: { cur_state, next_state, condition, output }
        if (CurTok != '{') {
            ReportError("expected '{' for transition");
            return nullptr;
        }
        getNextToken(); // eat {

        // Current state
        if (CurTok != static_cast<int>(TokenType::tok_number)) {
            ReportError("expected numeric current state");
            return nullptr;
        }
        int CurState = static_cast<int>(m_lexer.NumVal);
        getNextToken();

        if (CurTok != ',') {
            ReportError("expected ',' between current state and next state in transition");
            return nullptr;
        }
        getNextToken();

        // Next state
        if (CurTok != static_cast<int>(TokenType::tok_number)) {
            ReportError("expected numeric next state");
            return nullptr;
        }
        int NextState = static_cast<int>(m_lexer.NumVal);
        getNextToken();

        if (CurTok != ',') {
            ReportError("expected ',' between next state and condition in transition");
            return nullptr;
        }
        getNextToken();

        // Condition
        auto Cond = ParseExpression();
        if (!Cond)
            return nullptr;

        if (CurTok != ',') {
            ReportError("expected ',' between condition and output in transition");
            return nullptr;
        }
        getNextToken();

        // Output
        auto Out = ParseExpression();
        if (!Out)
            return nullptr;

        FSM->addTransition(CurState, NextState, std::move(Cond), std::move(Out));

        if (CurTok == '}')
            getNextToken(); // eat }
        if (CurTok == ',')
            getNextToken(); // eat , between transitions
    }

    if (CurTok != ']') {
        ReportError("expected ']' after transitions");
        return nullptr;
    }
    getNextToken(); // eat ]

    // Optional output function type
    if (CurTok == ',') {
        getNextToken();
        if (CurTok == static_cast<int>(TokenType::tok_string)) {
            std::string fn = m_lexer.StringVal;
            if (fn == "mealy" || fn == "moore") {
                // We need to recreate the FSM with the right output fn
                // For now, we'll store it - in a full impl we'd rebuild
            }
            getNextToken();
        }
    }

    if (CurTok != ')') {
        ReportError("expected ')' after fsm");
        return nullptr;
    }
    getNextToken(); // eat )

    return FSM;
}

void FSMExprAST::addTransition(int cur, int next, std::unique_ptr<ExprAST> cond, std::unique_ptr<ExprAST> out)
{
    Transitions.emplace_back(cur, next, std::move(cond), std::move(out));
}

// Parse edge(expr) or posedge(expr) / negedge(expr)
std::unique_ptr<ExprAST> Parser::ParseEdgeExpr()
{
    // Save the keyword before consuming the next token
    std::string edgeStr = m_lexer.IdentifierStr;
    getNextToken(); // eat edge/posedge/negedge keyword

    int edgeType = 0; // any
    if (edgeStr == "posedge")
        edgeType = 1;
    else if (edgeStr == "negedge")
        edgeType = -1;
    // If it was generic 'edge', edgeType stays 0 (will be set from args)

    if (CurTok != '(') {
        ReportError("expected '(' after edge keyword");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Expr = ParseExpression();
    if (!Expr)
        return nullptr;

    // If generic 'edge', check for type argument
    if (edgeType == 0 && CurTok == ',') {
        getNextToken();
        if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
            std::string typeStr = m_lexer.IdentifierStr;
            if (typeStr == "rising" || typeStr == "posedge")
                edgeType = 1;
            else if (typeStr == "falling" || typeStr == "negedge")
                edgeType = -1;
            getNextToken();
        }
    }

    if (CurTok != ')') {
        ReportError("expected ')' after edge expression");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<EdgeExprAST>(std::move(Expr), edgeType, edgeStr);
}

// Parse triggered(event, body)
std::unique_ptr<ExprAST> Parser::ParseTriggeredExpr()
{
    getNextToken(); // eat triggered

    if (CurTok != '(') {
        ReportError("expected '(' after 'triggered'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Event = ParseExpression();
    if (!Event)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' in triggered expression");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto Body = ParseExpression();
    if (!Body)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after triggered expression");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<TriggeredExprAST>(std::move(Event), std::move(Body));
}

// Parse noise(type, amplitude, [freq])
std::unique_ptr<ExprAST> Parser::ParseNoiseExpr()
{
    getNextToken(); // eat noise

    if (CurTok != '(') {
        ReportError("expected '(' after 'noise'");
        return nullptr;
    }
    getNextToken(); // eat (

    // Parse noise type string
    if (CurTok != static_cast<int>(TokenType::tok_string) && CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected noise type string");
        return nullptr;
    }
    std::string noiseType;
    if (CurTok == static_cast<int>(TokenType::tok_string)) {
        noiseType = m_lexer.StringVal;
    } else {
        noiseType = m_lexer.IdentifierStr;
    }
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' after noise type");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto Amplitude = ParseExpression();
    if (!Amplitude)
        return nullptr;

    std::unique_ptr<ExprAST> Freq = nullptr;
    if (CurTok == ',') {
        getNextToken(); // eat ,
        Freq = ParseExpression();
        if (!Freq)
            return nullptr;
    }

    if (CurTok != ')') {
        ReportError("expected ')' after noise expression");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<NoiseExprAST>(std::move(noiseType), std::move(Amplitude), std::move(Freq));
}

// Parse white_noise(amplitude)
std::unique_ptr<ExprAST> Parser::ParseWhiteNoiseExpr()
{
    getNextToken(); // eat white_noise

    if (CurTok != '(') {
        ReportError("expected '(' after 'white_noise'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Amp = ParseExpression();
    if (!Amp)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after white_noise");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<WhiteNoiseExprAST>(std::move(Amp));
}

// Parse flicker_noise(amplitude, corner_freq)
std::unique_ptr<ExprAST> Parser::ParseFlickerNoiseExpr()
{
    getNextToken(); // eat flicker_noise

    if (CurTok != '(') {
        ReportError("expected '(' after 'flicker_noise'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Amp = ParseExpression();
    if (!Amp)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' in flicker_noise");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto CF = ParseExpression();
    if (!CF)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after flicker_noise");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<FlickerNoiseExprAST>(std::move(Amp), std::move(CF));
}

// Parse thermal_noise(resistance, [temperature])
std::unique_ptr<ExprAST> Parser::ParseThermalNoiseExpr()
{
    getNextToken(); // eat thermal_noise

    if (CurTok != '(') {
        ReportError("expected '(' after 'thermal_noise'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Res = ParseExpression();
    if (!Res)
        return nullptr;

    std::unique_ptr<ExprAST> Temp = nullptr;
    if (CurTok == ',') {
        getNextToken(); // eat ,
        Temp = ParseExpression();
        if (!Temp)
            return nullptr;
    }

    if (CurTok != ')') {
        ReportError("expected ')' after thermal_noise");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<ThermalNoiseExprAST>(std::move(Res), std::move(Temp));
}

// Parse piecewise(x1, y1, x2, y2, ..., [interpolate="linear"], x)
std::unique_ptr<ExprAST> Parser::ParsePiecewiseExpr()
{
    getNextToken(); // eat piecewise

    if (CurTok != '(') {
        ReportError("expected '(' after 'piecewise'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto PW = std::make_unique<PiecewiseExprAST>("linear");

    // Parse pairs of (x, y)
    while (CurTok != ')' && CurTok != static_cast<int>(TokenType::tok_eof)) {
        auto X = ParseExpression();
        if (!X)
            return nullptr;

        if (CurTok != ',') {
            ReportError("expected ',' in piecewise");
            return nullptr;
        }
        getNextToken(); // eat ,

        auto Y = ParseExpression();
        if (!Y)
            return nullptr;

        // Check if next is another pair or query x
        if (CurTok == ',') {
            getNextToken(); // peek
            // If we see interpolate= or a standalone x (query), handle accordingly
            if (CurTok == static_cast<int>(TokenType::tok_identifier) &&
                (m_lexer.IdentifierStr == "interpolate" || m_lexer.IdentifierStr == "query")) {
                auto KeyId = m_lexer.IdentifierStr;
                getNextToken(); // eat identifier
                if (CurTok != '=') {
                    PW->setQueryX(std::move(X));
                    break;
                }
                getNextToken(); // eat =
                if (KeyId == "interpolate") {
                    if (CurTok == static_cast<int>(TokenType::tok_string)) {
                        // We'd need to rebuild PW with new interpolation
                        getNextToken();
                    }
                }
                continue;
            } else {
                // It was another pair
                PW->addPoint(std::move(X), std::move(Y));
                continue;
            }
        } else {
            // End of pairs
            PW->addPoint(std::move(X), std::move(Y));
            break;
        }
    }

    if (CurTok != ')') {
        ReportError("expected ')' after piecewise");
        return nullptr;
    }
    getNextToken(); // eat )

    return PW;
}

void PiecewiseExprAST::addPoint(std::unique_ptr<ExprAST> x, std::unique_ptr<ExprAST> y)
{
    Points.emplace_back(std::move(x), std::move(y));
}

// Parse table(k1, v1, k2, v2, ..., [default=v], key)
std::unique_ptr<ExprAST> Parser::ParseTableExpr()
{
    getNextToken(); // eat table

    if (CurTok != '(') {
        ReportError("expected '(' after 'table'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Table = std::make_unique<TableExprAST>();

    while (CurTok != ')' && CurTok != static_cast<int>(TokenType::tok_eof)) {
        auto Key = ParseExpression();
        if (!Key)
            return nullptr;

        if (CurTok == ',') {
            getNextToken();
            // Check if this was the query key (last item)
            if (CurTok == ')') {
                Table->setQueryKey(std::move(Key));
                break;
            }
        } else {
            // No comma - this should be the query key
            Table->setQueryKey(std::move(Key));
            break;
        }

        auto Val = ParseExpression();
        if (!Val)
            return nullptr;

        Table->addEntry(std::move(Key), std::move(Val));

        if (CurTok == ',')
            getNextToken();
    }

    if (CurTok != ')') {
        ReportError("expected ')' after table");
        return nullptr;
    }
    getNextToken(); // eat )

    return Table;
}

void TableExprAST::addEntry(std::unique_ptr<ExprAST> key, std::unique_ptr<ExprAST> val)
{
    Entries.emplace_back(std::move(key), std::move(val));
}

// Parse csv_import("file.csv", [options])
std::unique_ptr<ExprAST> Parser::ParseCsvImportExpr()
{
    getNextToken(); // eat csv_import

    if (CurTok != '(') {
        ReportError("expected '(' after 'csv_import'");
        return nullptr;
    }
    getNextToken(); // eat (

    if (CurTok != static_cast<int>(TokenType::tok_string)) {
        ReportError("expected filename string");
        return nullptr;
    }
    std::string filename = m_lexer.StringVal;
    getNextToken();

    std::map<std::string, std::string> options;
    if (CurTok == ',') {
        getNextToken();
        // Parse options as key=value pairs or just a dict
        if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
            getNextToken(); // eat {
            while (CurTok != '}' && CurTok != static_cast<int>(TokenType::tok_eof)) {
                if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    std::string k = m_lexer.IdentifierStr;
                    getNextToken();
                    if (CurTok == '=') {
                        getNextToken();
                        if (CurTok == static_cast<int>(TokenType::tok_string)) {
                            options[k] = m_lexer.StringVal;
                            getNextToken();
                        }
                    }
                }
                if (CurTok == ',')
                    getNextToken();
            }
            if (CurTok == '}')
                getNextToken();
        }
    }

    if (CurTok != ')') {
        ReportError("expected ')' after csv_import");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<CsvImportExprAST>(std::move(filename), std::move(options));
}

// Parse unit(value, "unit_string")
std::unique_ptr<ExprAST> Parser::ParseUnitExpr()
{
    getNextToken(); // eat unit

    if (CurTok != '(') {
        ReportError("expected '(' after 'unit'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Val = ParseExpression();
    if (!Val)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' in unit expression");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_string)) {
        ReportError("expected unit string");
        return nullptr;
    }
    std::string unitStr = m_lexer.StringVal;
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after unit");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<UnitExprAST>(std::move(Val), std::move(unitStr));
}

// Parse dimension(expr)
std::unique_ptr<ExprAST> Parser::ParseDimensionExpr()
{
    getNextToken(); // eat dimension

    if (CurTok != '(') {
        ReportError("expected '(' after 'dimension'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Expr = ParseExpression();
    if (!Expr)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after dimension");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<DimensionExprAST>(std::move(Expr));
}

// Parse convert(value, "from_unit", "to_unit")
std::unique_ptr<ExprAST> Parser::ParseConvertExpr()
{
    getNextToken(); // eat convert

    if (CurTok != '(') {
        ReportError("expected '(' after 'convert'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Val = ParseExpression();
    if (!Val)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' in convert");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_string)) {
        ReportError("expected from_unit string");
        return nullptr;
    }
    std::string fromUnit = m_lexer.StringVal;
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' in convert");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_string)) {
        ReportError("expected to_unit string");
        return nullptr;
    }
    std::string toUnit = m_lexer.StringVal;
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after convert");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<ConvertExprAST>(std::move(Val), std::move(fromUnit), std::move(toUnit));
}

// Parse has_unit(value, "unit_string")
std::unique_ptr<ExprAST> Parser::ParseHasUnitExpr()
{
    getNextToken(); // eat has_unit

    if (CurTok != '(') {
        ReportError("expected '(' after 'has_unit'");
        return nullptr;
    }
    getNextToken(); // eat (

    auto Val = ParseExpression();
    if (!Val)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' in has_unit");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok != static_cast<int>(TokenType::tok_string)) {
        ReportError("expected unit string");
        return nullptr;
    }
    std::string unitStr = m_lexer.StringVal;
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after has_unit");
        return nullptr;
    }
    getNextToken(); // eat )

    return std::make_unique<HasUnitExprAST>(std::move(Val), std::move(unitStr));
}

std::unique_ptr<ExprAST> Parser::ParseTrainExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat train

    if (CurTok != '(') {
        ReportError("expected '(' after train");
        return nullptr;
    }
    getNextToken(); // eat (

    auto model = ParseExpression();
    if (!model)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after model in train");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto inputs = ParseExpression();
    if (!inputs)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after inputs in train");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto outputs = ParseExpression();
    if (!outputs)
        return nullptr;

    int epochs = 100;
    if (CurTok == ',') {
        getNextToken();
        if (CurTok != static_cast<int>(TokenType::tok_number)) {
            ReportError("expected number of epochs");
            return nullptr;
        }
        epochs = (int)m_lexer.NumVal;
        getNextToken();
    }

    if (CurTok != ')') {
        ReportError("expected ')' after train arguments");
        return nullptr;
    }
    getNextToken();

    auto Res = std::make_unique<TrainExprAST>(std::move(model), std::move(inputs), std::move(outputs), epochs);
    Res->setLocation(line, col);
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParsePredictExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat predict

    if (CurTok != '(') {
        ReportError("expected '(' after predict");
        return nullptr;
    }
    getNextToken(); // eat (

    auto model = ParseExpression();
    if (!model)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after model in predict");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto input = ParseExpression();
    if (!input)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after predict arguments");
        return nullptr;
    }
    getNextToken();

    auto Res = std::make_unique<PredictExprAST>(std::move(model), std::move(input));
    Res->setLocation(line, col);
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParseGoalExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat goal

    if (CurTok != '(') {
        ReportError("expected '(' after goal");
        return nullptr;
    }
    getNextToken(); // eat (

    auto expr = ParseExpression();
    if (!expr)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after expression in goal");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto target = ParseExpression();
    if (!target)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after goal");
        return nullptr;
    }
    getNextToken(); // eat )

    auto Res = std::make_unique<GoalExprAST>(std::move(expr), std::move(target));
    Res->setLocation(line, col);
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParseOptimizeExpr()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat optimize

    if (CurTok != '(') {
        ReportError("expected '(' after optimize");
        return nullptr;
    }
    getNextToken(); // eat (

    // optimize() has no arguments for now, just runs the global optimizer
    if (CurTok != ')') {
        ReportError("expected ')' after optimize");
        return nullptr;
    }
    getNextToken(); // eat )

    auto Res = std::make_unique<CallExprAST>("optimize", std::vector<std::unique_ptr<ExprAST>>());
    Res->setLocation(line, col);
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParseThermalBlock()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat thermal

    if (CurTok != '(') {
        ReportError("expected '(' after thermal");
        return nullptr;
    }
    getNextToken(); // eat (

    auto power = ParseExpression();
    if (!power)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after power in thermal");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto res = ParseExpression();
    if (!res)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after resistance in thermal");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto cap = ParseExpression();
    if (!cap)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after thermal arguments");
        return nullptr;
    }
    getNextToken(); // eat )

    auto Res = std::make_unique<ThermalBlockAST>(std::move(power), std::move(res), std::move(cap));
    Res->setLocation(line, col);
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParseMonteCarlo()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat montecarlo

    if (CurTok != '(') {
        ReportError("expected '(' after montecarlo");
        return nullptr;
    }
    getNextToken(); // eat (

    std::string outputName;
    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        outputName = m_lexer.IdentifierStr;
        getNextToken();
    } else if (CurTok == static_cast<int>(TokenType::tok_string)) {
        outputName = m_lexer.StringVal;
        getNextToken();
    } else {
        ReportError("expected output name in montecarlo");
        return nullptr;
    }

    std::vector<std::string> params;
    if (CurTok != ',') {
        ReportError("expected ',' after output name in montecarlo");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (CurTok == '[') {
        getNextToken(); // eat [
        while (CurTok != ']') {
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                ReportError("expected parameter name in montecarlo list");
                return nullptr;
            }
            params.push_back(m_lexer.IdentifierStr);
            getNextToken();
            if (CurTok == ',')
                getNextToken();
        }
        getNextToken(); // eat ]
    } else {
        if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
            ReportError("expected parameter name or list in montecarlo");
            return nullptr;
        }
        params.push_back(m_lexer.IdentifierStr);
        getNextToken();
    }

    int iters = 100;
    if (CurTok == ',') {
        getNextToken(); // eat ,
        if (CurTok != static_cast<int>(TokenType::tok_number)) {
            ReportError("expected number of iterations in montecarlo");
            return nullptr;
        }
        iters = (int)m_lexer.NumVal;
        getNextToken();
    }

    if (CurTok != ')') {
        ReportError("expected ')' after montecarlo arguments");
        return nullptr;
    }
    getNextToken(); // eat )

    auto Res = std::make_unique<MonteCarloExprAST>(outputName, std::move(params),
                                                    std::vector<double>{}, std::vector<double>{}, iters);
    Res->setLocation(line, col);
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParseVirtualProbe()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat virtual_probe

    if (CurTok != '(') {
        ReportError("expected '(' after virtual_probe");
        return nullptr;
    }
    getNextToken(); // eat (

    auto sig = ParseExpression();
    if (!sig)
        return nullptr;

    std::string name = "scope1";
    if (CurTok == ',') {
        getNextToken(); // eat ,
        if (CurTok != static_cast<int>(TokenType::tok_string)) {
            ReportError("expected probe name string in virtual_probe");
            return nullptr;
        }
        name = m_lexer.StringVal;
        getNextToken();
    }

    if (CurTok != ')') {
        ReportError("expected ')' after virtual_probe arguments");
        return nullptr;
    }
    getNextToken(); // eat )

    auto Res = std::make_unique<VirtualProbeExprAST>(std::move(sig), name);
    Res->setLocation(line, col);
    return Res;
}

std::unique_ptr<ExprAST> Parser::ParseHotSwap()
{
    int line = m_lexer.getCurrentLine();
    int col = m_lexer.getCurrentColumn();
    getNextToken(); // eat hot_swap

    if (CurTok != '(') {
        ReportError("expected '(' after hot_swap");
        return nullptr;
    }
    getNextToken(); // eat (

    auto subckt = ParseExpression();
    if (!subckt)
        return nullptr;

    if (CurTok != ',') {
        ReportError("expected ',' after subcircuit name in hot_swap");
        return nullptr;
    }
    getNextToken(); // eat ,

    auto model = ParseExpression();
    if (!model)
        return nullptr;

    if (CurTok != ')') {
        ReportError("expected ')' after hot_swap arguments");
        return nullptr;
    }
    getNextToken(); // eat )

    auto Res = std::make_unique<HotSwapExprAST>(std::move(subckt), std::move(model));
    Res->setLocation(line, col);
    return Res;
}

// ============================================================================
// User-defined type parsers
// ============================================================================


} // namespace Flux
