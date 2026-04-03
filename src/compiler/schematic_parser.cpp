#include "flux/compiler/parser.h"
#include "flux/compiler/schematic_ast.h"
#include <iostream>

namespace Flux {

// ============ Schematic Parser ============

std::unique_ptr<ExprAST> Parser::ParseSchematicExpr() {
    getNextToken(); // eat schematic
    
    // Get schematic name
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected schematic name after schematic");
        return nullptr;
    }
    
    std::string name = m_lexer.IdentifierStr;
    getNextToken();
    
    auto schematic = std::make_unique<SchematicExprAST>(name);
    
    // Parse body until end
    while (CurTok != static_cast<int>(TokenType::tok_eof)) {
        if (CurTok == static_cast<int>(TokenType::tok_component)) {
            // Parse component: component R1 = 1k type=R footprint=0402
            getNextToken(); // eat component
            
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                ReportError("expected component name");
                return nullptr;
            }
            std::string compName = m_lexer.IdentifierStr;
            getNextToken();
            
            std::string value = "";
            std::string type = "";
            std::string footprint = "";
            
            // Parse component value
            if (CurTok == '=') {
                getNextToken(); // eat =
                if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    value = m_lexer.IdentifierStr;
                    getNextToken();
                } else if (CurTok == static_cast<int>(TokenType::tok_string)) {
                    value = m_lexer.StringVal;
                    getNextToken();
                }
            }
            
            // Parse optional attributes
            while (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string attr = m_lexer.IdentifierStr;
                getNextToken();
                
                if (CurTok == '=') {
                    getNextToken(); // eat =
                    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                        std::string attrVal = m_lexer.IdentifierStr;
                        if (attr == "type") type = attrVal;
                        else if (attr == "footprint") footprint = attrVal;
                        getNextToken();
                    }
                }
            }
            
            // Infer type from component name if not specified
            if (type.empty()) {
                if (!compName.empty()) {
                    char firstChar = compName[0];
                    switch (firstChar) {
                        case 'R': type = "R"; break;
                        case 'C': type = "C"; break;
                        case 'L': type = "L"; break;
                        case 'D': type = "D"; break;
                        case 'Q': type = "Q"; break;
                        case 'U': type = "U"; break;
                        default: type = "U"; break;
                    }
                }
            }
            
            SchematicComponent comp(compName, type, value);
            comp.Footprint = footprint;
            schematic->addComponent(comp);
            
        } else if (CurTok == static_cast<int>(TokenType::tok_connect)) {
            // Parse connection: connect R1.1 to C1.1 net=signal
            getNextToken(); // eat connect
            
            // Parse from component.pin
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                ReportError("expected component name in connect");
                return nullptr;
            }
            std::string fromComp = m_lexer.IdentifierStr;
            getNextToken();
            
            std::string fromPin = "1";
            if (CurTok == '.') {
                getNextToken(); // eat .
                if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    fromPin = m_lexer.IdentifierStr;
                    getNextToken();
                }
            }
            
            // Parse 'to'
            if (CurTok != static_cast<int>(TokenType::tok_identifier) || m_lexer.IdentifierStr != "to") {
                ReportError("expected 'to' in connect");
                return nullptr;
            }
            getNextToken(); // eat to
            
            // Parse to component.pin
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                ReportError("expected component name after 'to'");
                return nullptr;
            }
            std::string toComp = m_lexer.IdentifierStr;
            getNextToken();
            
            std::string toPin = "1";
            if (CurTok == '.') {
                getNextToken(); // eat .
                if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    toPin = m_lexer.IdentifierStr;
                    getNextToken();
                }
            }
            
            // Parse optional net name
            std::string netName = "";
            if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "net") {
                getNextToken(); // eat net
                if (CurTok == '=') {
                    getNextToken(); // eat =
                    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                        netName = m_lexer.IdentifierStr;
                        getNextToken();
                    }
                }
            }
            
            SchematicConnection conn(fromComp, fromPin, toComp, toPin);
            conn.NetName = netName;
            schematic->addConnection(conn);
            
        } else if (CurTok == static_cast<int>(TokenType::tok_port)) {
            // Parse port: port Vin type=input net=input_signal
            getNextToken(); // eat port
            
            if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
                ReportError("expected port name");
                return nullptr;
            }
            std::string portName = m_lexer.IdentifierStr;
            getNextToken();
            
            std::string portType = "bidirectional";
            std::string portNet = "";
            
            while (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                std::string attr = m_lexer.IdentifierStr;
                getNextToken();
                
                if (CurTok == '=') {
                    getNextToken(); // eat =
                    if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                        std::string attrVal = m_lexer.IdentifierStr;
                        if (attr == "type") portType = attrVal;
                        else if (attr == "net") portNet = attrVal;
                        getNextToken();
                    }
                }
            }
            
            schematic->addPort(SchematicPort(portName, portType));
            
        } else {
            // End of schematic block
            break;
        }
    }
    
    std::cout << "[Schematic] Parsed '" << name << "' with " 
              << schematic->getComponents().size() << " components and "
              << schematic->getConnections().size() << " connections" << std::endl;
    
    return schematic;
}

// ============ Export Schematic Parser ============

std::unique_ptr<ExprAST> Parser::ParseExportSchematic() {
    getNextToken(); // eat export_schematic
    
    // Parse schematic name
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected schematic name after export_schematic");
        return nullptr;
    }
    std::string name = m_lexer.IdentifierStr;
    getNextToken();
    
    // Parse output file
    std::string outputFile = "output.kicad_sch";
    std::string format = "kicad";
    
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "to") {
        getNextToken(); // eat to
        if (CurTok == static_cast<int>(TokenType::tok_string)) {
            outputFile = m_lexer.StringVal;
            getNextToken();
        }
    }
    
    // Parse format
    if (CurTok == static_cast<int>(TokenType::tok_identifier) && m_lexer.IdentifierStr == "format") {
        getNextToken(); // eat format
        if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
            format = m_lexer.IdentifierStr;
            getNextToken();
        }
    }
    
    return std::make_unique<ExportSchematicExprAST>(name, outputFile, format);
}

} // namespace Flux
