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

#include "flux/ui/snippets.h"
#include <QTextCursor>
#include <QTextBlock>
#include <QRegularExpression>

namespace Flux {

SnippetManager::SnippetManager(QObject* parent)
    : QObject(parent)
{
    registerFluxSnippets();
}

SnippetManager::~SnippetManager()
{
}

void SnippetManager::registerSnippet(const Snippet& snippet)
{
    QString scope = snippet.scope.isEmpty() ? "all" : snippet.scope;
    m_snippets[scope].append(snippet);
}

void SnippetManager::registerFluxSnippets()
{
    // Function definition
    registerSnippet({
        "function", "func",
        "function $1($2) {\n    $0\n}",
        "Create a new function",
        "flux"
    });
    
    // If statement
    registerSnippet({
        "if", "if",
        "if ($1) {\n    $0\n}",
        "If statement",
        "flux"
    });
    
    // If-else
    registerSnippet({
        "if-else", "ife",
        "if ($1) {\n    $2\n} else {\n    $0\n}",
        "If-else statement",
        "flux"
    });
    
    // For loop
    registerSnippet({
        "for", "for",
        "for (let $1 = 0; $1 < $2; $1++) {\n    $0\n}",
        "For loop",
        "flux"
    });
    
    // While loop
    registerSnippet({
        "while", "while",
        "while ($1) {\n    $0\n}",
        "While loop",
        "flux"
    });
    
    // Variable declaration
    registerSnippet({
        "let", "let",
        "let $1 = $0;",
        "Variable declaration",
        "flux"
    });
    
    // Print statement
    registerSnippet({
        "print", "print",
        "print(\"$1\");",
        "Print to console",
        "flux"
    });
    
    // Println
    registerSnippet({
        "println", "println",
        "println(\"$1\");",
        "Print with newline",
        "flux"
    });
    
    // Return statement
    registerSnippet({
        "return", "ret",
        "return $0;",
        "Return statement",
        "flux"
    });
    
    // Matrix definition
    registerSnippet({
        "matrix", "mat",
        "matrix([[${1:1}, ${2:2}], [${3:3}, ${4:4}]]);",
        "Matrix literal",
        "flux"
    });
    
    // Vector definition
    registerSnippet({
        "vector", "vec",
        "vector([${1:1}, ${2:2}, ${3:3}]);",
        "Vector literal",
        "flux"
    });
    
    // Class definition
    registerSnippet({
        "class", "class",
        "class $1 {\n    constructor($2) {\n        $0\n    }\n}",
        "Class definition",
        "flux"
    });
    
    // Try-catch
    registerSnippet({
        "try-catch", "try",
        "try {\n    $1\n} catch ($2) {\n    $0\n}",
        "Try-catch block",
        "flux"
    });
    
    // Switch statement
    registerSnippet({
        "switch", "switch",
        "switch ($1) {\n    case $2:\n        $0\n        break;\n    default:\n        break;\n}",
        "Switch statement",
        "flux"
    });
    
    // Comment block
    registerSnippet({
        "comment", "/*",
        "/**\n * $1\n * \n * @description $2\n */",
        "Comment block",
        "flux"
    });
    
    // Main function
    registerSnippet({
        "main", "main",
        "function main() {\n    $0\n}\n\nmain();",
        "Main function template",
        "flux"
    });
}

QList<Snippet> SnippetManager::getSnippetsForPrefix(const QString& prefix) const
{
    QList<Snippet> result;
    
    for (const auto& scope : {"flux", "all"}) {
        if (m_snippets.contains(scope)) {
            for (const auto& snippet : m_snippets[scope]) {
                if (snippet.prefix.startsWith(prefix, Qt::CaseInsensitive)) {
                    result.append(snippet);
                }
            }
        }
    }
    
    return result;
}

bool SnippetManager::tryExpandSnippet(QPlainTextEdit* editor, const QString& prefix)
{
    auto snippets = getSnippetsForPrefix(prefix);
    if (snippets.isEmpty()) return false;
    
    // Use first matching snippet
    const auto& snippet = snippets.first();
    
    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();
    
    // Delete the prefix
    cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, prefix.length());
    cursor.removeSelectedText();
    
    // Insert snippet body (simplified - just text without tab stops)
    QString body = snippet.body;
    body.replace(QRegularExpression("\\$\\d+"), ""); // Remove tab stops
    cursor.insertText(body);
    
    cursor.endEditBlock();
    
    return true;
}

void SnippetManager::insertSnippetText(QPlainTextEdit* editor, const QString& body)
{
    QTextCursor cursor = editor->textCursor();
    cursor.insertText(body);
}

} // namespace Flux
