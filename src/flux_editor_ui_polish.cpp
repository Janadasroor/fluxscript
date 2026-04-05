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

#include "flux/flux_editor_ui_polish.h"

#include <QProgressBar>
#include <QApplication>
#include <QLabel>
#include <QVBoxLayout>
#include <QShortcut>

namespace Flux {

// ============================================================================
// ThemeManager Implementation
// ============================================================================

ThemeManager* ThemeManager::s_instance = nullptr;

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
    , m_currentTheme("dark")
{
}

ThemeManager* ThemeManager::instance()
{
    if (!s_instance) {
        s_instance = new ThemeManager();
    }
    return s_instance;
}

void ThemeManager::setTheme(const QString& themeName)
{
    m_currentTheme = themeName;
    loadTheme(themeName);
    Q_EMIT themeChanged(themeName);
}

QStringList ThemeManager::availableThemes() const
{
    return {"dark", "light", "monokai", "vioSpice"};
}

QColor ThemeManager::backgroundColor() const
{
    return m_colors.value("background", QColor(30, 30, 30));
}

QColor ThemeManager::textColor() const
{
    return m_colors.value("text", QColor(200, 200, 200));
}

QColor ThemeManager::accentColor() const
{
    return m_colors.value("accent", QColor(14, 99, 156));
}

QColor ThemeManager::errorColor() const
{
    return m_colors.value("error", QColor(244, 67, 54));
}

QColor ThemeManager::warningColor() const
{
    return m_colors.value("warning", QColor(255, 152, 0));
}

QColor ThemeManager::successColor() const
{
    return m_colors.value("success", QColor(76, 175, 80));
}

void ThemeManager::applyTheme(QWidget* widget)
{
    if (!widget || m_stylesheet.isEmpty()) {
        return;
    }
    widget->setStyleSheet(m_stylesheet);
}

void ThemeManager::applyThemeToApplication()
{
    qApp->setStyleSheet(m_stylesheet);
}

void ThemeManager::loadTheme(const QString& themeName)
{
    m_stylesheet = loadStyleSheet(themeName);
    // Would load colors from theme file
}

QString ThemeManager::loadStyleSheet(const QString& themeName)
{
    // Would load from resources or file
    return QString();
}

// ============================================================================
// ShortcutManager Implementation
// ============================================================================

ShortcutManager* ShortcutManager::s_instance = nullptr;

ShortcutManager::ShortcutManager(QObject* parent)
    : QObject(parent)
{
}

ShortcutManager* ShortcutManager::instance()
{
    if (!s_instance) {
        s_instance = new ShortcutManager();
    }
    return s_instance;
}

void ShortcutManager::registerShortcut(const QString& actionName, const QKeySequence& shortcut,
                                       const char* slot, QObject* receiver)
{
    m_shortcuts[actionName] = shortcut;
    QShortcut* s = new QShortcut(shortcut, receiver);
    // Use old-style connect for const char* slot
    QObject::connect(s, SIGNAL(activated()), receiver, slot);
}

void ShortcutManager::registerShortcut(const QString& actionName, const QKeySequence& shortcut,
                                       std::function<void()> callback)
{
    m_shortcuts[actionName] = shortcut;
    m_callbacks[actionName] = callback;
}

QKeySequence ShortcutManager::shortcut(const QString& actionName) const
{
    return m_shortcuts.value(actionName);
}

void ShortcutManager::setShortcut(const QString& actionName, const QKeySequence& shortcut)
{
    m_shortcuts[actionName] = shortcut;
    Q_EMIT shortcutChanged(actionName, shortcut);
}

void ShortcutManager::loadVioSpicePresets()
{
    // VioSpice-compatible shortcuts
    registerShortcut("Run", QKeySequence("F5"), nullptr, nullptr);
    registerShortcut("Stop", QKeySequence("Shift+F5"), nullptr, nullptr);
    registerShortcut("Step Over", QKeySequence("F10"), nullptr, nullptr);
    registerShortcut("Step Into", QKeySequence("F11"), nullptr, nullptr);
    registerShortcut("Toggle Breakpoint", QKeySequence("F9"), nullptr, nullptr);
    registerShortcut("Save", QKeySequence::Save, nullptr, nullptr);
    registerShortcut("Open", QKeySequence::Open, nullptr, nullptr);
}

void ShortcutManager::loadDefaultPresets()
{
    // Default editor shortcuts
    registerShortcut("Cut", QKeySequence::Cut, nullptr, nullptr);
    registerShortcut("Copy", QKeySequence::Copy, nullptr, nullptr);
    registerShortcut("Paste", QKeySequence::Paste, nullptr, nullptr);
    registerShortcut("Undo", QKeySequence::Undo, nullptr, nullptr);
    registerShortcut("Redo", QKeySequence::Redo, nullptr, nullptr);
    registerShortcut("Find", QKeySequence::Find, nullptr, nullptr);
    registerShortcut("Select All", QKeySequence::SelectAll, nullptr, nullptr);
}

void ShortcutManager::exportShortcuts(const QString& filePath)
{
    QSettings settings(filePath, QSettings::IniFormat);
    for (auto it = m_shortcuts.begin(); it != m_shortcuts.end(); ++it) {
        settings.setValue(it.key(), it.value().toString());
    }
}

void ShortcutManager::importShortcuts(const QString& filePath)
{
    QSettings settings(filePath, QSettings::IniFormat);
    for (const QString& key : settings.allKeys()) {
        m_shortcuts[key] = QKeySequence(settings.value(key).toString());
    }
}

// ============================================================================
// DockManager Implementation
// ============================================================================

DockManager::DockManager(QMainWindow* mainWindow)
    : QObject(mainWindow)
    , m_mainWindow(mainWindow)
{
    setupPresets();
}

void DockManager::saveLayout(const QString& name)
{
    m_layouts[name] = m_mainWindow->saveState();
}

void DockManager::restoreLayout(const QString& name)
{
    if (m_layouts.contains(name)) {
        m_mainWindow->restoreState(m_layouts[name]);
        Q_EMIT layoutRestored(name);
    }
}

void DockManager::resetLayout()
{
    restoreLayout("default");
}

QStringList DockManager::availableDocks() const
{
    return m_docks.keys();
}

QStringList DockManager::visibleDocks() const
{
    QStringList visible;
    for (auto it = m_docks.begin(); it != m_docks.end(); ++it) {
        if (it.value()->isVisible()) {
            visible.append(it.key());
        }
    }
    return visible;
}

void DockManager::setDockVisible(const QString& dockName, bool visible)
{
    if (m_docks.contains(dockName)) {
        m_docks[dockName]->setVisible(visible);
    }
}

bool DockManager::isDockVisible(const QString& dockName) const
{
    QDockWidget* dock = m_docks.value(dockName, nullptr);
    return dock ? dock->isVisible() : false;
}

void DockManager::applyPreset(const QString& preset)
{
    if (m_presets.contains(preset)) {
        const auto& presetDocks = m_presets[preset];
        for (auto it = m_docks.begin(); it != m_docks.end(); ++it) {
            it.value()->setVisible(presetDocks.value(it.key(), false));
        }
    }
}

QStringList DockManager::availablePresets() const
{
    return m_presets.keys();
}

void DockManager::setupPresets()
{
    // Coding preset
    QMap<QString, bool> codingPreset;
    codingPreset["AI Co-Pilot"] = true;
    codingPreset["ERC Violations"] = true;
    codingPreset["Debugger"] = false;
    codingPreset["Waveforms"] = false;
    m_presets["Coding"] = codingPreset;
    
    // Debugging preset
    QMap<QString, bool> debuggingPreset;
    debuggingPreset["AI Co-Pilot"] = false;
    debuggingPreset["ERC Violations"] = false;
    debuggingPreset["Debugger"] = true;
    debuggingPreset["Waveforms"] = true;
    m_presets["Debugging"] = debuggingPreset;
    
    // Simulation preset
    QMap<QString, bool> simulationPreset;
    simulationPreset["AI Co-Pilot"] = false;
    simulationPreset["ERC Violations"] = false;
    simulationPreset["Debugger"] = true;
    simulationPreset["Waveforms"] = true;
    simulationPreset["Simulation Control"] = true;
    m_presets["Simulation"] = simulationPreset;
}

// ============================================================================
// ExtendedStatusBar Implementation
// ============================================================================

ExtendedStatusBar::ExtendedStatusBar(QWidget* parent)
    : QStatusBar(parent)
{
    setupWidgets();
}

void ExtendedStatusBar::setupWidgets()
{
    m_cursorLabel = new QLabel("Line: 1, Col: 1");
    m_selectionLabel = new QLabel("");
    m_encodingLabel = new QLabel("UTF-8");
    m_languageLabel = new QLabel("FluxScript");
    m_gitLabel = new QLabel("");
    m_simulationLabel = new QLabel("Ready");
    // m_progressBar = new QProgressBar();
    // // m_progressBar->setMaximumWidth(200);
    // // m_progressBar->setVisible(false);
    
    addPermanentWidget(m_cursorLabel);
    addPermanentWidget(m_selectionLabel);
    addPermanentWidget(m_encodingLabel);
    addPermanentWidget(m_languageLabel);
    addPermanentWidget(m_gitLabel);
    addPermanentWidget(m_simulationLabel);
    // addPermanentWidget(m_progressBar);
}

void ExtendedStatusBar::setCursorPosition(int line, int column)
{
    m_cursorLabel->setText(QString("Line: %1, Col: %2").arg(line).arg(column));
}

void ExtendedStatusBar::setSelectionInfo(int lines, int characters)
{
    if (lines > 0 || characters > 0) {
        m_selectionLabel->setText(QString("Selected: %1 lines, %2 chars")
            .arg(lines).arg(characters));
    } else {
        m_selectionLabel->clear();
    }
}

void ExtendedStatusBar::setEncoding(const QString& encoding)
{
    m_encodingLabel->setText(encoding);
}

void ExtendedStatusBar::setLanguage(const QString& language)
{
    m_languageLabel->setText(language);
}

void ExtendedStatusBar::setGitStatus(const QString& branch, bool modified)
{
    if (branch.isEmpty()) {
        m_gitLabel->clear();
    } else {
        m_gitLabel->setText(QString("Git: %1%2")
            .arg(branch)
            .arg(modified ? " *" : ""));
    }
}

void ExtendedStatusBar::setSimulationStatus(const QString& status)
{
    m_simulationLabel->setText(status);
}

void ExtendedStatusBar::showProgress(const QString& message, int minimum, int maximum)
{
    // // m_progressBar->setVisible(true);
    // m_progressBar->setRange(minimum, maximum);
    showMessage(message, 0);
}

void ExtendedStatusBar::setProgressValue(int value)
{
    // m_progressBar->setValue(value);
}

void ExtendedStatusBar::hideProgress()
{
    // // m_progressBar->setVisible(false);
    clearMessage();
}

// ============================================================================
// CommandPalette Implementation
// ============================================================================

CommandPalette::CommandPalette(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
}

void CommandPalette::setupUI()
{
    // Stub implementation
}

void CommandPalette::registerCommand(const QString& name, const QString& description,
                                     std::function<void()> callback)
{
    // Stub implementation
}

void CommandPalette::onSearchTextChanged(const QString& text)
{
    Q_UNUSED(text);
}

void CommandPalette::onCommandSelected(QListWidgetItem* item)
{
    Q_UNUSED(item);
}

// ============================================================================
// BreadcrumbNavigation Implementation
// ============================================================================

BreadcrumbNavigation::BreadcrumbNavigation(QWidget* parent)
    : QWidget(parent)
{
    // Stub implementation
}

void BreadcrumbNavigation::setPath(const QString& path)
{
    Q_UNUSED(path);
}

QString BreadcrumbNavigation::currentPath() const
{
    return m_currentPath;
}

void BreadcrumbNavigation::updateBreadcrumb()
{
    // Stub implementation
}

void BreadcrumbNavigation::clearBreadcrumb()
{
    // Stub implementation
}

// ============================================================================
// EditorSettingsDialog Implementation
// ============================================================================

EditorSettingsDialog::EditorSettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    // Stub implementation
}

void EditorSettingsDialog::onCategoryChanged(int index)
{
    Q_UNUSED(index);
}

void EditorSettingsDialog::onApplyClicked()
{
    // Stub implementation
}

void EditorSettingsDialog::onOkClicked()
{
    // Stub implementation
}

void EditorSettingsDialog::onResetClicked()
{
    // Stub implementation
}

// ============================================================================
// QuickOpenDialog Implementation
// ============================================================================

QuickOpenDialog::QuickOpenDialog(QWidget* parent)
    : QDialog(parent)
{
    // Stub implementation
}

void QuickOpenDialog::onSearchTextChanged(const QString& text)
{
    Q_UNUSED(text);
}

void QuickOpenDialog::onFileDoubleClicked(QListWidgetItem* item)
{
    Q_UNUSED(item);
}

void QuickOpenDialog::onEnterPressed()
{
    // Stub implementation
}

// ============================================================================
// WelcomeScreen Implementation
// ============================================================================

WelcomeScreen::WelcomeScreen(QWidget* parent)
    : QDialog(parent)
{
    // Stub implementation
}

// ============================================================================
// SystemTrayManager Implementation
// ============================================================================

SystemTrayManager::SystemTrayManager(QObject* parent)
    : QObject(parent)
{
    // Stub implementation
}

} // namespace Flux

void WelcomeScreen::onOpenFile()
{
    // Stub
}

void WelcomeScreen::onNewFile()
{
    // Stub
}

void WelcomeScreen::onRecentFileClicked(const QString& file)
{
    Q_UNUSED(file);
}

void WelcomeScreen::onTemplateClicked(const QString& templateName)
{
    Q_UNUSED(templateName);
}

void WelcomeScreen::onShowOnStartupToggled(bool show)
{
    Q_UNUSED(show);
}
