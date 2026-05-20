/* Copyright 2026 Janada Sroor
 SPDX-License-Identifier: Apache-2.0
 http://www.apache.org/licenses/LICENSE-2.0
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
     http://www.apache.org/licenses/LICENSE-2.0
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License. */

#include "flux/compiler/lexer.h"
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/SMLoc.h>
#include <llvm/Support/SourceMgr.h>
#include <cctype>
#include <cstdlib>

namespace Flux {

Lexer::Lexer(const std::string& input) 
    : m_input(input),
      m_pos(0),
      m_tokenStart(0),
      m_lastChar(' '),
      m_line(1),
      m_column(1),
      m_currentTokenLine(1),
      m_currentTokenColumn(1),
      m_lineStart(0),
      m_currentTokenOffset(0),
      m_currentTokenLength(0),
      m_buffer(llvm::MemoryBuffer::getMemBufferCopy(input, "<flux>")),
      m_sourceMgr(std::make_unique<llvm::SourceMgr>()) {
    m_sourceMgr->AddNewSourceBuffer(std::move(m_buffer), llvm::SMLoc());
}

int Lexer::advance() {
    if (m_pos < m_input.size()) {
        m_lastChar = m_input[m_pos++];
        if (m_lastChar == '\n') {
            m_line++;
            m_column = 1;
            m_lineStart = m_pos;
        } else {
            m_column++;
        }
    } else {
        m_lastChar = EOF;
    }
    return m_lastChar;
}

std::string Lexer::getCurrentLineText() const {
    if (m_lineStart >= m_input.size()) return "";
    size_t end = m_input.find('\n', m_lineStart);
    if (end == std::string::npos) end = m_input.size();
    return m_input.substr(m_lineStart, end - m_lineStart);
}

std::string Lexer::tokenSpelling(int token) {
    if (token >= 0 && token < 128)
        return std::string(1, static_cast<char>(token));

    switch (static_cast<TokenType>(token)) {
        case TokenType::tok_eof: return "eof";
        case TokenType::tok_def: return "def";
        case TokenType::tok_extern: return "extern";
        case TokenType::tok_return: return "return";
        case TokenType::tok_var: return "var";
        case TokenType::tok_if: return "if";
        case TokenType::tok_then: return "then";
        case TokenType::tok_else: return "else";
        case TokenType::tok_for: return "for";
        case TokenType::tok_in: return "in";
        case TokenType::tok_do: return "do";
        case TokenType::tok_while: return "while";
        case TokenType::tok_let: return "let";
        case TokenType::tok_fn: return "fn";
        case TokenType::tok_import: return "import";
        case TokenType::tok_type_float: return "float";
        case TokenType::tok_type_double: return "double";
        case TokenType::tok_type_int: return "int";
        case TokenType::tok_type_void: return "void";
        case TokenType::tok_type_complex: return "complex";
        case TokenType::tok_type_string: return "string";
        case TokenType::tok_type_vector: return "vector";
        case TokenType::tok_type_matrix: return "matrix";
        case TokenType::tok_type_bool: return "bool";
        case TokenType::tok_true: return "true";
        case TokenType::tok_false: return "false";
        case TokenType::tok_tran: return "tran";
        case TokenType::tok_dc: return "dc";
        case TokenType::tok_ac: return "ac";
        case TokenType::tok_noise: return "noise";
        case TokenType::tok_op: return "op";
        case TokenType::tok_tf: return "tf";
        case TokenType::tok_sens: return "sens";
        case TokenType::tok_fourier: return "fourier";
        case TokenType::tok_max: return "MAX";
        case TokenType::tok_min: return "MIN";
        case TokenType::tok_avg: return "AVG";
        case TokenType::tok_rms: return "RMS";
        case TokenType::tok_trig: return "TRIG";
        case TokenType::tok_targ: return "TARG";
        case TokenType::tok_meas_when: return "WHEN";
        case TokenType::tok_find: return "FIND";
        case TokenType::tok_deriv: return "DERIV";
        case TokenType::tok_integ: return "INTEG";

        // VioMATRIXC A-Device tokens
        case TokenType::tok_adevice: return "adevice";
        case TokenType::tok_buf: return "BUF";
        case TokenType::tok_not_gate: return "NOT";
        case TokenType::tok_and_gate: return "AND";
        case TokenType::tok_or_gate: return "OR";
        case TokenType::tok_nand_gate: return "NAND";
        case TokenType::tok_nor_gate: return "NOR";
        case TokenType::tok_xor_gate: return "XOR";
        case TokenType::tok_xnor_gate: return "XNOR";
        case TokenType::tok_schmitt: return "SCHMITT";
        case TokenType::tok_dff: return "DFF";
        case TokenType::tok_jkff: return "JKFF";
        case TokenType::tok_srff: return "SRFF";
        case TokenType::tok_dlatch: return "DLATCH";
        case TokenType::tok_counter: return "COUNTER";

        // VioMATRIXC WAVEFILE tokens
        case TokenType::tok_wavefile: return "WAVEFILE";
        case TokenType::tok_chan: return "CHAN";

        // Integer literal
        case TokenType::tok_integer: return "integer";

        default: return "token(" + std::to_string(token) + ")";
    }
}

std::string Lexer::getCurrentTokenText() const {
    if (m_currentTokenOffset >= m_input.size())
        return {};

    const size_t safeLength = std::min(m_currentTokenLength, m_input.size() - m_currentTokenOffset);
    return m_input.substr(m_currentTokenOffset, safeLength);
}

llvm::SMLoc Lexer::getCurrentTokenLoc() const {
    if (!m_sourceMgr || m_sourceMgr->getNumBuffers() == 0)
        return llvm::SMLoc();

    const auto *buffer = m_sourceMgr->getMemoryBuffer(m_sourceMgr->getMainFileID());
    const size_t safeOffset = std::min(m_tokenStart, buffer->getBufferSize());
    return llvm::SMLoc::getFromPointer(buffer->getBufferStart() + safeOffset);
}

void Lexer::reportError(const std::string& message) {
    m_errors.push_back({m_currentTokenLine, m_currentTokenColumn, message});
    if (m_sourceMgr && m_sourceMgr->getNumBuffers() > 0) {
        m_sourceMgr->PrintMessage(getCurrentTokenLoc(), llvm::SourceMgr::DK_Error, message);
    }
}

void Lexer::clearErrors() {
    m_errors.clear();
}

const std::vector<LexerDiagnostic>& Lexer::getErrors() const {
    return m_errors;
}

bool Lexer::hasErrors() const {
    return !m_errors.empty();
}

int Lexer::gettok() {
    m_tokenStart = m_pos > 0 ? m_pos - 1 : 0;

    // Skip whitespace
    while (isspace(m_lastChar)) {
        advance();
    }
    m_tokenStart = m_pos > 0 ? m_pos - 1 : 0;

    if (isalpha(m_lastChar)) { // identifier: [a-zA-Z][a-zA-Z0-9_]*
        IdentifierStr = m_lastChar;
        while (isalnum(advance()) || m_lastChar == '_') {
            IdentifierStr += m_lastChar;
        }

        if (IdentifierStr == "def") return static_cast<int>(TokenType::tok_def);
        if (IdentifierStr == "extern") return static_cast<int>(TokenType::tok_extern);
        if (IdentifierStr == "return") return static_cast<int>(TokenType::tok_return);
        if (IdentifierStr == "var") return static_cast<int>(TokenType::tok_var);
        if (IdentifierStr == "let") return static_cast<int>(TokenType::tok_let);
        if (IdentifierStr == "fn") return static_cast<int>(TokenType::tok_fn);
        if (IdentifierStr == "if") return static_cast<int>(TokenType::tok_if);
        if (IdentifierStr == "fixed") return static_cast<int>(TokenType::tok_fixed);
        if (IdentifierStr == "surrogate") return static_cast<int>(TokenType::tok_surrogate);
        if (IdentifierStr == "train") return static_cast<int>(TokenType::tok_train);
        if (IdentifierStr == "predict") return static_cast<int>(TokenType::tok_predict);
        if (IdentifierStr == "goal") return static_cast<int>(TokenType::tok_goal);
        if (IdentifierStr == "thermal") return static_cast<int>(TokenType::tok_thermal);
        if (IdentifierStr == "virtual_probe") return static_cast<int>(TokenType::tok_virtual_probe);
        if (IdentifierStr == "hot_swap") return static_cast<int>(TokenType::tok_hot_swap);
        if (IdentifierStr == "then") return static_cast<int>(TokenType::tok_then);
        if (IdentifierStr == "else") return static_cast<int>(TokenType::tok_else);
        if (IdentifierStr == "for") return static_cast<int>(TokenType::tok_for);
        if (IdentifierStr == "in") return static_cast<int>(TokenType::tok_in);
        if (IdentifierStr == "do") return static_cast<int>(TokenType::tok_do);
        if (IdentifierStr == "while") return static_cast<int>(TokenType::tok_while);
        if (IdentifierStr == "yield") return static_cast<int>(TokenType::tok_yield);
        if (IdentifierStr == "import") return static_cast<int>(TokenType::tok_import);
        if (IdentifierStr == "analysis") return static_cast<int>(TokenType::tok_analysis);
        if (IdentifierStr == "measure") return static_cast<int>(TokenType::tok_measure);
        if (IdentifierStr == "subckt") return static_cast<int>(TokenType::tok_subckt);
        if (IdentifierStr == "model") return static_cast<int>(TokenType::tok_model);
        if (IdentifierStr == "tran") return static_cast<int>(TokenType::tok_tran);
        if (IdentifierStr == "dc") return static_cast<int>(TokenType::tok_dc);
        if (IdentifierStr == "ac") return static_cast<int>(TokenType::tok_ac);
        if (IdentifierStr == "MAX") return static_cast<int>(TokenType::tok_max);
        if (IdentifierStr == "MIN") return static_cast<int>(TokenType::tok_min);
        if (IdentifierStr == "debug") return static_cast<int>(TokenType::tok_debug);
        if (IdentifierStr == "sensitivity") return static_cast<int>(TokenType::tok_sensitivity);
        if (IdentifierStr == "ask") return static_cast<int>(TokenType::tok_ask);
        if (IdentifierStr == "explain") return static_cast<int>(TokenType::tok_explain);
        if (IdentifierStr == "corner") return static_cast<int>(TokenType::tok_corner);
        if (IdentifierStr == "substitute") return static_cast<int>(TokenType::tok_substitute);
        if (IdentifierStr == "false") return static_cast<int>(TokenType::tok_false);
        if (IdentifierStr == "true") return static_cast<int>(TokenType::tok_true);
        if (IdentifierStr == "float") return static_cast<int>(TokenType::tok_type_float);
        if (IdentifierStr == "double") return static_cast<int>(TokenType::tok_type_double);
        if (IdentifierStr == "int") return static_cast<int>(TokenType::tok_type_int);
        if (IdentifierStr == "bool") return static_cast<int>(TokenType::tok_type_bool);
        if (IdentifierStr == "void") return static_cast<int>(TokenType::tok_type_void);
        if (IdentifierStr == "complex") return static_cast<int>(TokenType::tok_type_complex);
        if (IdentifierStr == "string") return static_cast<int>(TokenType::tok_type_string);
        if (IdentifierStr == "xor") return static_cast<int>(TokenType::tok_bitwise_xor);
        if (IdentifierStr == "break") return static_cast<int>(TokenType::tok_break);
        if (IdentifierStr == "continue") return static_cast<int>(TokenType::tok_continue);
        if (IdentifierStr == "switch") return static_cast<int>(TokenType::tok_switch);
        if (IdentifierStr == "case") return static_cast<int>(TokenType::tok_case);
        if (IdentifierStr == "default") return static_cast<int>(TokenType::tok_default);
        if (IdentifierStr == "try") return static_cast<int>(TokenType::tok_try);
        if (IdentifierStr == "catch") return static_cast<int>(TokenType::tok_catch);
        if (IdentifierStr == "finally") return static_cast<int>(TokenType::tok_finally);
        if (IdentifierStr == "throw") return static_cast<int>(TokenType::tok_throw);
        if (IdentifierStr == "assert") return static_cast<int>(TokenType::tok_assert);
        if (IdentifierStr == "static") return static_cast<int>(TokenType::tok_static);
        if (IdentifierStr == "const") return static_cast<int>(TokenType::tok_const);
        if (IdentifierStr == "enum") return static_cast<int>(TokenType::tok_enum);
        if (IdentifierStr == "struct") return static_cast<int>(TokenType::tok_struct);
        if (IdentifierStr == "class") return static_cast<int>(TokenType::tok_class);
        if (IdentifierStr == "public") return static_cast<int>(TokenType::tok_public);
        if (IdentifierStr == "private") return static_cast<int>(TokenType::tok_private);
        if (IdentifierStr == "protected") return static_cast<int>(TokenType::tok_protected);
        if (IdentifierStr == "virtual") return static_cast<int>(TokenType::tok_virtual);
        if (IdentifierStr == "override") return static_cast<int>(TokenType::tok_override);
        if (IdentifierStr == "inline") return static_cast<int>(TokenType::tok_inline);
        if (IdentifierStr == "noexcept") return static_cast<int>(TokenType::tok_noexcept);
        if (IdentifierStr == "export") return static_cast<int>(TokenType::tok_export);
        if (IdentifierStr == "namespace") return static_cast<int>(TokenType::tok_namespace);
        if (IdentifierStr == "using") return static_cast<int>(TokenType::tok_using);
        if (IdentifierStr == "typedef") return static_cast<int>(TokenType::tok_typedef);
        if (IdentifierStr == "alias") return static_cast<int>(TokenType::tok_alias);
        if (IdentifierStr == "match") return static_cast<int>(TokenType::tok_match);
        if (IdentifierStr == "guard") return static_cast<int>(TokenType::tok_guard);
        if (IdentifierStr == "foreach") return static_cast<int>(TokenType::tok_foreach);
        if (IdentifierStr == "repeat") return static_cast<int>(TokenType::tok_repeat);
        if (IdentifierStr == "until") return static_cast<int>(TokenType::tok_until);
        if (IdentifierStr == "parallel") return static_cast<int>(TokenType::tok_parallel);
        if (IdentifierStr == "schematic") return static_cast<int>(TokenType::tok_schematic);
        if (IdentifierStr == "component") return static_cast<int>(TokenType::tok_component);
        if (IdentifierStr == "connect") return static_cast<int>(TokenType::tok_connect);
        if (IdentifierStr == "port") return static_cast<int>(TokenType::tok_port);
        if (IdentifierStr == "net") return static_cast<int>(TokenType::tok_net);
        if (IdentifierStr == "pin") return static_cast<int>(TokenType::tok_pin);
        if (IdentifierStr == "sym") return static_cast<int>(TokenType::tok_sym);
        if (IdentifierStr == "simplify") return static_cast<int>(TokenType::tok_simplify);
        if (IdentifierStr == "differentiate") return static_cast<int>(TokenType::tok_differentiate);
        if (IdentifierStr == "integrate") return static_cast<int>(TokenType::tok_integrate);
        if (IdentifierStr == "laplace") return static_cast<int>(TokenType::tok_laplace);
        if (IdentifierStr == "inverse_laplace") return static_cast<int>(TokenType::tok_inverse_laplace);
        if (IdentifierStr == "evaluate") return static_cast<int>(TokenType::tok_evaluate);
        if (IdentifierStr == "jacobian") return static_cast<int>(TokenType::tok_jacobian);
        if (IdentifierStr == "pde") return static_cast<int>(TokenType::tok_pde);
        if (IdentifierStr == "pdiff") return static_cast<int>(TokenType::tok_partial_diff);
        if (IdentifierStr == "substitute") return static_cast<int>(TokenType::tok_substitute);
        if (IdentifierStr == "expand") return static_cast<int>(TokenType::tok_expand);
        if (IdentifierStr == "factor") return static_cast<int>(TokenType::tok_factor);
        if (IdentifierStr == "numerator") return static_cast<int>(TokenType::tok_numerator);
        if (IdentifierStr == "denominator") return static_cast<int>(TokenType::tok_denominator);
        if (IdentifierStr == "poles") return static_cast<int>(TokenType::tok_poles);
        if (IdentifierStr == "zeros") return static_cast<int>(TokenType::tok_zeros);
        if (IdentifierStr == "collect") return static_cast<int>(TokenType::tok_collect);
        if (IdentifierStr == "time") return static_cast<int>(TokenType::tok_time);
        if (IdentifierStr == "dt") return static_cast<int>(TokenType::tok_dt_var);
        if (IdentifierStr == "inputs") return static_cast<int>(TokenType::tok_inputs);
        if (IdentifierStr == "outputs") return static_cast<int>(TokenType::tok_outputs);
        if (IdentifierStr == "update") return static_cast<int>(TokenType::tok_update);
        if (IdentifierStr == "bsource") return static_cast<int>(TokenType::tok_bsource);
        if (IdentifierStr == "montecarlo" || IdentifierStr == "monte_carlo") return static_cast<int>(TokenType::tok_montecarlo);
        if (IdentifierStr == "worstcase" || IdentifierStr == "worst_case") return static_cast<int>(TokenType::tok_worstcase);
        if (IdentifierStr == "stability") return static_cast<int>(TokenType::tok_stability);
        if (IdentifierStr == "sensitivity") return static_cast<int>(TokenType::tok_sensitivity);
        if (IdentifierStr == "optimize") return static_cast<int>(TokenType::tok_optimize);
        // fft handled as regular identifier (function name)
        if (IdentifierStr == "phasor") return static_cast<int>(TokenType::tok_phasor);
        if (IdentifierStr == "bode") return static_cast<int>(TokenType::tok_bode);
        if (IdentifierStr == "esource") return static_cast<int>(TokenType::tok_esource);
        if (IdentifierStr == "fsource") return static_cast<int>(TokenType::tok_fsource);
        if (IdentifierStr == "gsource") return static_cast<int>(TokenType::tok_gsource);
        if (IdentifierStr == "hsource") return static_cast<int>(TokenType::tok_hsource);
        if (IdentifierStr == "node") return static_cast<int>(TokenType::tok_node);
        if (IdentifierStr == "initial") return static_cast<int>(TokenType::tok_initial);
        if (IdentifierStr == "transient") return static_cast<int>(TokenType::tok_transient);
        if (IdentifierStr == "timestep") return static_cast<int>(TokenType::tok_timestep);
        if (IdentifierStr == "simtime") return static_cast<int>(TokenType::tok_simtime);
        /* Hierarchical Design */
        if (IdentifierStr == "instance") return static_cast<int>(TokenType::tok_instance);
        if (IdentifierStr == "params") return static_cast<int>(TokenType::tok_params);
        
        /* Verilog-A Lite */
        if (IdentifierStr == "analog") return static_cast<int>(TokenType::tok_analog);
        if (IdentifierStr == "branch") return static_cast<int>(TokenType::tok_branch);
        if (IdentifierStr == "ddt") return static_cast<int>(TokenType::tok_ddt);
        if (IdentifierStr == "idt") return static_cast<int>(TokenType::tok_idt);
        if (IdentifierStr == "abstol") return static_cast<int>(TokenType::tok_abstol);
        if (IdentifierStr == "reltol") return static_cast<int>(TokenType::tok_reltol);
        
        /* Section 7.1: Model Quality & Verification */
        if (IdentifierStr == "settle") return static_cast<int>(TokenType::tok_settle);
        if (IdentifierStr == "golden") return static_cast<int>(TokenType::tok_golden);
        if (IdentifierStr == "compare") return static_cast<int>(TokenType::tok_compare);
        if (IdentifierStr == "tolerance") return static_cast<int>(TokenType::tok_tolerance);
        if (IdentifierStr == "converge") return static_cast<int>(TokenType::tok_converge);
        if (IdentifierStr == "diagnostic") return static_cast<int>(TokenType::tok_diagnostic);
        if (IdentifierStr == "discontinuity") return static_cast<int>(TokenType::tok_discontinuity);
        if (IdentifierStr == "state") return static_cast<int>(TokenType::tok_state);
        if (IdentifierStr == "verify") return static_cast<int>(TokenType::tok_verify);

        /* Section 7.2: Mixed-Signal & Modeling Extensions */
        // Event-driven constructs
        if (IdentifierStr == "above") return static_cast<int>(TokenType::tok_above);
        if (IdentifierStr == "timer") return static_cast<int>(TokenType::tok_timer);

        // Real-valued digital modeling
        if (IdentifierStr == "fsm") return static_cast<int>(TokenType::tok_fsm);
        if (IdentifierStr == "edge") return static_cast<int>(TokenType::tok_edge);
        if (IdentifierStr == "triggered") return static_cast<int>(TokenType::tok_triggered);
        if (IdentifierStr == "state_machine") return static_cast<int>(TokenType::tok_state_machine);
        if (IdentifierStr == "posedge") return static_cast<int>(TokenType::tok_edge);  // Reuse edge token
        if (IdentifierStr == "negedge") return static_cast<int>(TokenType::tok_edge);  // Reuse edge token

        // Noise modeling primitives
        if (IdentifierStr == "noise") return static_cast<int>(TokenType::tok_noise_fn);
        if (IdentifierStr == "white_noise") return static_cast<int>(TokenType::tok_white_noise);
        if (IdentifierStr == "flicker_noise") return static_cast<int>(TokenType::tok_flicker_noise);
        if (IdentifierStr == "thermal_noise") return static_cast<int>(TokenType::tok_thermal_noise);

        // Piecewise and table-based models
        if (IdentifierStr == "piecewise") return static_cast<int>(TokenType::tok_piecewise);
        if (IdentifierStr == "table") return static_cast<int>(TokenType::tok_table);
        if (IdentifierStr == "interpolate") return static_cast<int>(TokenType::tok_interpolate);
        if (IdentifierStr == "csv_import") return static_cast<int>(TokenType::tok_csv_import);

        // Units and dimensional analysis
        if (IdentifierStr == "unit") return static_cast<int>(TokenType::tok_unit);
        if (IdentifierStr == "dimension") return static_cast<int>(TokenType::tok_dimension);
        if (IdentifierStr == "convert") return static_cast<int>(TokenType::tok_convert);
        if (IdentifierStr == "has_unit") return static_cast<int>(TokenType::tok_has_unit);
        
        /* Advanced Features Keywords */
        if (IdentifierStr == "title") return static_cast<int>(TokenType::tok_title);
        if (IdentifierStr == "color") return static_cast<int>(TokenType::tok_color);
        if (IdentifierStr == "grid") return static_cast<int>(TokenType::tok_grid);
        if (IdentifierStr == "autoscale") return static_cast<int>(TokenType::tok_autoscale);
        if (IdentifierStr == "metric") return static_cast<int>(TokenType::tok_metric);
        if (IdentifierStr == "goals") return static_cast<int>(TokenType::tok_goals);
        if (IdentifierStr == "tune") return static_cast<int>(TokenType::tok_tune);
        if (IdentifierStr == "algorithm") return static_cast<int>(TokenType::tok_algorithm);
        if (IdentifierStr == "runs") return static_cast<int>(TokenType::tok_runs);
        if (IdentifierStr == "gpu") return static_cast<int>(TokenType::tok_gpu);
        if (IdentifierStr == "controls") return static_cast<int>(TokenType::tok_controls);
        if (IdentifierStr == "slider") return static_cast<int>(TokenType::tok_slider);
        if (IdentifierStr == "knob") return static_cast<int>(TokenType::tok_knob);
        if (IdentifierStr == "sections") return static_cast<int>(TokenType::tok_sections);
        if (IdentifierStr == "include") return static_cast<int>(TokenType::tok_include);
        
        /* Symbol Pin Mapping */
        if (IdentifierStr == "symbol") return static_cast<int>(TokenType::tok_symbol);
        if (IdentifierStr == "pinmap") return static_cast<int>(TokenType::tok_pinmap);
        if (IdentifierStr == "map") return static_cast<int>(TokenType::tok_map);

        // SPICE Time-Domain Simulation

        // Analysis Control
        if (IdentifierStr == "analysis") return static_cast<int>(TokenType::tok_analysis);
        if (IdentifierStr == "tran") return static_cast<int>(TokenType::tok_tran);
        if (IdentifierStr == "dc") return static_cast<int>(TokenType::tok_dc);
        if (IdentifierStr == "ac") return static_cast<int>(TokenType::tok_ac);
        if (IdentifierStr == "noise") return static_cast<int>(TokenType::tok_noise);
        if (IdentifierStr == "op") return static_cast<int>(TokenType::tok_op);
        if (IdentifierStr == "tf") return static_cast<int>(TokenType::tok_tf);
        if (IdentifierStr == "sens") return static_cast<int>(TokenType::tok_sens);
        if (IdentifierStr == "fourier") return static_cast<int>(TokenType::tok_fourier);

        // Measurements
        if (IdentifierStr == "measure") return static_cast<int>(TokenType::tok_measure);
        if (IdentifierStr == "MAX") return static_cast<int>(TokenType::tok_max);
        if (IdentifierStr == "MIN") return static_cast<int>(TokenType::tok_min);
        if (IdentifierStr == "AVG") return static_cast<int>(TokenType::tok_avg);
        if (IdentifierStr == "RMS") return static_cast<int>(TokenType::tok_rms);
        if (IdentifierStr == "TRIG") return static_cast<int>(TokenType::tok_trig);
        if (IdentifierStr == "TARG") return static_cast<int>(TokenType::tok_targ);
        if (IdentifierStr == "WHEN") return static_cast<int>(TokenType::tok_meas_when);
        if (IdentifierStr == "FIND") return static_cast<int>(TokenType::tok_find);
        if (IdentifierStr == "DERIV") return static_cast<int>(TokenType::tok_deriv);
        if (IdentifierStr == "INTEG") return static_cast<int>(TokenType::tok_integ);

        // Probing
        if (IdentifierStr == "probe") return static_cast<int>(TokenType::tok_probe);
        if (IdentifierStr == "save") return static_cast<int>(TokenType::tok_save);

        // Subcircuits and Models
        if (IdentifierStr == "subckt") return static_cast<int>(TokenType::tok_subckt);
        if (IdentifierStr == "ends") return static_cast<int>(TokenType::tok_ends);
        if (IdentifierStr == "model") return static_cast<int>(TokenType::tok_model);
        if (IdentifierStr == "param") return static_cast<int>(TokenType::tok_param);
        if (IdentifierStr == "ic") return static_cast<int>(TokenType::tok_ic);

        // VioMATRIXC Integration: A-Device Digital Gates
        if (IdentifierStr == "adevice") return static_cast<int>(TokenType::tok_adevice);
        if (IdentifierStr == "BUF") return static_cast<int>(TokenType::tok_buf);
        if (IdentifierStr == "NOT") return static_cast<int>(TokenType::tok_not_gate);
        if (IdentifierStr == "AND") return static_cast<int>(TokenType::tok_and_gate);
        if (IdentifierStr == "OR") return static_cast<int>(TokenType::tok_or_gate);
        if (IdentifierStr == "NAND") return static_cast<int>(TokenType::tok_nand_gate);
        if (IdentifierStr == "NOR") return static_cast<int>(TokenType::tok_nor_gate);
        if (IdentifierStr == "XOR") return static_cast<int>(TokenType::tok_xor_gate);
        if (IdentifierStr == "XNOR") return static_cast<int>(TokenType::tok_xnor_gate);
        if (IdentifierStr == "SCHMITT") return static_cast<int>(TokenType::tok_schmitt);
        if (IdentifierStr == "DFF") return static_cast<int>(TokenType::tok_dff);
        if (IdentifierStr == "JKFF") return static_cast<int>(TokenType::tok_jkff);
        if (IdentifierStr == "SRFF") return static_cast<int>(TokenType::tok_srff);
        if (IdentifierStr == "DLATCH") return static_cast<int>(TokenType::tok_dlatch);
        if (IdentifierStr == "COUNTER") return static_cast<int>(TokenType::tok_counter);

        // VioMATRIXC Integration: WAVEFILE Source
        if (IdentifierStr == "WAVEFILE") return static_cast<int>(TokenType::tok_wavefile);
        if (IdentifierStr == "CHAN") return static_cast<int>(TokenType::tok_chan);

        return static_cast<int>(TokenType::tok_identifier);
    }

    if (isdigit(m_lastChar)) { // Number: [0-9]+ or [0-9]+\.[0-9]*
        std::string NumStr;
        do {
            NumStr += m_lastChar;
            advance();
        } while (isdigit(m_lastChar));
        
        // Check for decimal point followed by digits
        if (m_lastChar == '.') {
            advance(); // consume the '.'
            NumStr += '.';
            // Only continue if there are digits after the decimal point
            if (isdigit(m_lastChar)) {
                do {
                    NumStr += m_lastChar;
                    advance();
                } while (isdigit(m_lastChar));
            }
        }

        // Check for scientific notation: 1e-10, 2.5E+5
        if (m_lastChar == 'e' || m_lastChar == 'E') {
            // Peek to see if it's a number (scientific notation) or a suffix (e.g., in 'meg')
            // If it's 'e' followed by + or - or digit, it's scientific notation.
            char peek = m_input[m_pos];
            if (isdigit(peek) || peek == '+' || peek == '-') {
                NumStr += m_lastChar;
                advance(); // consume 'e'
                if (m_lastChar == '+' || m_lastChar == '-') {
                    NumStr += m_lastChar;
                    advance();
                }
                while (isdigit(m_lastChar)) {
                    NumStr += m_lastChar;
                    advance();
                }
                // Once we have scientific notation, we don't allow SPICE suffixes on top of it.
                NumVal = strtod(NumStr.c_str(), nullptr);
                return static_cast<int>(TokenType::tok_number);
            }
        }

        // Check for imaginary suffix 'j' or 'J'
        if (m_lastChar == 'j' || m_lastChar == 'J') {
            advance(); // consume the 'j'
            NumVal = strtod(NumStr.c_str(), nullptr);
            return static_cast<int>(TokenType::tok_imaginary);
        }

        // If the number contains a decimal point, parse as float
        bool hasDot = NumStr.find('.') != std::string::npos;

        // Alphabetic suffix (SPICE multiplier or unit identifier)
        if (isalpha(m_lastChar)) {
            hasDot = true; // any number with a suffix is a float
            // Check for SPICE-style unit suffixes
            double multiplier = 1.0;
            bool hasSuffix = false;
            
            if (m_lastChar == 'k' || m_lastChar == 'K') {
                multiplier = 1e3; hasSuffix = true;
            } else if (m_lastChar == 'm' || m_lastChar == 'M') {
                char peek = m_input[m_pos];
                if (peek == 'e' || peek == 'E') {
                    char peek2 = m_input[m_pos + 1];
                    if (peek2 == 'g' || peek2 == 'G') {
                        multiplier = 1e6; hasSuffix = true;
                        advance(); advance();
                    } else {
                        multiplier = 1e-3; hasSuffix = true;
                    }
                } else {
                    multiplier = 1e-3; hasSuffix = true;
                }
            } else if (m_lastChar == 'u' || m_lastChar == 'U') {
                multiplier = 1e-6; hasSuffix = true;
            } else if (m_lastChar == 'n' || m_lastChar == 'N') {
                multiplier = 1e-9; hasSuffix = true;
            } else if (m_lastChar == 'p' || m_lastChar == 'P') {
                multiplier = 1e-12; hasSuffix = true;
            } else if (m_lastChar == 'f' || m_lastChar == 'F') {
                multiplier = 1e-15; hasSuffix = true;
            } else if (m_lastChar == 'a' || m_lastChar == 'A') {
                multiplier = 1e-18; hasSuffix = true;
            } else if (m_lastChar == 'g' || m_lastChar == 'G') {
                multiplier = 1e9; hasSuffix = true;
            } else if (m_lastChar == 't' || m_lastChar == 'T') {
                multiplier = 1e12; hasSuffix = true;
            }

            StringVal = "";
            if (hasSuffix) {
                char suffix = m_lastChar;
                StringVal += suffix;
                advance();
                while (m_lastChar != EOF && (isalpha(m_lastChar) || (unsigned char)m_lastChar > 127)) {
                    StringVal += m_lastChar;
                    advance();
                }
            } else {
                while (m_lastChar != EOF && (isalpha(m_lastChar) || (unsigned char)m_lastChar > 127)) {
                    StringVal += m_lastChar;
                    advance();
                }
            }

            NumVal = strtod(NumStr.c_str(), nullptr);
            return static_cast<int>(TokenType::tok_number);
        }

        if (hasDot) {
            // Has decimal point → parse as float
            NumVal = strtod(NumStr.c_str(), nullptr);
            return static_cast<int>(TokenType::tok_number);
        }

        // Bare integer literal (no '.', 'e', 'j', or suffix)
        IntVal = strtoll(NumStr.c_str(), nullptr, 10);
        return static_cast<int>(TokenType::tok_integer);
    }
    
    // Handle numbers starting with decimal point (.5) or element-wise operators (.*)
    if (m_lastChar == '.') {
        advance(); // consume the '.'
        if (isdigit(m_lastChar)) {
            std::string NumStr = ".";
            do {
                NumStr += m_lastChar;
                advance();
            } while (isdigit(m_lastChar));
            
            // Check for imaginary suffix
            if (m_lastChar == 'j' || m_lastChar == 'J') {
                advance();
                NumVal = strtod(NumStr.c_str(), nullptr);
                return static_cast<int>(TokenType::tok_imaginary);
            }
            
            NumVal = strtod(NumStr.c_str(), nullptr);
            return static_cast<int>(TokenType::tok_number);
        }
        
        // Element-wise operators (MATLAB-style): .*, ./, .^
        if (m_lastChar == '*') {
            advance();
            return static_cast<int>(TokenType::tok_ew_mul);
        }
        if (m_lastChar == '/') {
            advance();
            return static_cast<int>(TokenType::tok_ew_div);
        }
        if (m_lastChar == '^') {
            advance();
            return static_cast<int>(TokenType::tok_ew_power);
        }
        // Just a single . - could be part of a number or struct access
        return static_cast<int>(TokenType::tok_dot);
    }

    if (m_lastChar == '"') { // String literal: "..."
        advance(); // consume opening quote
        StringVal = "";
        while (m_lastChar != '"' && m_lastChar != EOF && m_lastChar != '\n') {
            if (m_lastChar == '\\') {
                // Handle escape sequences
                advance();
                switch (m_lastChar) {
                    case 'n': StringVal += '\n'; break;
                    case 't': StringVal += '\t'; break;
                    case 'r': StringVal += '\r'; break;
                    case '\\': StringVal += '\\'; break;
                    case '"': StringVal += '"'; break;
                    default: StringVal += m_lastChar; break;
                }
            } else {
                StringVal += m_lastChar;
            }
            advance();
        }
        if (m_lastChar == '"') {
            advance(); // consume closing quote
        } else {
            reportError("unterminated string literal");
        }
        return static_cast<int>(TokenType::tok_string);
    }

    if (m_lastChar == '#') {
        // Comment until end of line.
        do {
            advance();
        } while (m_lastChar != EOF && m_lastChar != '\n' && m_lastChar != '\r');

        if (m_lastChar != EOF) {
            // After eating comment, recursion to find next REAL token
            return gettok();
        }
    }

    // Handle C++ style // comments
    if (m_lastChar == '/') {
        advance();
        if (m_lastChar == '/') {
            // Comment until end of line
            do {
                advance();
            } while (m_lastChar != EOF && m_lastChar != '\n' && m_lastChar != '\r');

            if (m_lastChar != EOF) {
                return gettok();
            }
        }
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_slash_equal);
        }
        // Just a single / - return as division operator
        return '/';
    }

    // Handle semicolons as matrix row separators (MATLAB-style)
    if (m_lastChar == ';') {
        advance();
        return static_cast<int>(TokenType::tok_semicolon);
    }

    // Handle colon as slice operator (MATLAB-style) or namespace separator ::
    if (m_lastChar == ':') {
        advance();
        if (m_lastChar == ':') {
            advance();
            return static_cast<int>(TokenType::tok_namespace_sep);
        }
        return static_cast<int>(TokenType::tok_colon);
    }

    if (m_lastChar == '^') {
        advance();
        return static_cast<int>(TokenType::tok_power);
    }

    if (m_lastChar == '&') {
        advance();
        if (m_lastChar == '&') {
            advance();
            return static_cast<int>(TokenType::tok_logical_and);
        }
        return static_cast<int>(TokenType::tok_bitwise_and);
    }

    // Compound assignment operators
    if (m_lastChar == '+') {
        advance();
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_plus_equal);
        }
        return '+';
    }

    if (m_lastChar == '-') {
        advance();
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_minus_equal);
        }
        if (m_lastChar == '>') {
            advance();
            return static_cast<int>(TokenType::tok_arrow);
        }
        return '-';
    }

    if (m_lastChar == '*') {
        advance();
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_star_equal);
        }
        return '*';
    }

    if (m_lastChar == '|') {
        advance();
        if (m_lastChar == '|') {
            advance();
            return static_cast<int>(TokenType::tok_logical_or);
        }
        return static_cast<int>(TokenType::tok_bitwise_or);
    }
    
    if (m_lastChar == '!') {
        advance();
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_not_equal);
        }
        return static_cast<int>(TokenType::tok_logical_not);
    if (m_lastChar == '<') {
        advance();
        if (m_lastChar == '+') {
            advance();
            return static_cast<int>(TokenType::tok_contributor_op);
        }
        return '<';
    }
    }
    
    if (m_lastChar == '=') {
        advance();
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_equal);
        }
        return '='; // Assignment (not used in expressions currently)
    }
    
    if (m_lastChar == '<') {
        advance();
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_less_equal);
        }
        if (m_lastChar == '<') {
            advance();
            return static_cast<int>(TokenType::tok_bitwise_shl);
        }
        return '<';
    }
    
    if (m_lastChar == '>') {
        advance();
        if (m_lastChar == '=') {
            advance();
            return static_cast<int>(TokenType::tok_greater_equal);
        }
        if (m_lastChar == '>') {
            advance();
            return static_cast<int>(TokenType::tok_bitwise_shr);
        }
        return '>';
    }

    if (m_lastChar == '~') {
        advance();
        return static_cast<int>(TokenType::tok_bitwise_not);
    }

    // Handle transpose operator
    if (m_lastChar == '\'') {
        advance();
        return static_cast<int>(TokenType::tok_transpose);
    }

    // Handle braces for blocks
    if (m_lastChar == '{') {
        advance();
        return static_cast<int>(TokenType::tok_lbrace);
    }

    if (m_lastChar == '}') {
        advance();
        return static_cast<int>(TokenType::tok_rbrace);
    }

    // Check for end of file.  Don't eat the EOF.
    if (m_lastChar == EOF) {
        return static_cast<int>(TokenType::tok_eof);
    }

    // Otherwise, just return the character as its ascii value.
    int ThisChar = m_lastChar;
    advance();
    return ThisChar;
}

int Lexer::getNextToken() {
    CurTok = gettok();
    m_currentTokenOffset = std::min(m_tokenStart, m_input.size());
    if (m_pos >= m_tokenStart) {
        m_currentTokenLength = m_pos - m_tokenStart;
    } else {
        m_currentTokenLength = 0;
    }

    if (CurTok == static_cast<int>(TokenType::tok_eof))
        m_currentTokenLength = 0;

    // Update current token line/column for source mapping
    auto loc = getCurrentTokenLoc();
    if (loc.isValid()) {
        auto lineAndCol = m_sourceMgr->getLineAndColumn(loc);
        m_currentTokenLine = lineAndCol.first;
        m_currentTokenColumn = lineAndCol.second;
    }

    // std::cerr << "DEBUG getNextToken: returning " << CurTok << " '" << (char)CurTok << "'" << std::endl;
    return CurTok;
}

int Lexer::peekToken() {
    // Save state
    size_t savedPos = m_pos;
    char savedLastChar = m_lastChar;
    int savedLine = m_line;
    int savedColumn = m_column;
    size_t savedTokenStart = m_tokenStart;
    size_t savedErrorCount = m_errors.size();

    // Advance and get next token
    int nextTok = gettok();

    // Restore state
    m_pos = savedPos;
    m_lastChar = savedLastChar;
    m_line = savedLine;
    m_column = savedColumn;
    m_tokenStart = savedTokenStart;
    m_errors.resize(savedErrorCount);

    return nextTok;
}

} // namespace Flux
