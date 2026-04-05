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

#ifndef FLUX_COMMAND_PALETTE_H
#define FLUX_COMMAND_PALETTE_H

#include <QWidget>
#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QMap>
#include <QAction>

namespace Flux {

/**
 * @brief Command entry in the palette
 */
struct CommandEntry {
    QString name;
    QString description;
    QString shortcut;
    std::function<void()> callback;
    int category; // 0=Edit, 1=View, 2=File, 3=Debug, 4=Run

    bool operator==(const CommandEntry& other) const { return name == other.name; }
};

/**
 * @brief VS Code-style command palette
 * 
 * Activated with Ctrl+Shift+P, provides fuzzy search for all editor commands.
 */
class CommandPalette : public QDialog {
    Q_OBJECT

public:
    explicit CommandPalette(QWidget* parent = nullptr);
    ~CommandPalette() override;

    void registerCommand(const CommandEntry& cmd);
    void registerCommand(const QString& name, const QString& shortcut, std::function<void()> callback, int category = 0);
    void showPalette();

protected:
    void keyPressEvent(QKeyEvent* event) override;

private Q_SLOTS:
    void onTextChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);

private:
    void filterCommands(const QString& query);
    int fuzzyMatchScore(const QString& pattern, const QString& text);

    QLineEdit* m_searchEdit;
    QListWidget* m_commandList;
    QVBoxLayout* m_layout;
    
    QList<CommandEntry> m_allCommands;
    QList<CommandEntry> m_filteredCommands;
};

} // namespace Flux

#endif // FLUX_COMMAND_PALETTE_H
