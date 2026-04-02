#include "flux/ui/flux_completer.h"

#include <QTextDocument>
#include <QTextCursor>
#include <QTextBlock>
#include <QAbstractItemView>
#include <QListView>
#include <QStandardItem>
#include <QApplication>
#include <QScrollBar>
#include <QKeyEvent>
#include <QRegularExpression>

#include <vector>

namespace Flux {

// ============================================================================
// CompletionItem Implementation
// ============================================================================

QString CompletionItem::iconName() const
{
    switch (type) {
    case CompletionItemType::Keyword:
        return "keyword";
    case CompletionItemType::Builtin:
        return "builtin";
    case CompletionItemType::Function:
        return "function";
    case CompletionItemType::Variable:
        return "variable";
    case CompletionItemType::Type:
        return "type";
    case CompletionItemType::Module:
        return "module";
    case CompletionItemType::Snippet:
        return "snippet";
    case CompletionItemType::Parameter:
        return "parameter";
    case CompletionItemType::File:
        return "file";
    default:
        return "default";
    }
}

// ============================================================================
// KeywordCompletionProvider Implementation
// ============================================================================

KeywordCompletionProvider::KeywordCompletionProvider(const QStringList& keywords)
    : m_keywords(keywords)
{
}

QList<CompletionItem> KeywordCompletionProvider::completions(const CompletionContext& context)
{
    QList<CompletionItem> items;

    for (const QString& keyword : m_keywords) {
        if (keyword.startsWith(context.currentWord, Qt::CaseInsensitive)) {
            CompletionItem item;
            item.text = keyword;
            item.displayText = keyword;
            item.type = CompletionItemType::Keyword;
            item.priority = 10;
            items.append(item);
        }
    }

    return items;
}

void KeywordCompletionProvider::setKeywords(const QStringList& keywords)
{
    m_keywords = keywords;
}

// ============================================================================
// BuiltinCompletionProvider Implementation
// ============================================================================

BuiltinCompletionProvider::BuiltinCompletionProvider(const QMap<QString, QString>& builtins)
    : m_builtins(builtins)
{
}

QList<CompletionItem> BuiltinCompletionProvider::completions(const CompletionContext& context)
{
    QList<CompletionItem> items;

    for (auto it = m_builtins.begin(); it != m_builtins.end(); ++it) {
        const QString& name = it.key();
        const QString& signature = it.value();

        if (name.startsWith(context.currentWord, Qt::CaseInsensitive)) {
            CompletionItem item;
            item.text = name;
            item.displayText = name;
            item.detailText = signature;
            item.type = CompletionItemType::Builtin;
            item.priority = 20;
            items.append(item);
        }
    }

    return items;
}

void BuiltinCompletionProvider::setBuiltins(const QMap<QString, QString>& builtins)
{
    m_builtins = builtins;
}

// ============================================================================
// SymbolCompletionProvider Implementation
// ============================================================================

QList<CompletionItem> SymbolCompletionProvider::completions(const CompletionContext& context)
{
    QList<CompletionItem> items;

    for (const auto& symbol : m_symbols) {
        if (symbol.text.startsWith(context.currentWord, Qt::CaseInsensitive)) {
            items.append(symbol);
        }
    }

    return items;
}

void SymbolCompletionProvider::updateSymbols(const QList<CompletionItem>& symbols)
{
    m_symbols = symbols;
}

// ============================================================================
// Completer Implementation
// ============================================================================

Completer::Completer(QObject* parent)
    : QCompleter(parent)
    , m_model(nullptr)
    , m_isShowingCompletion(false)
{
    setupModel();
    setupPopup();
    setupDefaultProviders();
}

Completer::~Completer()
{
}

void Completer::setupModel()
{
    m_model = new QStandardItemModel(this);
    setModel(m_model);

    // Set model columns
    m_model->setColumnCount(2);  // Text and detail
}

void Completer::setupPopup()
{
    auto* listView = qobject_cast<QListView*>(popup());
    if (listView) {
        listView->setUniformItemSizes(true);
        listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    }
}

void Completer::setupDefaultProviders()
{
    // Add keyword provider
    addProvider(std::make_unique<KeywordCompletionProvider>(m_keywords));

    // Add builtin provider
    addProvider(std::make_unique<BuiltinCompletionProvider>(m_builtins));

    // Add symbol provider
    addProvider(std::make_unique<SymbolCompletionProvider>());
}

void Completer::addProvider(std::unique_ptr<CompletionProvider> provider)
{
    if (provider) {
        m_providers[provider->name()] = provider.get();
        m_providerStorage.push_back(std::move(provider));
    }
}

void Completer::removeProvider(const QString& name)
{
    auto it = m_providers.find(name);
    if (it != m_providers.end()) {
        CompletionProvider* provider = it.value();
        m_providerStorage.erase(
            std::remove_if(m_providerStorage.begin(), m_providerStorage.end(),
                [provider](const std::unique_ptr<CompletionProvider>& p) {
                    return p.get() == provider;
                }),
            m_providerStorage.end()
        );
        m_providers.erase(it);
    }
}

CompletionProvider* Completer::provider(const QString& name) const
{
    return m_providers.value(name, nullptr);
}

void Completer::updateCompletions(const QStringList& additionalKeywords)
{
    for (const QString& keyword : additionalKeywords) {
        if (!m_keywords.contains(keyword)) {
            m_keywords.append(keyword);
        }
    }

    // Update keyword provider
    auto* keywordProvider = dynamic_cast<KeywordCompletionProvider*>(provider("Keywords"));
    if (keywordProvider) {
        keywordProvider->setKeywords(m_keywords);
    }
}

void Completer::updateSymbols(const QList<CompletionItem>& symbols)
{
    auto* symbolProvider = dynamic_cast<SymbolCompletionProvider*>(provider("Symbols"));
    if (symbolProvider) {
        symbolProvider->updateSymbols(symbols);
    }
}

void Completer::updateFunctions(const QMap<QString, QString>& functions)
{
    m_functions = functions;

    // Merge with builtins
    for (auto it = functions.begin(); it != functions.end(); ++it) {
        if (!m_builtins.contains(it.key())) {
            m_builtins[it.key()] = it.value();
        }
    }

    // Update builtin provider
    auto* builtinProvider = dynamic_cast<BuiltinCompletionProvider*>(provider("Builtins"));
    if (builtinProvider) {
        builtinProvider->setBuiltins(m_builtins);
    }
}

void Completer::updateFluxKeywords()
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

    updateCompletions();
}

void Completer::updateFluxBuiltins()
{
    m_builtins = {
        // Math functions
        { "sin", "sin(x)" },
        { "cos", "cos(x)" },
        { "tan", "tan(x)" },
        { "asin", "asin(x)" },
        { "acos", "acos(x)" },
        { "atan", "atan(x)" },
        { "atan2", "atan2(y, x)" },
        { "sinh", "sinh(x)" },
        { "cosh", "cosh(x)" },
        { "tanh", "tanh(x)" },
        { "exp", "exp(x)" },
        { "log", "log(x)" },
        { "log10", "log10(x)" },
        { "log2", "log2(x)" },
        { "sqrt", "sqrt(x)" },
        { "cbrt", "cbrt(x)" },
        { "pow", "pow(base, exp)" },
        { "abs", "abs(x)" },
        { "floor", "floor(x)" },
        { "ceil", "ceil(x)" },
        { "round", "round(x)" },

        // Matrix operations
        { "det", "det(A)" },
        { "inv", "inv(A)" },
        { "transpose", "transpose(A)" },
        { "trace", "trace(A)" },
        { "eig", "eig(A)" },
        { "svd", "svd(A)" },
        { "norm", "norm(A)" },

        // Constants
        { "pi", "3.14159..." },
        { "e", "2.71828..." },

        // I/O
        { "print", "print(x)" },
        { "println", "println(x)" },

        // Utilities
        { "size", "size(A)" },
        { "length", "length(v)" },
        { "range", "range(start, end)" },
        { "linspace", "linspace(start, end, n)" }
    };

    // Update builtin provider
    auto* builtinProvider = dynamic_cast<BuiltinCompletionProvider*>(provider("Builtins"));
    if (builtinProvider) {
        builtinProvider->setBuiltins(m_builtins);
    }
}

CompletionContext Completer::analyzeContext(const QTextCursor& cursor)
{
    CompletionContext context;
    context.cursor = cursor;

    // Get current block
    QTextBlock block = cursor.block();
    context.lineNumber = block.blockNumber();
    context.columnNumber = cursor.positionInBlock();

    // Get text before cursor
    QString textBeforeCursor = block.text().left(cursor.positionInBlock());

    // Find current word
    QRegularExpression wordRegex("[a-zA-Z_][a-zA-Z0-9_]*$");
    QRegularExpressionMatch match = wordRegex.match(textBeforeCursor);
    if (match.hasMatch()) {
        context.currentWord = match.captured(0);
    }

    context.precedingText = textBeforeCursor;

    // Determine trigger kind
    if (!context.currentWord.isEmpty()) {
        context.triggerKind = CompletionContext::TriggerKind::Continuous;
    } else {
        context.triggerKind = CompletionContext::TriggerKind::Invoked;
    }

    // Check for trigger character
    if (!textBeforeCursor.isEmpty()) {
        QChar lastChar = textBeforeCursor.at(textBeforeCursor.length() - 1);
        if (lastChar == '.' || lastChar == ':') {
            context.triggerKind = CompletionContext::TriggerKind::TriggerCharacter;
            context.triggerCharacter = lastChar;
        }
    }

    return context;
}

QString Completer::documentationFor(const QString& completion) const
{
    return m_documentation.value(completion);
}

void Completer::setDocumentation(const QString& completion, const QString& docs)
{
    m_documentation[completion] = docs;
}

void Completer::showCompletionPopup()
{
    m_isShowingCompletion = true;
}

void Completer::hideCompletionPopup()
{
    m_isShowingCompletion = false;
}

bool Completer::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);

        // Handle Tab/Enter to accept completion
        if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Return) {
            if (popup()->isVisible()) {
                QModelIndex currentIndex = popup()->currentIndex();
                if (currentIndex.isValid()) {
                    QString completion = currentIndex.data().toString();
                    emit completionSelected(CompletionItem{completion, completion, "", "", CompletionItemType::Keyword, 0, completion, 0});
                }
            }
        }

        // Handle Escape to close popup
        if (keyEvent->key() == Qt::Key_Escape) {
            if (popup()->isVisible()) {
                popup()->hide();
                m_isShowingCompletion = false;
            }
        }
    }

    return QCompleter::eventFilter(watched, event);
}

QList<CompletionItem> Completer::generateCompletions(const CompletionContext& context)
{
    QList<CompletionItem> allItems;

    // Collect completions from all providers
    for (auto& provider : m_providerStorage) {
        auto items = provider->completions(context);
        allItems.append(items);
    }

    // Sort by priority
    std::sort(allItems.begin(), allItems.end(), [](const CompletionItem& a, const CompletionItem& b) {
        return a.priority < b.priority;
    });

    return allItems;
}

void Completer::populateModel(const QList<CompletionItem>& items)
{
    m_model->clear();

    for (const auto& item : items) {
        auto* textItem = new QStandardItem(item.displayText);
        auto* detailItem = new QStandardItem(item.detailText);

        // Set icon based on type
        // (Would use actual icons in production)

        QList<QStandardItem*> rowItems;
        rowItems << textItem << detailItem;
        m_model->appendRow(rowItems);
    }
}

CompletionItem Completer::createKeywordItem(const QString& keyword)
{
    CompletionItem item;
    item.text = keyword;
    item.displayText = keyword;
    item.type = CompletionItemType::Keyword;
    item.priority = 10;
    return item;
}

CompletionItem Completer::createBuiltinItem(const QString& name, const QString& signature)
{
    CompletionItem item;
    item.text = name;
    item.displayText = name;
    item.detailText = signature;
    item.type = CompletionItemType::Builtin;
    item.priority = 20;
    return item;
}

CompletionItem Completer::createSymbolItem(const CompletionItem& symbol)
{
    CompletionItem item = symbol;
    item.priority = 30;
    return item;
}

} // namespace Flux
