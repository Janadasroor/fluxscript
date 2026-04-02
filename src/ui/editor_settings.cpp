#include "flux/ui/editor_settings.h"
#include "flux/ui/flux_highlighter.h"

#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFontDatabase>
#include <QApplication>

namespace Flux {

EditorSettings* EditorSettings::s_instance = nullptr;

// ============================================================================
// EditorSettings Implementation
// ============================================================================

EditorSettings::EditorSettings(QObject* parent)
    : QObject(parent)
    , m_fontFamily("Fira Code")
    , m_fontSize(12)
    , m_tabWidth(4)
    , m_useSpacesForTabs(true)
    , m_showLineNumbers(true)
    , m_showMinimap(true)
    , m_showWhitespace(false)
    , m_highlightCurrentLine(true)
    , m_highlightMatchingBrackets(true)
    , m_showFoldMarkers(true)
    , m_minimapWidth(120)
    , m_autoSave(false)
    , m_autoSaveInterval(60)
    , m_confirmOnClose(true)
    , m_confirmOnDiscard(true)
    , m_restoreLastSession(true)
    , m_maxRecentFiles(10)
    , m_enableCodeCompletion(true)
    , m_completionTriggerDelay(200)
    , m_enableSnippets(true)
    , m_enableFolding(true)
    , m_currentTheme("dark")
    , m_enableSemanticHighlighting(true)
    , m_syntaxHighlightDelay(100)
    , m_enableDiagnostics(true)
    , m_showErrorsInline(true)
    , m_maxFileSize(10)
{
    s_instance = this;
    setupDefaults();
    ensureSettingsFile();
    load();
}

EditorSettings::~EditorSettings()
{
    save();
    s_instance = nullptr;
}

EditorSettings* EditorSettings::instance()
{
    if (!s_instance) {
        s_instance = new EditorSettings();
    }
    return s_instance;
}

void EditorSettings::setupDefaults()
{
    // Default values are set in constructor
}

void EditorSettings::ensureSettingsFile()
{
    QString settingsDir = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);

    if (!QDir(settingsDir).exists()) {
        QDir().mkpath(settingsDir);
    }
}

QSettings* EditorSettings::createSettings() const
{
    QString settingsPath = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/settings.ini";

    return new QSettings(settingsPath, QSettings::IniFormat);
}

void EditorSettings::applyFont()
{
    QFont font(m_fontFamily, m_fontSize);
    font.setStyleHint(QFont::Monospace);
    QApplication::setFont(font);
}

QString EditorSettings::fontFamily() const
{
    return m_fontFamily;
}

int EditorSettings::fontSize() const
{
    return m_fontSize;
}

QFont EditorSettings::font() const
{
    QFont font(m_fontFamily, m_fontSize);
    font.setStyleHint(QFont::Monospace);
    return font;
}

int EditorSettings::tabWidth() const
{
    return m_tabWidth;
}

bool EditorSettings::useSpacesForTabs() const
{
    return m_useSpacesForTabs;
}

int EditorSettings::indentWidth() const
{
    return m_tabWidth;
}

bool EditorSettings::showLineNumbers() const
{
    return m_showLineNumbers;
}

bool EditorSettings::showMinimap() const
{
    return m_showMinimap;
}

bool EditorSettings::showWhitespace() const
{
    return m_showWhitespace;
}

bool EditorSettings::highlightCurrentLine() const
{
    return m_highlightCurrentLine;
}

bool EditorSettings::highlightMatchingBrackets() const
{
    return m_highlightMatchingBrackets;
}

bool EditorSettings::showFoldMarkers() const
{
    return m_showFoldMarkers;
}

bool EditorSettings::showBookmarkMarkers() const
{
    return true;
}

int EditorSettings::minimapWidth() const
{
    return m_minimapWidth;
}

bool EditorSettings::autoSave() const
{
    return m_autoSave;
}

int EditorSettings::autoSaveInterval() const
{
    return m_autoSaveInterval;
}

bool EditorSettings::confirmOnClose() const
{
    return m_confirmOnClose;
}

bool EditorSettings::confirmOnDiscard() const
{
    return m_confirmOnDiscard;
}

bool EditorSettings::restoreLastSession() const
{
    return m_restoreLastSession;
}

int EditorSettings::maxRecentFiles() const
{
    return m_maxRecentFiles;
}

bool EditorSettings::enableCodeCompletion() const
{
    return m_enableCodeCompletion;
}

int EditorSettings::completionTriggerDelay() const
{
    return m_completionTriggerDelay;
}

bool EditorSettings::enableSnippets() const
{
    return m_enableSnippets;
}

bool EditorSettings::enableFolding() const
{
    return m_enableFolding;
}

QString EditorSettings::currentTheme() const
{
    return m_currentTheme;
}

EditorTheme EditorSettings::loadTheme(const QString& themeName)
{
    if (themeName == "dark") {
        return EditorTheme::darkTheme();
    } else if (themeName == "light") {
        return EditorTheme::lightTheme();
    } else if (themeName == "monokai") {
        return EditorTheme::monokaiTheme();
    }

    // Try to load custom theme from settings
    std::unique_ptr<QSettings> settings(createSettings());
    settings->beginGroup("Themes/" + themeName);

    EditorTheme theme;
    theme.name = themeName;

    // Load colors from settings
    theme.backgroundColor = settings->value("backgroundColor", QColor(30, 30, 30)).value<QColor>();
    theme.textColor = settings->value("textColor", QColor(200, 200, 200)).value<QColor>();
    theme.keywordColor = settings->value("keywordColor", QColor(197, 134, 192)).value<QColor>();
    theme.builtinColor = settings->value("builtinColor", QColor(229, 192, 123)).value<QColor>();
    theme.functionColor = settings->value("functionColor", QColor(97, 175, 239)).value<QColor>();
    theme.numberColor = settings->value("numberColor", QColor(181, 206, 168)).value<QColor>();
    theme.stringColor = settings->value("stringColor", QColor(206, 145, 120)).value<QColor>();
    theme.commentColor = settings->value("commentColor", QColor(90, 100, 110)).value<QColor>();

    settings->endGroup();

    return theme;
}

void EditorSettings::saveTheme(const QString& themeName, const EditorTheme& theme)
{
    std::unique_ptr<QSettings> settings(createSettings());
    settings->beginGroup("Themes/" + themeName);

    settings->setValue("name", theme.name);
    settings->setValue("backgroundColor", theme.backgroundColor);
    settings->setValue("textColor", theme.textColor);
    settings->setValue("keywordColor", theme.keywordColor);
    settings->setValue("builtinColor", theme.builtinColor);
    settings->setValue("functionColor", theme.functionColor);
    settings->setValue("numberColor", theme.numberColor);
    settings->setValue("stringColor", theme.stringColor);
    settings->setValue("commentColor", theme.commentColor);

    settings->endGroup();
    settings->sync();
}

QStringList EditorSettings::availableThemes() const
{
    QStringList themes = { "dark", "light", "monokai" };

    // Load custom themes from settings
    std::unique_ptr<QSettings> settings(const_cast<EditorSettings*>(this)->createSettings());
    settings->beginGroup("Themes");
    QStringList customThemes = settings->childGroups();
    settings->endGroup();

    themes.append(customThemes);
    return themes;
}

bool EditorSettings::enableSemanticHighlighting() const
{
    return m_enableSemanticHighlighting;
}

int EditorSettings::syntaxHighlightDelay() const
{
    return m_syntaxHighlightDelay;
}

bool EditorSettings::enableDiagnostics() const
{
    return m_enableDiagnostics;
}

bool EditorSettings::showErrorsInline() const
{
    return m_showErrorsInline;
}

int EditorSettings::maxFileSize() const
{
    return m_maxFileSize;
}

QString EditorSettings::settingsFilePath() const
{
    return QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation) + "/settings.ini";
}

void EditorSettings::load()
{
    std::unique_ptr<QSettings> settings(createSettings());

    settings->beginGroup("Editor");

    // Font settings
    m_fontFamily = settings->value("fontFamily", m_fontFamily).toString();
    m_fontSize = settings->value("fontSize", m_fontSize).toInt();

    // Indentation
    m_tabWidth = settings->value("tabWidth", m_tabWidth).toInt();
    m_useSpacesForTabs = settings->value("useSpacesForTabs", m_useSpacesForTabs).toBool();

    // Display
    m_showLineNumbers = settings->value("showLineNumbers", m_showLineNumbers).toBool();
    m_showMinimap = settings->value("showMinimap", m_showMinimap).toBool();
    m_showWhitespace = settings->value("showWhitespace", m_showWhitespace).toBool();
    m_highlightCurrentLine = settings->value("highlightCurrentLine", m_highlightCurrentLine).toBool();
    m_highlightMatchingBrackets = settings->value("highlightMatchingBrackets", m_highlightMatchingBrackets).toBool();
    m_showFoldMarkers = settings->value("showFoldMarkers", m_showFoldMarkers).toBool();
    m_minimapWidth = settings->value("minimapWidth", m_minimapWidth).toInt();

    // Behavior
    m_autoSave = settings->value("autoSave", m_autoSave).toBool();
    m_autoSaveInterval = settings->value("autoSaveInterval", m_autoSaveInterval).toInt();
    m_confirmOnClose = settings->value("confirmOnClose", m_confirmOnClose).toBool();
    m_confirmOnDiscard = settings->value("confirmOnDiscard", m_confirmOnDiscard).toBool();
    m_restoreLastSession = settings->value("restoreLastSession", m_restoreLastSession).toBool();
    m_maxRecentFiles = settings->value("maxRecentFiles", m_maxRecentFiles).toInt();
    m_enableCodeCompletion = settings->value("enableCodeCompletion", m_enableCodeCompletion).toBool();
    m_completionTriggerDelay = settings->value("completionTriggerDelay", m_completionTriggerDelay).toInt();
    m_enableSnippets = settings->value("enableSnippets", m_enableSnippets).toBool();
    m_enableFolding = settings->value("enableFolding", m_enableFolding).toBool();

    // Theme
    m_currentTheme = settings->value("currentTheme", m_currentTheme).toString();

    // Advanced
    m_enableSemanticHighlighting = settings->value("enableSemanticHighlighting", m_enableSemanticHighlighting).toBool();
    m_syntaxHighlightDelay = settings->value("syntaxHighlightDelay", m_syntaxHighlightDelay).toInt();
    m_enableDiagnostics = settings->value("enableDiagnostics", m_enableDiagnostics).toBool();
    m_showErrorsInline = settings->value("showErrorsInline", m_showErrorsInline).toBool();
    m_maxFileSize = settings->value("maxFileSize", m_maxFileSize).toInt();

    settings->endGroup();

    applyFont();
}

void EditorSettings::save()
{
    std::unique_ptr<QSettings> settings(createSettings());

    settings->beginGroup("Editor");

    // Font settings
    settings->setValue("fontFamily", m_fontFamily);
    settings->setValue("fontSize", m_fontSize);

    // Indentation
    settings->setValue("tabWidth", m_tabWidth);
    settings->setValue("useSpacesForTabs", m_useSpacesForTabs);

    // Display
    settings->setValue("showLineNumbers", m_showLineNumbers);
    settings->setValue("showMinimap", m_showMinimap);
    settings->setValue("showWhitespace", m_showWhitespace);
    settings->setValue("highlightCurrentLine", m_highlightCurrentLine);
    settings->setValue("highlightMatchingBrackets", m_highlightMatchingBrackets);
    settings->setValue("showFoldMarkers", m_showFoldMarkers);
    settings->setValue("minimapWidth", m_minimapWidth);

    // Behavior
    settings->setValue("autoSave", m_autoSave);
    settings->setValue("autoSaveInterval", m_autoSaveInterval);
    settings->setValue("confirmOnClose", m_confirmOnClose);
    settings->setValue("confirmOnDiscard", m_confirmOnDiscard);
    settings->setValue("restoreLastSession", m_restoreLastSession);
    settings->setValue("maxRecentFiles", m_maxRecentFiles);
    settings->setValue("enableCodeCompletion", m_enableCodeCompletion);
    settings->setValue("completionTriggerDelay", m_completionTriggerDelay);
    settings->setValue("enableSnippets", m_enableSnippets);
    settings->setValue("enableFolding", m_enableFolding);

    // Theme
    settings->setValue("currentTheme", m_currentTheme);

    // Advanced
    settings->setValue("enableSemanticHighlighting", m_enableSemanticHighlighting);
    settings->setValue("syntaxHighlightDelay", m_syntaxHighlightDelay);
    settings->setValue("enableDiagnostics", m_enableDiagnostics);
    settings->setValue("showErrorsInline", m_showErrorsInline);
    settings->setValue("maxFileSize", m_maxFileSize);

    settings->endGroup();
    settings->sync();
}

void EditorSettings::resetToDefaults()
{
    m_fontFamily = "Fira Code";
    m_fontSize = 12;
    m_tabWidth = 4;
    m_useSpacesForTabs = true;
    m_showLineNumbers = true;
    m_showMinimap = true;
    m_showWhitespace = false;
    m_highlightCurrentLine = true;
    m_highlightMatchingBrackets = true;
    m_showFoldMarkers = true;
    m_minimapWidth = 120;
    m_autoSave = false;
    m_autoSaveInterval = 60;
    m_confirmOnClose = true;
    m_confirmOnDiscard = true;
    m_restoreLastSession = true;
    m_maxRecentFiles = 10;
    m_enableCodeCompletion = true;
    m_completionTriggerDelay = 200;
    m_enableSnippets = true;
    m_enableFolding = true;
    m_currentTheme = "dark";
    m_enableSemanticHighlighting = true;
    m_syntaxHighlightDelay = 100;
    m_enableDiagnostics = true;
    m_showErrorsInline = true;
    m_maxFileSize = 10;

    applyFont();
    emit settingsChanged();
}

void EditorSettings::setFontFamily(const QString& family)
{
    if (m_fontFamily != family) {
        m_fontFamily = family;
        applyFont();
        emit fontChanged();
        emit settingsChanged();
        save();
    }
}

void EditorSettings::setFontSize(int size)
{
    if (m_fontSize != size) {
        m_fontSize = size;
        applyFont();
        emit fontChanged();
        emit settingsChanged();
        save();
    }
}

void EditorSettings::setTabWidth(int width)
{
    if (m_tabWidth != width) {
        m_tabWidth = width;
        emit settingsChanged();
        save();
    }
}

void EditorSettings::setUseSpacesForTabs(bool use)
{
    if (m_useSpacesForTabs != use) {
        m_useSpacesForTabs = use;
        emit settingsChanged();
        save();
    }
}

void EditorSettings::setShowLineNumbers(bool show)
{
    if (m_showLineNumbers != show) {
        m_showLineNumbers = show;
        emit settingsChanged();
        save();
    }
}

void EditorSettings::setShowMinimap(bool show)
{
    if (m_showMinimap != show) {
        m_showMinimap = show;
        emit settingsChanged();
        save();
    }
}

void EditorSettings::setShowWhitespace(bool show)
{
    if (m_showWhitespace != show) {
        m_showWhitespace = show;
        emit settingsChanged();
        save();
    }
}

void EditorSettings::setHighlightCurrentLine(bool highlight)
{
    if (m_highlightCurrentLine != highlight) {
        m_highlightCurrentLine = highlight;
        emit settingsChanged();
        save();
    }
}

void EditorSettings::setHighlightMatchingBrackets(bool highlight)
{
    if (m_highlightMatchingBrackets != highlight) {
        m_highlightMatchingBrackets = highlight;
        emit settingsChanged();
        save();
    }
}

void EditorSettings::setAutoSave(bool enabled)
{
    if (m_autoSave != enabled) {
        m_autoSave = enabled;
        emit settingsChanged();
        save();
    }
}

void EditorSettings::setAutoSaveInterval(int seconds)
{
    if (m_autoSaveInterval != seconds) {
        m_autoSaveInterval = seconds;
        emit settingsChanged();
        save();
    }
}

void EditorSettings::setCurrentTheme(const QString& theme)
{
    if (m_currentTheme != theme) {
        m_currentTheme = theme;
        emit themeChanged();
        emit settingsChanged();
        save();
    }
}

void EditorSettings::setEnableCodeCompletion(bool enabled)
{
    if (m_enableCodeCompletion != enabled) {
        m_enableCodeCompletion = enabled;
        emit settingsChanged();
        save();
    }
}

void EditorSettings::setEnableFolding(bool enabled)
{
    if (m_enableFolding != enabled) {
        m_enableFolding = enabled;
        emit settingsChanged();
        save();
    }
}

} // namespace Flux
