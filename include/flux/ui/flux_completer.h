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

#ifndef FLUX_COMPLETER_H
#define FLUX_COMPLETER_H

#include <QCompleter>
#include <QAbstractItemView>
#include <QStandardItemModel>
#include <QTextCursor>
#include <QMap>
#include <memory>

namespace Flux {

/**
 * @brief Completion item types for intelligent code completion
 */
enum class CompletionItemType {
    Keyword,
    Builtin,
    Function,
    Variable,
    Type,
    Module,
    Snippet,
    Parameter,
    File
};

/**
 * @brief Represents a single completion item with metadata
 */
struct CompletionItem {
    QString text;
    QString displayText;      // What's shown in the popup
    QString detailText;       // Type info or signature
    QString documentation;    // Hover documentation
    CompletionItemType type;
    int priority;             // Lower = higher priority
    QString insertText;       // Actual text to insert (may differ from display)
    int insertOffset;         // Cursor position after insert (for snippets)
    
    // Icon based on type
    QString iconName() const;
};

/**
 * @brief Context information for completion requests
 */
struct CompletionContext {
    enum class TriggerKind {
        Invoked,      // Manual trigger (Ctrl+Space)
        TriggerCharacter,  // Auto-triggered by character (., ->)
        Continuous    // Continuous typing
    };
    
    QTextCursor cursor;
    QString currentWord;
    QString precedingText;
    TriggerKind triggerKind;
    QChar triggerCharacter;
    int lineNumber;
    int columnNumber;
};

/**
 * @brief Abstract base for completion providers
 */
class CompletionProvider {
public:
    virtual ~CompletionProvider() = default;
    virtual QList<CompletionItem> completions(const CompletionContext& context) = 0;
    virtual int priority() const = 0;  // Lower = higher priority
    virtual QString name() const = 0;
};

/**
 * @brief Keyword completion provider
 */
class KeywordCompletionProvider : public CompletionProvider {
public:
    explicit KeywordCompletionProvider(const QStringList& keywords);
    QList<CompletionItem> completions(const CompletionContext& context) override;
    int priority() const override { return 10; }
    QString name() const override { return "Keywords"; }
    
    void setKeywords(const QStringList& keywords);
    
private:
    QStringList m_keywords;
};

/**
 * @brief Builtin function completion provider
 */
class BuiltinCompletionProvider : public CompletionProvider {
public:
    explicit BuiltinCompletionProvider(const QMap<QString, QString>& builtins);
    QList<CompletionItem> completions(const CompletionContext& context) override;
    int priority() const override { return 20; }
    QString name() const override { return "Builtins"; }
    
    void setBuiltins(const QMap<QString, QString>& builtins);
    
private:
    QMap<QString, QString> m_builtins;  // name -> signature
};

/**
 * @brief Symbol completion provider (variables, functions from current document)
 */
class SymbolCompletionProvider : public CompletionProvider {
public:
    QList<CompletionItem> completions(const CompletionContext& context) override;
    int priority() const override { return 30; }
    QString name() const override { return "Symbols"; }
    
    void updateSymbols(const QList<CompletionItem>& symbols);
    
private:
    QList<CompletionItem> m_symbols;
};

/**
 * @brief Intelligent code completer with LSP-like features for FluxScript
 * 
 * Features:
 * - Context-aware completion
 * - Multiple completion providers
 * - Type-aware suggestions
 * - Documentation tooltips
 * - Snippet support
 * - Priority-based sorting
 */
class Completer : public QCompleter {
    Q_OBJECT
    
public:
    explicit Completer(QObject* parent = nullptr);
    ~Completer() override;

    // ========================================================================
    // Provider Management
    // ========================================================================
    void addProvider(std::unique_ptr<CompletionProvider> provider);
    void removeProvider(const QString& name);
    CompletionProvider* provider(const QString& name) const;
    
    // ========================================================================
    // Completion Updates
    // ========================================================================
    void updateCompletions(const QStringList& additionalKeywords = {});
    void updateSymbols(const QList<CompletionItem>& symbols);
    void updateFunctions(const QMap<QString, QString>& functions);  // name -> signature
    
    // ========================================================================
    // FluxScript-Specific
    // ========================================================================
    void updateFluxKeywords();
    void updateFluxBuiltins();
    
    // ========================================================================
    // Context Analysis
    // ========================================================================
    CompletionContext analyzeContext(const QTextCursor& cursor);
    
    // ========================================================================
    // Documentation
    // ========================================================================
    QString documentationFor(const QString& completion) const;
    void setDocumentation(const QString& completion, const QString& docs);

public slots:
    void showCompletionPopup();
    void hideCompletionPopup();

signals:
    void completionSelected(const CompletionItem& item);
    void documentationRequested(const QString& completion);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // ========================================================================
    // Initialization
    // ========================================================================
    void setupModel();
    void setupPopup();
    void setupDefaultProviders();
    
    // ========================================================================
    // Completion Generation
    // ========================================================================
    QList<CompletionItem> generateCompletions(const CompletionContext& context);
    void populateModel(const QList<CompletionItem>& items);
    CompletionItem createKeywordItem(const QString& keyword);
    CompletionItem createBuiltinItem(const QString& name, const QString& signature);
    CompletionItem createSymbolItem(const CompletionItem& symbol);
    
    // ========================================================================
    // Members
    // ========================================================================
    QStandardItemModel* m_model;
    
    // Completion providers
    QMap<QString, CompletionProvider*> m_providers;
    std::vector<std::unique_ptr<CompletionProvider>> m_providerStorage;
    
    // Documentation cache
    QMap<QString, QString> m_documentation;
    
    // FluxScript keywords and builtins
    QStringList m_keywords;
    QMap<QString, QString> m_builtins;  // name -> signature
    QMap<QString, QString> m_functions; // name -> signature
    
    // State
    CompletionContext m_lastContext;
    bool m_isShowingCompletion;
};

} // namespace Flux

#endif // FLUX_COMPLETER_H
