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

#ifndef FLUX_SNIPPETS_H
#define FLUX_SNIPPETS_H

#include <QObject>
#include <QString>
#include <QMap>
#include <QList>
#include <QPlainTextEdit>

namespace Flux {

/**
 * @brief Code snippet definition with tab stops
 */
struct Snippet {
    QString name;
    QString prefix;
    QString body;  // Use $1, $2 for tab stops, $0 for final position
    QString description;
    QString scope; // "flux", "all", etc.
};

/**
 * @brief Code snippets manager
 * 
 * Provides snippet expansion with tab stops like VS Code.
 */
class SnippetManager : public QObject {
    Q_OBJECT

public:
    explicit SnippetManager(QObject* parent = nullptr);
    ~SnippetManager() override;

    // Register snippets
    void registerSnippet(const Snippet& snippet);
    void registerFluxSnippets();
    
    // Get snippets by prefix
    QList<Snippet> getSnippetsForPrefix(const QString& prefix) const;
    
    // Expand snippet at cursor
    bool tryExpandSnippet(QPlainTextEdit* editor, const QString& prefix);

private:
    void insertSnippetText(QPlainTextEdit* editor, const QString& body);
    
    QMap<QString, QList<Snippet>> m_snippets;
};

} // namespace Flux

#endif // FLUX_SNIPPETS_H
