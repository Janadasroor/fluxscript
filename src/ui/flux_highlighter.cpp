#include "flux/ui/flux_highlighter.h"

#include <QTextDocument>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QFont>

namespace Flux {

// ============================================================================
// EditorTheme Implementation
// ============================================================================

EditorTheme EditorTheme::darkTheme()
{
    EditorTheme theme;
    theme.name = "Dark";

    // Base colors
    theme.backgroundColor = QColor(30, 30, 30);
    theme.textColor = QColor(220, 220, 220);  // Brighter text
    theme.lineNumberColor = QColor(120, 120, 120);
    theme.currentLineColor = QColor(50, 50, 60);
    theme.selectionColor = QColor(60, 100, 150);
    theme.bracketColor = QColor(150, 150, 150);
    theme.whitespaceColor = QColor(80, 80, 80);

    // Syntax colors - brighter for better visibility
    theme.keywordColor = QColor(220, 150, 220);      // Bright purple
    theme.builtinColor = QColor(240, 200, 140);      // Bright orange
    theme.functionColor = QColor(120, 190, 255);     // Bright blue
    theme.functionDeclColor = QColor(100, 220, 200); // Bright teal
    theme.numberColor = QColor(200, 220, 180);       // Bright green
    theme.stringColor = QColor(230, 160, 140);       // Bright red-orange
    theme.commentColor = QColor(100, 120, 140);      // Lighter gray
    theme.typeColor = QColor(100, 220, 200);         // Bright teal
    theme.operatorColor = QColor(180, 180, 180);     // Lighter gray
    theme.preprocessorColor = QColor(180, 140, 220); // Light purple

    // Diagnostic colors
    theme.errorColor = QColor(255, 100, 100);
    theme.warningColor = QColor(255, 200, 50);
    theme.infoColor = QColor(100, 150, 255);

    return theme;
}

EditorTheme EditorTheme::lightTheme()
{
    EditorTheme theme;
    theme.name = "Light";

    // Base colors
    theme.backgroundColor = QColor(255, 255, 255);
    theme.textColor = QColor(0, 0, 0);
    theme.lineNumberColor = QColor(128, 128, 128);
    theme.currentLineColor = QColor(232, 242, 255);
    theme.selectionColor = QColor(180, 210, 255);
    theme.bracketColor = QColor(80, 80, 80);
    theme.whitespaceColor = QColor(200, 200, 200);

    // Syntax colors
    theme.keywordColor = QColor(0, 0, 128);          // Dark blue
    theme.builtinColor = QColor(170, 100, 0);        // Dark orange
    theme.functionColor = QColor(0, 100, 180);       // Dark blue
    theme.functionDeclColor = QColor(0, 120, 100);   // Dark teal
    theme.numberColor = QColor(0, 100, 0);           // Dark green
    theme.stringColor = QColor(180, 50, 50);         // Dark red
    theme.commentColor = QColor(128, 128, 128);      // Gray
    theme.typeColor = QColor(0, 120, 100);           // Dark teal
    theme.operatorColor = QColor(80, 80, 80);        // Dark gray
    theme.preprocessorColor = QColor(100, 50, 150);  // Purple

    // Diagnostic colors
    theme.errorColor = QColor(200, 50, 50);
    theme.warningColor = QColor(200, 150, 0);
    theme.infoColor = QColor(50, 100, 200);

    return theme;
}

EditorTheme EditorTheme::monokaiTheme()
{
    EditorTheme theme;
    theme.name = "Monokai";

    // Base colors
    theme.backgroundColor = QColor(39, 40, 34);
    theme.textColor = QColor(248, 248, 242);
    theme.lineNumberColor = QColor(117, 113, 94);
    theme.currentLineColor = QColor(63, 63, 52);
    theme.selectionColor = QColor(66, 66, 56);
    theme.bracketColor = QColor(150, 150, 150);
    theme.whitespaceColor = QColor(80, 80, 80);

    // Syntax colors
    theme.keywordColor = QColor(249, 38, 114);       // Pink
    theme.builtinColor = QColor(174, 129, 255);      // Purple
    theme.functionColor = QColor(166, 226, 46);      // Green
    theme.functionDeclColor = QColor(102, 217, 239); // Cyan
    theme.numberColor = QColor(174, 129, 255);       // Purple
    theme.stringColor = QColor(230, 219, 116);       // Yellow
    theme.commentColor = QColor(117, 113, 94);       // Gray
    theme.typeColor = QColor(102, 217, 239);         // Cyan
    theme.operatorColor = QColor(249, 38, 114);      // Pink
    theme.preprocessorColor = QColor(230, 219, 116); // Yellow

    // Diagnostic colors
    theme.errorColor = QColor(249, 38, 114);
    theme.warningColor = QColor(230, 219, 116);
    theme.infoColor = QColor(102, 217, 239);

    return theme;
}

// ============================================================================
// Highlighter Implementation
// ============================================================================

Highlighter::Highlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
    , m_semanticTokensDirty(false)
{
    setupFluxKeywords();
    setupFluxBuiltins();
    setupHighlightingRules();
    setupMultilineRules();
}

Highlighter::~Highlighter()
{
}

void Highlighter::setTheme(const EditorTheme& theme)
{
    m_theme = theme;
    setupHighlightingRules();
    rehighlight();
}

void Highlighter::setSemanticTokens(const QVector<SemanticToken>& tokens)
{
    m_semanticTokens = tokens;
    m_semanticTokensDirty = true;
    rehighlight();
}

void Highlighter::clearSemanticTokens()
{
    m_semanticTokens.clear();
    m_semanticTokensDirty = false;
}

void Highlighter::setErrorLines(const QList<int>& lines)
{
    m_errorLines = lines;
    rehighlight();
}

void Highlighter::setWarningLines(const QList<int>& lines)
{
    m_warningLines = lines;
    rehighlight();
}

void Highlighter::clearDiagnostics()
{
    m_errorLines.clear();
    m_warningLines.clear();
    rehighlight();
}

void Highlighter::addKeyword(const QString& keyword)
{
    if (!m_keywords.contains(keyword)) {
        m_keywords.append(keyword);
        setupHighlightingRules();
        rehighlight();
    }
}

void Highlighter::addBuiltin(const QString& builtin)
{
    if (!m_builtins.contains(builtin)) {
        m_builtins.append(builtin);
        setupHighlightingRules();
        rehighlight();
    }
}

void Highlighter::addType(const QString& type)
{
    if (!m_types.contains(type)) {
        m_types.append(type);
        setupHighlightingRules();
        rehighlight();
    }
}

void Highlighter::removeKeyword(const QString& keyword)
{
    m_keywords.removeOne(keyword);
    setupHighlightingRules();
    rehighlight();
}

void Highlighter::updateFluxKeywords()
{
    setupFluxKeywords();
    setupHighlightingRules();
    rehighlight();
}

void Highlighter::updateFluxBuiltins()
{
    setupFluxBuiltins();
    setupHighlightingRules();
    rehighlight();
}

void Highlighter::highlightBlock(const QString& text)
{
    int blockNumber = currentBlock().blockNumber();

    // Apply lexical highlighting (regex-based)
    for (const auto& rule : m_highlightingRules) {
        applyFormat(rule, text);
    }

    // Apply semantic highlighting
    if (!m_semanticTokens.isEmpty()) {
        applySemanticHighlighting(text, blockNumber);
    }

    // Apply diagnostic highlighting
    applyDiagnosticHighlighting(text, blockNumber);

    // Handle multiline constructs
    setCurrentBlockState(StateNormal);

    int state = previousBlockState();
    int startIndex = 0;
    int startOffset = 0;

    // Handle multiline strings
    if (state == StateMultiLineString) {
        QRegularExpressionMatch match = m_multilineRules[0].endPattern.match(text);
        if (match.hasMatch()) {
            int endIndex = match.capturedStart();
            QTextCharFormat format = m_multilineRules[0].format;
            setFormat(0, endIndex + match.capturedLength(), format);
            setCurrentBlockState(StateNormal);
        } else {
            QTextCharFormat format = m_multilineRules[0].format;
            setFormat(0, text.length(), format);
            setCurrentBlockState(StateMultiLineString);
        }
        return;
    }

    // Check for multiline string start
    for (const auto& rule : m_multilineRules) {
        QRegularExpressionMatch startMatch = rule.startPattern.match(text);
        if (startMatch.hasMatch()) {
            startIndex = startMatch.capturedStart();
            startOffset = startMatch.capturedLength();

            QRegularExpressionMatch endMatch = rule.endPattern.match(text, startIndex + startOffset);
            if (endMatch.hasMatch()) {
                int endIndex = endMatch.capturedStart();
                int endLength = endMatch.capturedLength();
                setFormat(startIndex, endIndex - startIndex + endLength, rule.format);
            } else {
                setFormat(startIndex, text.length() - startIndex, rule.format);
                setCurrentBlockState(rule.startState);
            }
            break;
        }
    }
}

void Highlighter::applySemanticHighlighting(const QString& text, int blockNumber)
{
    for (const auto& token : m_semanticTokens) {
        // Check if token is in this block
        int blockStart = 0;
        int blockEnd = text.length();

        if (token.start >= blockStart && token.start < blockEnd) {
            QTextCharFormat format;

            switch (token.type) {
            case SemanticToken::Type::Variable:
                format.setForeground(m_theme.textColor);
                break;
            case SemanticToken::Type::Function:
                format.setForeground(m_theme.functionColor);
                break;
            case SemanticToken::Type::Type:
                format.setForeground(m_theme.typeColor);
                break;
            case SemanticToken::Type::Namespace:
                format.setForeground(m_theme.builtinColor);
                break;
            default:
                break;
            }

            if (token.modifier == SemanticToken::Modifier::Declaration) {
                format.setFontWeight(QFont::Bold);
            }

            setFormat(token.start, token.length, format);
        }
    }
}

void Highlighter::applyDiagnosticHighlighting(const QString& text, int blockNumber)
{
    // Error lines
    if (m_errorLines.contains(blockNumber)) {
        QTextCharFormat format;
        format.setUnderlineStyle(QTextCharFormat::SpellCheckUnderline);
        format.setUnderlineColor(m_theme.errorColor);
        setFormat(0, text.length(), format);
    }

    // Warning lines
    if (m_warningLines.contains(blockNumber)) {
        QTextCharFormat format;
        format.setUnderlineStyle(QTextCharFormat::SingleUnderline);
        format.setUnderlineColor(m_theme.warningColor);
        setFormat(0, text.length(), format);
    }
}

void Highlighter::setupFluxKeywords()
{
    m_keywords = {
        // Control flow
        "if", "else", "then", "endif",
        "for", "while", "do", "end",
        "break", "continue", "return",

        // Declarations
        "def", "var", "let", "in",
        "extern",

        // Types
        "int", "double", "float", "void",
        "bool", "true", "false",
        "matrix", "vector", "complex",

        // Logical operators
        "and", "or", "not",
        "&&", "||", "!",

        // Other keywords
        "import", "export", "module",
        "namespace", "using"
    };
}

void Highlighter::setupFluxBuiltins()
{
    m_builtins = {
        // Math functions
        "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
        "sinh", "cosh", "tanh",
        "exp", "log", "log10", "log2",
        "sqrt", "cbrt", "pow",
        "abs", "floor", "ceil", "round",

        // Matrix operations
        "det", "inv", "transpose", "trace",
        "eig", "svd", "norm",

        // Constants
        "pi", "e", "inf", "nan",

        // I/O
        "print", "println", "read", "write",

        // Utilities
        "size", "length", "range", "linspace"
    };

    m_types = {
        "double", "float", "int", "void",
        "bool", "string", "matrix", "vector", "complex"
    };
}

void Highlighter::setupHighlightingRules()
{
    m_highlightingRules.clear();

    // Keywords
    HighlightingRule keywordRule;
    keywordRule.pattern = QRegularExpression("\\b(" + m_keywords.join("|") + ")\\b");
    keywordRule.format.setForeground(m_theme.keywordColor);
    keywordRule.format.setFontWeight(QFont::Bold);
    keywordRule.ruleName = "keyword";
    m_highlightingRules.append(keywordRule);

    // Builtins
    HighlightingRule builtinRule;
    builtinRule.pattern = QRegularExpression("\\b(" + m_builtins.join("|") + ")\\b");
    builtinRule.format.setForeground(m_theme.builtinColor);
    builtinRule.ruleName = "builtin";
    m_highlightingRules.append(builtinRule);

    // Types
    HighlightingRule typeRule;
    typeRule.pattern = QRegularExpression("\\b(" + m_types.join("|") + ")\\b");
    typeRule.format.setForeground(m_theme.typeColor);
    typeRule.format.setFontWeight(QFont::Bold);
    typeRule.ruleName = "type";
    m_highlightingRules.append(typeRule);

    // Numbers (integers and floats)
    HighlightingRule numberRule;
    numberRule.pattern = QRegularExpression("\\b[0-9]+\\.?[0-9]*([eE][+-]?[0-9]+)?\\b");
    numberRule.format.setForeground(m_theme.numberColor);
    numberRule.ruleName = "number";
    m_highlightingRules.append(numberRule);

    // Hex numbers
    HighlightingRule hexRule;
    hexRule.pattern = QRegularExpression("0[xX][0-9a-fA-F]+");
    hexRule.format.setForeground(m_theme.numberColor);
    hexRule.ruleName = "hex";
    m_highlightingRules.append(hexRule);

    // Strings (double-quoted)
    HighlightingRule stringRule;
    stringRule.pattern = QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\"");
    stringRule.format.setForeground(m_theme.stringColor);
    stringRule.ruleName = "string";
    m_highlightingRules.append(stringRule);

    // Strings (single-quoted)
    HighlightingRule singleStringRule;
    singleStringRule.pattern = QRegularExpression("'(?:[^'\\\\]|\\\\.)*'");
    singleStringRule.format.setForeground(m_theme.stringColor);
    singleStringRule.ruleName = "single_string";
    m_highlightingRules.append(singleStringRule);

    // Comments (single line)
    HighlightingRule commentRule;
    commentRule.pattern = QRegularExpression("//[^\n]*");
    commentRule.format.setForeground(m_theme.commentColor);
    commentRule.format.setFontItalic(true);
    commentRule.ruleName = "comment";
    m_highlightingRules.append(commentRule);

    // Operators
    HighlightingRule operatorRule;
    operatorRule.pattern = QRegularExpression("[+\\-*/%=<>!&|^~?:]+");
    operatorRule.format.setForeground(m_theme.operatorColor);
    operatorRule.ruleName = "operator";
    m_highlightingRules.append(operatorRule);

    // Brackets
    HighlightingRule bracketRule;
    bracketRule.pattern = QRegularExpression("[()\\[\\]{}]");
    bracketRule.format.setForeground(m_theme.bracketColor);
    bracketRule.ruleName = "bracket";
    m_highlightingRules.append(bracketRule);

    // Function calls (identifier followed by parenthesis)
    HighlightingRule functionRule;
    functionRule.pattern = QRegularExpression("\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*(?=\\()");
    functionRule.format.setForeground(m_theme.functionColor);
    functionRule.ruleName = "function";
    m_highlightingRules.append(functionRule);
}

void Highlighter::setupMultilineRules()
{
    m_multilineRules.clear();

    // Multi-line strings (triple quotes)
    MultilineRule multiStringRule;
    multiStringRule.startPattern = QRegularExpression("\"\"\"");
    multiStringRule.endPattern = QRegularExpression("\"\"\"");
    multiStringRule.format.setForeground(m_theme.stringColor);
    multiStringRule.startState = StateMultiLineString;
    multiStringRule.endState = StateNormal;
    m_multilineRules.append(multiStringRule);

    // Multi-line comments (/* ... */)
    MultilineRule multiCommentRule;
    multiCommentRule.startPattern = QRegularExpression("/\\*");
    multiCommentRule.endPattern = QRegularExpression("\\*/");
    multiCommentRule.format.setForeground(m_theme.commentColor);
    multiCommentRule.format.setFontItalic(true);
    multiCommentRule.startState = StateMultiLineComment;
    multiCommentRule.endState = StateNormal;
    m_multilineRules.append(multiCommentRule);
}

void Highlighter::applyFormat(const HighlightingRule& rule, const QString& text)
{
    QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        setFormat(match.capturedStart(), match.capturedLength(), rule.format);
    }
}

int Highlighter::previousBlockState() const
{
    return QSyntaxHighlighter::previousBlockState();
}

void Highlighter::setCurrentBlockState(int state)
{
    QSyntaxHighlighter::setCurrentBlockState(state);
}

} // namespace Flux
