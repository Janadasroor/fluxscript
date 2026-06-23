/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0 */

#include "flux/compiler/ast.h"
#include "flux/compiler/parser.h"

namespace Flux {

std::unique_ptr<ExprAST> Parser::ParseBuiltinVar()
{
    std::string VarName = m_lexer.IdentifierStr;
    getNextToken(); // eat variable name
    return std::make_unique<BuiltinVarExprAST>(VarName);
}

// Parse update function: update(t, inputs) or update(t, inputs, outputs, state) { ... }
std::unique_ptr<ExprAST> Parser::ParseUpdateFunc()
{
    getNextToken(); // eat update

    if (CurTok != '(') {
        ReportError("expected '(' after update");
        return nullptr;
    }
    getNextToken(); // eat (

    // Parse time variable name
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected time variable name");
        return nullptr;
    }
    std::string TimeVar = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' in update function");
        return nullptr;
    }
    getNextToken(); // eat ,

    // Parse inputs variable name (also accept keyword tok_inputs as a variable name)
    if (CurTok != static_cast<int>(TokenType::tok_identifier) && CurTok != static_cast<int>(TokenType::tok_inputs)) {
        ReportError("expected inputs variable name");
        return nullptr;
    }
    std::string InputsVar = (CurTok == static_cast<int>(TokenType::tok_inputs)) ? "inputs" : m_lexer.IdentifierStr;
    getNextToken();

    // Check for optional outputs and state parameters
    std::string OutputsVar, StateVar;
    if (CurTok == ',') {
        getNextToken(); // eat ,
        if (CurTok != static_cast<int>(TokenType::tok_identifier) &&
            CurTok != static_cast<int>(TokenType::tok_outputs)) {
            ReportError("expected outputs variable name");
            return nullptr;
        }
        OutputsVar = (CurTok == static_cast<int>(TokenType::tok_outputs)) ? "outputs" : m_lexer.IdentifierStr;
        getNextToken();

        if (CurTok == ',') {
            getNextToken(); // eat ,
            if (CurTok != static_cast<int>(TokenType::tok_identifier) &&
                CurTok != static_cast<int>(TokenType::tok_state)) {
                ReportError("expected state variable name");
                return nullptr;
            }
            StateVar = (CurTok == static_cast<int>(TokenType::tok_state)) ? "state" : m_lexer.IdentifierStr;
            getNextToken();
        }
    }

    if (CurTok != ')') {
        ReportError("expected ')' after update parameters");
        return nullptr;
    }
    getNextToken(); // eat )

    // Parse body
    auto Body = ParseBlockExpr();
    if (!Body) {
        ReportError("expected body for update function");
        return nullptr;
    }

    if (!OutputsVar.empty() && !StateVar.empty()) {
        return std::make_unique<UpdateFuncAST>(TimeVar, InputsVar, OutputsVar, StateVar, std::move(Body));
    }
    return std::make_unique<UpdateFuncAST>(TimeVar, InputsVar, std::move(Body));
}

// Parse B-source: bsource name V(node+, node-) { expression }
std::unique_ptr<ExprAST> Parser::ParseBSource()
{
    getNextToken(); // eat bsource

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected B-source name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Parse V or I prefix
    bool IsCurrent = false;
    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
        std::string Prefix = m_lexer.IdentifierStr;
        if (Prefix == "I")
            IsCurrent = true;
        else if (Prefix != "V") {
            ReportError("expected V or I prefix for B-source");
            return nullptr;
        }
        getNextToken();
    }

    // Parse V(node+, node-)
    if (CurTok != '(') {
        ReportError("expected '(' for B-source nodes");
        return nullptr;
    }
    getNextToken(); // eat (

    auto isNodeToken = [](int tok) {
        return tok == static_cast<int>(TokenType::tok_identifier) || tok == static_cast<int>(TokenType::tok_number) ||
               tok == static_cast<int>(TokenType::tok_integer);
    };
    auto nodeName = [&](int tok) -> std::string {
        if (tok == static_cast<int>(TokenType::tok_identifier))
            return m_lexer.IdentifierStr;
        if (tok == static_cast<int>(TokenType::tok_number))
            return std::to_string(m_lexer.NumVal);
        return std::to_string(static_cast<int>(m_lexer.IntVal));
    };
    if (!isNodeToken(CurTok)) {
        ReportError("expected positive node name");
        return nullptr;
    }
    std::string PosNode = nodeName(CurTok);
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' between nodes");
        return nullptr;
    }
    getNextToken(); // eat ,

    if (!isNodeToken(CurTok)) {
        ReportError("expected negative node name");
        return nullptr;
    }
    std::string NegNode = nodeName(CurTok);
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after nodes");
        return nullptr;
    }
    getNextToken(); // eat )

    // Parse expression body
    auto Expr = ParseBlockExpr();
    if (!Expr) {
        ReportError("expected expression body for B-source");
        return nullptr;
    }

    return std::make_unique<BSourceExprAST>(Name, PosNode, NegNode, std::move(Expr), IsCurrent);
}

// Parse E-source (VCVS): esource name V(node+, node-) V(ctrl+, ctrl-) { gain }
std::unique_ptr<ExprAST> Parser::ParseESource()
{
    getNextToken(); // eat esource

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected E-source name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Parse output nodes V(node+, node-)
    if (CurTok != '(') {
        ReportError("expected '(' for E-source output nodes");
        return nullptr;
    }
    getNextToken();

    std::string PosNode =
        (CurTok == static_cast<int>(TokenType::tok_identifier) || CurTok == static_cast<int>(TokenType::tok_number))
            ? ((CurTok == static_cast<int>(TokenType::tok_identifier))
                   ? m_lexer.IdentifierStr
                   : std::to_string(static_cast<int>(m_lexer.NumVal)))
            : "";
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' between E-source output nodes");
        return nullptr;
    }
    getNextToken();

    std::string NegNode =
        (CurTok == static_cast<int>(TokenType::tok_identifier) || CurTok == static_cast<int>(TokenType::tok_number))
            ? ((CurTok == static_cast<int>(TokenType::tok_identifier))
                   ? m_lexer.IdentifierStr
                   : std::to_string(static_cast<int>(m_lexer.NumVal)))
            : "";
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after E-source output nodes");
        return nullptr;
    }
    getNextToken();

    // Parse control nodes V(ctrl+, ctrl-)
    if (CurTok != '(') {
        ReportError("expected '(' for E-source control nodes");
        return nullptr;
    }
    getNextToken();

    std::string CtrlPosNode =
        (CurTok == static_cast<int>(TokenType::tok_identifier) || CurTok == static_cast<int>(TokenType::tok_number))
            ? ((CurTok == static_cast<int>(TokenType::tok_identifier))
                   ? m_lexer.IdentifierStr
                   : std::to_string(static_cast<int>(m_lexer.NumVal)))
            : "";
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' between E-source control nodes");
        return nullptr;
    }
    getNextToken();

    std::string CtrlNegNode =
        (CurTok == static_cast<int>(TokenType::tok_identifier) || CurTok == static_cast<int>(TokenType::tok_number))
            ? ((CurTok == static_cast<int>(TokenType::tok_identifier))
                   ? m_lexer.IdentifierStr
                   : std::to_string(static_cast<int>(m_lexer.NumVal)))
            : "";
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after E-source control nodes");
        return nullptr;
    }
    getNextToken();

    // Parse gain expression
    auto Gain = ParseBlockExpr();
    if (!Gain)
        Gain = std::make_unique<NumberExprAST>(1.0);

    return std::make_unique<ESourceExprAST>(Name, PosNode, NegNode, CtrlPosNode, CtrlNegNode, std::move(Gain));
}

// Parse F-source (CCCS): fsource name V(node+, node-) Vname { gain }
std::unique_ptr<ExprAST> Parser::ParseFSource()
{
    getNextToken(); // eat fsource

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected F-source name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Parse output nodes
    if (CurTok != '(') {
        ReportError("expected '(' for F-source output nodes");
        return nullptr;
    }
    getNextToken();

    std::string PosNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                              ? m_lexer.IdentifierStr
                              : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' between F-source output nodes");
        return nullptr;
    }
    getNextToken();

    std::string NegNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                              ? m_lexer.IdentifierStr
                              : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after F-source output nodes");
        return nullptr;
    }
    getNextToken();

    // Parse voltage source name for current sensing
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected voltage source name for current sensing");
        return nullptr;
    }
    std::string VSourceName = m_lexer.IdentifierStr;
    getNextToken();

    // Parse gain
    auto Gain = ParseBlockExpr();
    if (!Gain)
        Gain = std::make_unique<NumberExprAST>(1.0);

    return std::make_unique<FSourceExprAST>(Name, PosNode, NegNode, VSourceName, std::move(Gain));
}

// Parse G-source (VCCS): gsource name V(node+, node-) V(ctrl+, ctrl-) { transconductance }
std::unique_ptr<ExprAST> Parser::ParseGSource()
{
    getNextToken(); // eat gsource

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected G-source name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Parse output nodes
    if (CurTok != '(') {
        ReportError("expected '(' for G-source output nodes");
        return nullptr;
    }
    getNextToken();

    std::string PosNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                              ? m_lexer.IdentifierStr
                              : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' between G-source output nodes");
        return nullptr;
    }
    getNextToken();

    std::string NegNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                              ? m_lexer.IdentifierStr
                              : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after G-source output nodes");
        return nullptr;
    }
    getNextToken();

    // Parse control nodes
    if (CurTok != '(') {
        ReportError("expected '(' for control nodes");
        return nullptr;
    }
    getNextToken();

    std::string CtrlPosNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                                  ? m_lexer.IdentifierStr
                                  : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' between G-source control nodes");
        return nullptr;
    }
    getNextToken();

    std::string CtrlNegNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                                  ? m_lexer.IdentifierStr
                                  : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after G-source control nodes");
        return nullptr;
    }
    getNextToken();

    // Parse transconductance
    auto Transcond = ParseBlockExpr();
    if (!Transcond)
        Transcond = std::make_unique<NumberExprAST>(1.0);

    return std::make_unique<GSourceExprAST>(Name, PosNode, NegNode, CtrlPosNode, CtrlNegNode, std::move(Transcond));
}

// Parse H-source (CCVS): hsource name V(node+, node-) Vname { transresistance }
std::unique_ptr<ExprAST> Parser::ParseHSource()
{
    getNextToken(); // eat hsource

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected H-source name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Parse output nodes
    if (CurTok != '(') {
        ReportError("expected '(' for H-source output nodes");
        return nullptr;
    }
    getNextToken();

    std::string PosNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                              ? m_lexer.IdentifierStr
                              : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ',') {
        ReportError("expected ',' between H-source output nodes");
        return nullptr;
    }
    getNextToken();

    std::string NegNode = (CurTok == static_cast<int>(TokenType::tok_identifier))
                              ? m_lexer.IdentifierStr
                              : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after H-source output nodes");
        return nullptr;
    }
    getNextToken();

    // Parse voltage source name
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected voltage source name");
        return nullptr;
    }
    std::string VSourceName = m_lexer.IdentifierStr;
    getNextToken();

    // Parse transresistance
    auto Transres = ParseBlockExpr();
    if (!Transres)
        Transres = std::make_unique<NumberExprAST>(1.0);

    return std::make_unique<HSourceExprAST>(Name, PosNode, NegNode, VSourceName, std::move(Transres));
}
// Parse analysis directive: analysis tran { tstop, tstart, tstep }
std::unique_ptr<AnalysisExprAST> Parser::ParseAnalysis()
{
    getNextToken(); // eat analysis

    std::string typeStr = (CurTok == static_cast<int>(TokenType::tok_identifier))
                              ? m_lexer.IdentifierStr
                              : Lexer::tokenSpelling(CurTok);
    AnalysisType AType;

    if (typeStr == "tran")
        AType = AnalysisType::TRAN;
    else if (typeStr == "dc")
        AType = AnalysisType::DC;
    else if (typeStr == "ac")
        AType = AnalysisType::AC;
    else if (typeStr == "noise")
        AType = AnalysisType::NOISE;
    else if (typeStr == "op")
        AType = AnalysisType::OP;
    else if (typeStr == "tf")
        AType = AnalysisType::TF;
    else if (typeStr == "sens")
        AType = AnalysisType::SENS;
    else if (typeStr == "fourier")
        AType = AnalysisType::FOURIER;
    else {
        ReportError("expected analysis type, found: " + typeStr);
        return nullptr;
    }

    getNextToken(); // eat type

    auto Analysis = std::make_unique<AnalysisExprAST>(AType);

    // Parse parameters in braces
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_identifier) || CurTok < 0) {
                std::string ParamName = (CurTok == static_cast<int>(TokenType::tok_identifier))
                                            ? m_lexer.IdentifierStr
                                            : Lexer::tokenSpelling(CurTok);
                getNextToken();

                if (CurTok != '=') {
                    ReportError("expected '=' after parameter name");
                    return nullptr;
                }
                getNextToken(); // eat =

                auto ParamValue = ParseExpression();
                if (!ParamValue)
                    return nullptr;

                Analysis->addParameter(ParamName, std::move(ParamValue));
            }

            if (CurTok == ',')
                getNextToken();
        }

        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }

    return Analysis;
}
// Parse measure directive: measure name MAX { expression, params }
std::unique_ptr<MeasureExprAST> Parser::ParseMeasure()
{
    getNextToken(); // eat measure

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected measurement name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // CurTok now points to the measurement type (MAX, MIN, etc.)
    // Parse measurement type
    std::string typeStr = Lexer::tokenSpelling(CurTok);
    MeasureType MType;
    if (typeStr == "MAX")
        MType = MeasureType::MAX;
    else if (typeStr == "MIN")
        MType = MeasureType::MIN;
    else if (typeStr == "AVG")
        MType = MeasureType::AVG;
    else if (typeStr == "RMS")
        MType = MeasureType::RMS;
    else if (typeStr == "TRIG")
        MType = MeasureType::TRIG;
    else if (typeStr == "TARG")
        MType = MeasureType::TARG;
    else if (typeStr == "WHEN")
        MType = MeasureType::WHEN;
    else if (typeStr == "FIND")
        MType = MeasureType::FIND;
    else if (typeStr == "DERIV")
        MType = MeasureType::DERIV;
    else if (typeStr == "INTEG")
        MType = MeasureType::INTEG;
    else {
        ReportError("expected measurement type, found: " + typeStr);
        return nullptr;
    }

    getNextToken(); // eat type

    // Parse expression
    auto Expr = ParseExpression();
    if (!Expr) {
        ReportError("expected expression for measure");
        return nullptr;
    }

    auto Measure = std::make_unique<MeasureExprAST>(Name, MType, std::move(Expr));

    // Parse optional parameters
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string ParamName = m_lexer.IdentifierStr;
                getNextToken();

                if (CurTok != '=') {
                    ReportError("expected '=' after parameter name");
                    return nullptr;
                }
                getNextToken(); // eat =

                auto ParamValue = ParseExpression();
                if (!ParamValue)
                    return nullptr;

                Measure->addParameter(ParamName, std::move(ParamValue));
            }

            if (CurTok == ',')
                getNextToken();
        }

        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }

    return Measure;
}

// Parse probe directive: probe varname [as "outputname"]
std::unique_ptr<ExprAST> Parser::ParseProbe()
{
    getNextToken(); // eat probe

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected variable name after probe");
        return nullptr;
    }
    std::string VarName = m_lexer.IdentifierStr;
    getNextToken();

    std::string OutputName;
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "as") {
        getNextToken(); // eat as
        if (CurTok != static_cast<int>(TokenType::tok_string)) {
            ReportError("expected string after 'as'");
            return nullptr;
        }
        OutputName = m_lexer.StringVal;
        getNextToken();
    }

    return std::make_unique<ProbeExprAST>(VarName, OutputName);
}

// Parse save directive: save varname
std::unique_ptr<ExprAST> Parser::ParseSave()
{
    getNextToken(); // eat save

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected variable name after save");
        return nullptr;
    }
    std::string VarName = m_lexer.IdentifierStr;
    getNextToken();

    return std::make_unique<SaveExprAST>(VarName);
}

// Parse subckt definition: subckt name (pin1, pin2, ...) { params... body... }
std::unique_ptr<SubcktAST> Parser::ParseSubckt()
{
    getNextToken(); // eat subckt

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected subcircuit name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    // Parse pin list
    std::vector<std::string> Pins;
    if (CurTok == '(') {
        getNextToken(); // eat (

        while (CurTok != ')' && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok != static_cast<int>(TokenType::tok_identifier) && CurTok >= 0) {
                ReportError("expected pin name");
                return nullptr;
            }
            Pins.push_back(m_lexer.IdentifierStr);
            getNextToken();
            if (CurTok == ',')
                getNextToken();
        }

        if (CurTok == ')')
            getNextToken(); // eat )
    }

    auto Subckt = std::make_unique<SubcktExprAST>(Name, std::move(Pins));

    // Parse body
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_def)) {
                if (auto Func = ParseDefinition()) {
                    Subckt->addFunction(std::move(Func));
                } else {
                    return nullptr;
                }
            } else if (CurTok == static_cast<int>(TokenType::tok_param) ||
                       CurTok == static_cast<int>(TokenType::tok_params)) {
                if (auto Param = ParseParam()) {
                    Subckt->addStatement(std::move(Param));
                } else {
                    return nullptr;
                }
            } else if (auto Stmt = ParseExpression()) {
                Subckt->addStatement(std::move(Stmt));
            } else {
                ReportError("invalid statement in subcircuit");
                return nullptr;
            }
        }

        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }

    return Subckt;
}

// Parse model declaration: model name type { params }
std::unique_ptr<ModelAST> Parser::ParseModel()
{
    getNextToken(); // eat model

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected model name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected model type (D, NPN, PNP, NMOS, PMOS, R, L, C, etc.)");
        return nullptr;
    }
    std::string ModelType = m_lexer.IdentifierStr;
    getNextToken();

    auto Model = std::make_unique<ModelExprAST>(Name, ModelType);

    // Parse parameters
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {

        while (CurTok != static_cast<int>(TokenType::tok_rbrace) && CurTok != static_cast<int>(TokenType::tok_eof)) {
            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string ParamName = m_lexer.IdentifierStr;
                getNextToken();

                if (CurTok != '=') {
                    ReportError("expected '=' after parameter name");
                    return nullptr;
                }
                getNextToken(); // eat =

                auto ParamValue = ParseExpression();
                if (!ParamValue)
                    return nullptr;

                Model->addParameter(ParamName, std::move(ParamValue));
            }

            if (CurTok == ',')
                getNextToken();
        }

        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }

    return Model;
}

// Parse param declaration: param name = value
std::unique_ptr<ExprAST> Parser::ParseParam()
{
    getNextToken(); // eat param

    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected parameter name");
        return nullptr;
    }
    std::string Name = m_lexer.IdentifierStr;
    getNextToken();

    std::unique_ptr<ExprAST> Value = std::make_unique<NumberExprAST>(0.0);

    if (CurTok == '=') {
        getNextToken(); // eat =
        Value = ParseExpression();
        if (!Value)
            return nullptr;
    }

    return std::make_unique<ParamExprAST>(Name, std::move(Value));
}

// Parse initial condition: ic V(node) = value
std::unique_ptr<ExprAST> Parser::ParseIC()
{
    getNextToken(); // eat ic

    // Parse V(node)
    if (CurTok != static_cast<int>(TokenType::tok_identifier) || m_lexer.IdentifierStr != "V") {
        ReportError("expected V(node) for initial condition");
        return nullptr;
    }
    getNextToken(); // eat V

    if (CurTok != '(') {
        ReportError("expected '(' after V");
        return nullptr;
    }
    getNextToken(); // eat (

    if (CurTok != static_cast<int>(TokenType::tok_identifier) && CurTok != static_cast<int>(TokenType::tok_number)) {
        ReportError("expected node name");
        return nullptr;
    }
    std::string NodeName = (CurTok == static_cast<int>(TokenType::tok_identifier))
                               ? m_lexer.IdentifierStr
                               : std::to_string(static_cast<int>(m_lexer.NumVal));
    getNextToken();

    if (CurTok != ')') {
        ReportError("expected ')' after node name");
        return nullptr;
    }
    getNextToken(); // eat )

    if (CurTok != '=') {
        ReportError("expected '=' after node specification");
        return nullptr;
    }
    getNextToken(); // eat =

    auto Value = ParseExpression();
    if (!Value)
        return nullptr;

    return std::make_unique<ICExprAST>(NodeName, std::move(Value));
}

/* ========================================================================
 * Section 7.2: Mixed-Signal & Modeling Extensions - Parser
 * ======================================================================== */

// Parse above(expr, threshold)

} // namespace Flux
