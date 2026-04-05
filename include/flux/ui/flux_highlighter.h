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

#ifndef FLUX_HIGHLIGHTER_H
#define FLUX_HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>
#include <QColor>
#include <QFont>
#include <memory>

namespace Flux {

/**
 * @brief Editor theme configuration for syntax highlighting
 */
struct EditorTheme {
    QString name;
    
    // Base colors
    QColor backgroundColor;
    QColor textColor;
    QColor lineNumberColor;
    QColor currentLineColor;
    QColor selectionColor;
    QColor bracketColor;
    QColor whitespaceColor;
    
    // Syntax colors
    QColor keywordColor;
    QColor builtinColor;
    QColor functionColor;
    QColor functionDeclColor;
    QColor numberColor;
    QColor stringColor;
    QColor commentColor;
    QColor typeColor;
    QColor operatorColor;
    QColor preprocessorColor;
    QColor labelColor;
    
    // Diagnostic colors
    QColor errorColor;
    QColor warningColor;
    QColor infoColor;
    
    static EditorTheme darkTheme();
    static EditorTheme lightTheme();
    static EditorTheme monokaiTheme();
};

/**
 * @brief Semantic token information for advanced highlighting
 */
struct SemanticToken {
    enum class Type {
        Variable,
        Parameter,
        Function,
        Method,
        Class,
        Namespace,
        Type,
        Property,
        Enum,
        Macro,
        Decorator,
        Label
    };
    
    enum class Modifier {
        None,
        Declaration,
        Definition,
        Readonly,
        Static,
        Modified,
        DefaultLibrary
    };
    
    int start;
    int length;
    Type type;
    Modifier modifier;
};

/**
 * @brief Advanced syntax highlighter with multi-level highlighting support
 * 
 * Highlighting Levels:
 * 1. Lexical: Keywords, operators, literals (regex-based, immediate)
 * 2. Semantic: Variable types, function definitions (AST-based, delayed)
 * 3. Diagnostic: Errors, warnings, hints (compiler feedback)
 */
class Highlighter : public QSyntaxHighlighter {
    Q_OBJECT
    
public:
    explicit Highlighter(QTextDocument* parent = nullptr);
    ~Highlighter() override;

    // ========================================================================
    // Theme Management
    // ========================================================================
    void setTheme(const EditorTheme& theme);
    EditorTheme currentTheme() const { return m_theme; }
    
    // ========================================================================
    // Semantic Highlighting
    // ========================================================================
    void setSemanticTokens(const QVector<SemanticToken>& tokens);
    void clearSemanticTokens();
    
    // ========================================================================
    // Diagnostic Highlighting
    // ========================================================================
    void setErrorLines(const QList<int>& lines);
    void setWarningLines(const QList<int>& lines);
    void clearDiagnostics();
    
    // ========================================================================
    // Custom Keywords
    // ========================================================================
    void addKeyword(const QString& keyword);
    void addBuiltin(const QString& builtin);
    void addType(const QString& type);
    void removeKeyword(const QString& keyword);
    
    // ========================================================================
    // FluxScript-Specific
    // ========================================================================
    void updateFluxKeywords();
    void updateFluxBuiltins();

protected:
    void highlightBlock(const QString& text) override;
    void applySemanticHighlighting(const QString& text, int blockNumber);
    void applyDiagnosticHighlighting(const QString& text, int blockNumber);

private:
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
        QString ruleName;  // For debugging/profiling
    };
    
    struct MultilineRule {
        QRegularExpression startPattern;
        QRegularExpression endPattern;
        QTextCharFormat format;
        int startState;
        int endState;
    };

    // ========================================================================
    // Initialization
    // ========================================================================
    void setupFluxKeywords();
    void setupFluxBuiltins();
    void setupHighlightingRules();
    void setupMultilineRules();
    void applyFormat(const HighlightingRule& rule, const QString& text);
    
    // ========================================================================
    // State Management
    // ========================================================================
    int previousBlockState() const;
    void setCurrentBlockState(int state);
    
    // ========================================================================
    // Members
    // ========================================================================
    EditorTheme m_theme;
    
    // Highlighting rules (lexical level)
    QVector<HighlightingRule> m_highlightingRules;
    QVector<MultilineRule> m_multilineRules;
    
    // Semantic tokens (semantic level)
    QVector<SemanticToken> m_semanticTokens;
    bool m_semanticTokensDirty;
    
    // Diagnostic lines (diagnostic level)
    QList<int> m_errorLines;
    QList<int> m_warningLines;
    
    // Formats (cached for performance)
    QTextCharFormat m_keywordFormat;
    QTextCharFormat m_builtinFormat;
    QTextCharFormat m_functionFormat;
    QTextCharFormat m_functionDeclFormat;
    QTextCharFormat m_numberFormat;
    QTextCharFormat m_stringFormat;
    QTextCharFormat m_commentFormat;
    QTextCharFormat m_typeFormat;
    QTextCharFormat m_operatorFormat;
    QTextCharFormat m_preprocessorFormat;
    QTextCharFormat m_errorFormat;
    QTextCharFormat m_warningFormat;
    
    // FluxScript keywords
    QStringList m_keywords;
    QStringList m_builtins;
    QStringList m_types;
    QStringList m_operators;
    
    // Multiline state tracking
    enum BlockState {
        StateNormal = 0,
        StateMultiLineString = 1,
        StateMultiLineComment = 2
    };
};

} // namespace Flux

#endif // FLUX_HIGHLIGHTER_H
