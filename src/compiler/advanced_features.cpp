#include "flux/compiler/advanced_features.h"
#include "flux/compiler/parser.h"
#include <sstream>

namespace Flux {

// ============ Parser Implementations ============

std::unique_ptr<ExprAST> Parser::ParsePlotDecl() {
    getNextToken(); // eat plot
    
    std::vector<std::string> signals;
    
    // Parse signal names
    while (CurTok != static_cast<int>(TokenType::tok_eof) &&
           CurTok != static_cast<int>(TokenType::tok_semicolon) &&
           CurTok != static_cast<int>(TokenType::tok_lbrace)) {
        
        if (CurTok == ',') {
            getNextToken();
            continue;
        }
        
        if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
            signals.push_back(m_lexer.IdentifierStr);
            getNextToken();
        } else {
            getNextToken();
        }
    }
    
    auto plotDecl = std::make_unique<PlotDeclAST>(std::move(signals));
    
    // Parse optional configuration block
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {
        
        while (CurTok != static_cast<int>(TokenType::tok_eof) &&
               CurTok != static_cast<int>(TokenType::tok_rbrace)) {
            
            if (CurTok == static_cast<int>(TokenType::tok_title)) {
                getNextToken(); // eat title
                if (CurTok == static_cast<int>(TokenType::tok_string)) {
                    plotDecl->setTitle(m_lexer.StringVal);
                    getNextToken();
                }
            } else if (CurTok == static_cast<int>(TokenType::tok_color)) {
                getNextToken(); // eat color
                std::vector<std::string> colors;
                while (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    colors.push_back(m_lexer.IdentifierStr);
                    getNextToken();
                    if (CurTok == ',') getNextToken();
                }
                plotDecl->setColors(std::move(colors));
            } else if (CurTok == static_cast<int>(TokenType::tok_grid)) {
                getNextToken(); // eat grid
                bool enabled = (CurTok == static_cast<int>(TokenType::tok_identifier) &&
                               m_lexer.IdentifierStr == "on");
                plotDecl->setGrid(enabled);
                if (enabled) getNextToken();
            } else if (CurTok == static_cast<int>(TokenType::tok_autoscale)) {
                getNextToken(); // eat autoscale
                plotDecl->setAutoScale(true);
            } else {
                getNextToken();
            }
            
            if (CurTok == static_cast<int>(TokenType::tok_semicolon)) {
                getNextToken();
            }
        }
        
        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }
    
    return plotDecl;
}

std::unique_ptr<ExprAST> Parser::ParseBenchmarkDecl() {
    getNextToken(); // eat benchmark
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected circuit name after benchmark");
        return nullptr;
    }
    
    std::string circuit = m_lexer.IdentifierStr;
    getNextToken(); // eat circuit name
    
    auto benchmarkDecl = std::make_unique<BenchmarkDeclAST>(circuit);
    
    // Parse configuration block
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {
        
        while (CurTok != static_cast<int>(TokenType::tok_eof) &&
               CurTok != static_cast<int>(TokenType::tok_rbrace)) {
            
            if (CurTok == static_cast<int>(TokenType::tok_compare)) {
                getNextToken(); // eat compare
                std::vector<std::string> comparators;
                while (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    comparators.push_back(m_lexer.IdentifierStr);
                    getNextToken();
                    if (CurTok == ',') getNextToken();
                }
                benchmarkDecl->setComparators(std::move(comparators));
            } else if (CurTok == static_cast<int>(TokenType::tok_metric)) {
                getNextToken(); // eat metric
                std::vector<std::string> metrics;
                while (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    metrics.push_back(m_lexer.IdentifierStr);
                    getNextToken();
                    if (CurTok == ',') getNextToken();
                }
                benchmarkDecl->setMetrics(std::move(metrics));
            } else {
                getNextToken();
            }
            
            if (CurTok == static_cast<int>(TokenType::tok_semicolon)) {
                getNextToken();
            }
        }
        
        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }
    
    return benchmarkDecl;
}

std::unique_ptr<ExprAST> Parser::ParseOptimizeDecl() {
    getNextToken(); // eat optimize
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected circuit name after optimize");
        return nullptr;
    }
    
    std::string circuit = m_lexer.IdentifierStr;
    getNextToken(); // eat circuit name
    
    auto optimizeDecl = std::make_unique<OptimizeDeclAST>(circuit);
    
    // Parse configuration block
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {
        
        while (CurTok != static_cast<int>(TokenType::tok_eof) &&
               CurTok != static_cast<int>(TokenType::tok_rbrace)) {
            
            if (CurTok == static_cast<int>(TokenType::tok_goals)) {
                getNextToken(); // eat goals
                std::map<std::string, std::string> goals;
                while (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    std::string name = m_lexer.IdentifierStr;
                    getNextToken();
                    if (CurTok == '=') {
                        getNextToken();
                        if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                            goals[name] = m_lexer.IdentifierStr;
                            getNextToken();
                        }
                    }
                    if (CurTok == ',') getNextToken();
                }
                optimizeDecl->setGoals(std::move(goals));
            } else if (CurTok == static_cast<int>(TokenType::tok_tune)) {
                getNextToken(); // eat tune
                std::vector<std::string> params;
                while (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    params.push_back(m_lexer.IdentifierStr);
                    getNextToken();
                    if (CurTok == ',') getNextToken();
                }
                optimizeDecl->setTuneParams(std::move(params));
            } else if (CurTok == static_cast<int>(TokenType::tok_algorithm)) {
                getNextToken(); // eat algorithm
                if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    optimizeDecl->setAlgorithm(m_lexer.IdentifierStr);
                    getNextToken();
                }
            } else if (CurTok == static_cast<int>(TokenType::tok_runs)) {
                getNextToken(); // eat runs
                if (CurTok == static_cast<int>(TokenType::tok_number)) {
                    optimizeDecl->setMaxIterations(static_cast<int>(m_lexer.NumVal));
                    getNextToken();
                }
            } else if (CurTok == static_cast<int>(TokenType::tok_gpu)) {
                getNextToken(); // eat gpu
                optimizeDecl->setGPUAccelerated(true);
            } else {
                getNextToken();
            }
            
            if (CurTok == static_cast<int>(TokenType::tok_semicolon)) {
                getNextToken();
            }
        }
        
        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }
    
    return optimizeDecl;
}

std::unique_ptr<ExprAST> Parser::ParseSweepDecl() {
    getNextToken(); // eat sweep
    
    if (CurTok != static_cast<int>(TokenType::tok_identifier)) {
        ReportError("expected signal name after sweep");
        return nullptr;
    }
    
    std::string signal = m_lexer.IdentifierStr;
    getNextToken(); // eat signal name
    
    auto sweepDecl = std::make_unique<SweepDeclAST>(signal);
    
    // Parse configuration block
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {
        
        while (CurTok != static_cast<int>(TokenType::tok_eof) &&
               CurTok != static_cast<int>(TokenType::tok_rbrace)) {
            
            if (CurTok == static_cast<int>(TokenType::tok_controls)) {
                getNextToken(); // eat controls
                if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
                    getNextToken(); // eat {
                    
                    while (CurTok != static_cast<int>(TokenType::tok_rbrace)) {
                        if (CurTok == static_cast<int>(TokenType::tok_slider)) {
                            getNextToken(); // eat slider
                            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                                std::string name = m_lexer.IdentifierStr;
                                getNextToken();
                                
                                std::map<std::string, std::string> params;
                                params["type"] = "slider";
                                
                                // Parse slider params: name min max [step]
                                if (CurTok == static_cast<int>(TokenType::tok_number)) {
                                    params["min"] = std::to_string(m_lexer.NumVal);
                                    getNextToken();
                                }
                                if (CurTok == static_cast<int>(TokenType::tok_number)) {
                                    params["max"] = std::to_string(m_lexer.NumVal);
                                    getNextToken();
                                }
                                if (CurTok == static_cast<int>(TokenType::tok_number)) {
                                    params["step"] = std::to_string(m_lexer.NumVal);
                                    getNextToken();
                                }
                                
                                sweepDecl->addControl(name, "slider", params);
                            }
                        } else if (CurTok == static_cast<int>(TokenType::tok_knob)) {
                            getNextToken(); // eat knob
                            if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                                std::string name = m_lexer.IdentifierStr;
                                getNextToken();
                                
                                std::map<std::string, std::string> params;
                                params["type"] = "knob";
                                
                                // Parse knob params: name min max
                                if (CurTok == static_cast<int>(TokenType::tok_number)) {
                                    params["min"] = std::to_string(m_lexer.NumVal);
                                    getNextToken();
                                }
                                if (CurTok == static_cast<int>(TokenType::tok_number)) {
                                    params["max"] = std::to_string(m_lexer.NumVal);
                                    getNextToken();
                                }
                                
                                sweepDecl->addControl(name, "knob", params);
                            }
                        } else {
                            getNextToken();
                        }
                        
                        if (CurTok == ',') getNextToken();
                    }
                    
                    if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
                        getNextToken(); // eat }
                    }
                }
            } else {
                getNextToken();
            }
            
            if (CurTok == static_cast<int>(TokenType::tok_semicolon)) {
                getNextToken();
            }
        }
        
        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }
    
    return sweepDecl;
}

std::unique_ptr<ExprAST> Parser::ParseReportDecl() {
    getNextToken(); // eat generate_report
    
    if (CurTok != static_cast<int>(TokenType::tok_string)) {
        ReportError("expected filename after generate_report");
        return nullptr;
    }
    
    std::string filename = m_lexer.StringVal;
    getNextToken(); // eat filename
    
    auto reportDecl = std::make_unique<ReportDeclAST>(filename);
    
    // Parse configuration block
    if (CurTok == static_cast<int>(TokenType::tok_lbrace)) {
        getNextToken(); // eat {
        
        while (CurTok != static_cast<int>(TokenType::tok_eof) &&
               CurTok != static_cast<int>(TokenType::tok_rbrace)) {
            
            if (CurTok == static_cast<int>(TokenType::tok_sections)) {
                getNextToken(); // eat sections
                std::vector<std::string> sections;
                while (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    sections.push_back(m_lexer.IdentifierStr);
                    getNextToken();
                    if (CurTok == ',') getNextToken();
                }
                reportDecl->setSections(std::move(sections));
            } else if (CurTok == static_cast<int>(TokenType::tok_include)) {
                getNextToken(); // eat include
                if (CurTok == static_cast<int>(TokenType::tok_identifier)) {
                    std::string item = m_lexer.IdentifierStr;
                    if (item == "png" || item == "PNG") {
                        reportDecl->setIncludePNG(true);
                    } else if (item == "csv" || item == "CSV") {
                        reportDecl->setIncludeCSV(true);
                    }
                    getNextToken();
                }
            } else {
                getNextToken();
            }
            
            if (CurTok == static_cast<int>(TokenType::tok_semicolon)) {
                getNextToken();
            }
        }
        
        if (CurTok == static_cast<int>(TokenType::tok_rbrace)) {
            getNextToken(); // eat }
        }
    }
    
    return reportDecl;
}

// ============ Codegen Implementations ============

TypedValue PlotDeclAST::codegen(CodegenContext& context) {
    // Generate code to plot waveforms
    // This would integrate with VioSpice's plotting system
    
    llvm::Function* PlotF = context.TheModule->getFunction("flux_plot_waveforms");
    if (!PlotF) {
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        PlotF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext), {CharPtrTy}, true),
            llvm::Function::ExternalLinkage,
            "flux_plot_waveforms",
            context.TheModule.get());
    }
    
    // For now, just print the plot configuration
    std::ostringstream oss;
    oss << "PLOT: ";
    for (size_t i = 0; i < Signals.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << Signals[i];
    }
    if (!Title.empty()) oss << " title=\"" << Title << "\"";
    if (GridEnabled) oss << " grid=on";
    if (AutoScale) oss << " autoscale";
    
    llvm::Value* MsgPtr = context.Builder.CreateGlobalStringPtr(oss.str(), "plot_msg");
    llvm::Function* PrintF = context.TheModule->getFunction("println_string");
    if (PrintF) {
        context.Builder.CreateCall(PrintF, {MsgPtr});
    }
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

TypedValue BenchmarkDeclAST::codegen(CodegenContext& context) {
    // Generate benchmark code
    llvm::Function* BenchmarkF = context.TheModule->getFunction("flux_run_benchmark");
    if (!BenchmarkF) {
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        BenchmarkF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext), {CharPtrTy}, true),
            llvm::Function::ExternalLinkage,
            "flux_run_benchmark",
            context.TheModule.get());
    }
    
    std::ostringstream oss;
    oss << "BENCHMARK: " << Circuit;
    if (!Comparators.empty()) {
        oss << " vs ";
        for (size_t i = 0; i < Comparators.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << Comparators[i];
        }
    }
    
    llvm::Value* MsgPtr = context.Builder.CreateGlobalStringPtr(oss.str(), "benchmark_msg");
    llvm::Function* PrintF = context.TheModule->getFunction("println_string");
    if (PrintF) {
        context.Builder.CreateCall(PrintF, {MsgPtr});
    }
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

TypedValue OptimizeDeclAST::codegen(CodegenContext& context) {
    // Generate optimization code
    llvm::Function* OptimizeF = context.TheModule->getFunction("flux_run_optimization");
    if (!OptimizeF) {
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        OptimizeF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext), {CharPtrTy}, true),
            llvm::Function::ExternalLinkage,
            "flux_run_optimization",
            context.TheModule.get());
    }
    
    std::ostringstream oss;
    oss << "OPTIMIZE: " << Circuit;
    if (!Goals.empty()) {
        oss << " goals=[";
        bool first = true;
        for (const auto& [name, target] : Goals) {
            if (!first) oss << ", ";
            oss << name << "=" << target;
            first = false;
        }
        oss << "]";
    }
    if (!TuneParams.empty()) {
        oss << " tune=[";
        for (size_t i = 0; i < TuneParams.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << TuneParams[i];
        }
        oss << "]";
    }
    if (!Algorithm.empty()) oss << " algorithm=" << Algorithm;
    if (MaxIterations > 0) oss << " max_iter=" << MaxIterations;
    if (GPUAccelerated) oss << " GPU=on";
    
    llvm::Value* MsgPtr = context.Builder.CreateGlobalStringPtr(oss.str(), "optimize_msg");
    llvm::Function* PrintF = context.TheModule->getFunction("println_string");
    if (PrintF) {
        context.Builder.CreateCall(PrintF, {MsgPtr});
    }
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

TypedValue SweepDeclAST::codegen(CodegenContext& context) {
    // Generate interactive sweep code
    llvm::Function* SweepF = context.TheModule->getFunction("flux_run_sweep");
    if (!SweepF) {
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        SweepF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext), {CharPtrTy}, true),
            llvm::Function::ExternalLinkage,
            "flux_run_sweep",
            context.TheModule.get());
    }
    
    std::ostringstream oss;
    oss << "SWEEP: " << Signal;
    if (!Controls.empty()) {
        oss << " controls=[";
        bool first = true;
        for (const auto& [name, params] : Controls) {
            if (!first) oss << ", ";
            oss << name << "(";
            bool firstParam = true;
            for (const auto& [key, val] : params) {
                if (!firstParam) oss << ", ";
                oss << key << "=" << val;
                firstParam = false;
            }
            oss << ")";
            first = false;
        }
        oss << "]";
    }
    
    llvm::Value* MsgPtr = context.Builder.CreateGlobalStringPtr(oss.str(), "sweep_msg");
    llvm::Function* PrintF = context.TheModule->getFunction("println_string");
    if (PrintF) {
        context.Builder.CreateCall(PrintF, {MsgPtr});
    }
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

TypedValue ReportDeclAST::codegen(CodegenContext& context) {
    // Generate report generation code
    llvm::Function* ReportF = context.TheModule->getFunction("flux_generate_report");
    if (!ReportF) {
        llvm::Type* CharPtrTy = llvm::PointerType::get(llvm::Type::getInt8Ty(context.TheContext), 0);
        ReportF = llvm::Function::Create(
            llvm::FunctionType::get(llvm::Type::getVoidTy(context.TheContext), {CharPtrTy}, true),
            llvm::Function::ExternalLinkage,
            "flux_generate_report",
            context.TheModule.get());
    }
    
    std::ostringstream oss;
    oss << "REPORT: " << Filename;
    if (!Sections.empty()) {
        oss << " sections=[";
        for (size_t i = 0; i < Sections.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << Sections[i];
        }
        oss << "]";
    }
    if (IncludePNG) oss << " png=on";
    if (IncludeCSV) oss << " csv=on";
    
    llvm::Value* MsgPtr = context.Builder.CreateGlobalStringPtr(oss.str(), "report_msg");
    llvm::Function* PrintF = context.TheModule->getFunction("println_string");
    if (PrintF) {
        context.Builder.CreateCall(PrintF, {MsgPtr});
    }
    
    return TypedValue(llvm::ConstantFP::get(context.TheContext, llvm::APFloat(0.0)), TypeKind::Double);
}

void SweepDeclAST::addControl(const std::string& name, const std::string& type,
                              const std::map<std::string, std::string>& params) {
    Controls[name] = params;
}

} // namespace Flux
