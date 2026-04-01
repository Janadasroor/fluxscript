#ifndef FLUX_EDITOR_THEME_H
#define FLUX_EDITOR_THEME_H

#include <QObject>
#include <QString>
#include <QColor>
#include <QFont>
#include <QMap>
#include <memory>

namespace Flux {

/**
 * @brief Color scheme for syntax highlighting
 */
struct SyntaxColors {
    QColor keyword;
    QColor type;
    QColor function;
    QColor string;
    QColor number;
    QColor comment;
    QColor operator_color;
    QColor punctuation;
    QColor variable;
    QColor constant;
    QColor decorator;
    QColor preprocessor;
};

/**
 * @brief Color scheme for editor UI elements
 */
struct EditorColors {
    QColor background;
    QColor foreground;
    QColor selection;
    QColor selectionInactive;
    QColor currentLine;
    QColor lineNumber;
    QColor lineNumberActive;
    QColor gutterBackground;
    QColor gutterForeground;
    QColor gutterBorder;
    QColor cursor;
    QColor whitespace;
    QColor bracketMatch;
    QColor bracketMismatch;
    QColor indentGuide;
    QColor indentGuideActive;
};

/**
 * @brief Color scheme for diagnostic markers
 */
struct DiagnosticColors {
    QColor error;
    QColor errorBackground;
    QColor warning;
    QColor warningBackground;
    QColor info;
    QColor infoBackground;
    QColor hint;
    QColor hintBackground;
};

/**
 * @brief Color scheme for debugging elements
 */
struct DebugColors {
    QColor breakpoint;
    QColor breakpointDisabled;
    QColor executionLine;
    QColor executionLineBackground;
    QColor stackFrame;
    QColor stackFrameBackground;
};

/**
 * @brief Color scheme for minimap
 */
struct MinimapColors {
    QColor background;
    QColor viewport;
    QColor viewportBorder;
    QColor selection;
    QColor findResult;
};

/**
 * @brief Complete editor theme containing all color schemes
 */
struct EditorTheme {
    QString name;
    QString description;
    
    // Color schemes
    SyntaxColors syntax;
    EditorColors editor;
    DiagnosticColors diagnostic;
    DebugColors debug;
    MinimapColors minimap;
    
    // Font settings
    QString fontFamily;
    int fontSize;
    int lineSpacing;
    
    // Editor behavior
    int tabWidth;
    bool useSpacesForTabs;
    bool showWhitespace;
    bool highlightCurrentLine;
    bool highlightMatchingBrackets;
    
    // Validation
    bool isValid() const;
    
    // Factory methods
    static EditorTheme darkTheme();
    static EditorTheme lightTheme();
    static EditorTheme monokaiTheme();
    static EditorTheme solarizedDarkTheme();
    static EditorTheme solarizedLightTheme();
    static EditorTheme highContrastTheme();
};

/**
 * @brief Manages editor themes and provides theme application
 * 
 * Responsibilities:
 * - Load and save themes
 * - Apply themes to editor components
 * - Provide default themes
 * - Support custom themes
 */
class ThemeManager : public QObject {
    Q_OBJECT
    
public:
    explicit ThemeManager(QObject* parent = nullptr);
    ~ThemeManager() override;
    
    // ========================================================================
    // Theme Access
    // ========================================================================
    static ThemeManager* instance();
    
    EditorTheme currentTheme() const;
    void setCurrentTheme(const QString& themeName);
    
    QStringList availableThemes() const;
    bool hasTheme(const QString& themeName) const;
    EditorTheme theme(const QString& themeName) const;
    
    // ========================================================================
    // Theme Management
    // ========================================================================
    void registerTheme(const EditorTheme& theme);
    void unregisterTheme(const QString& themeName);
    void loadThemesFromDirectory(const QString& path);
    void saveTheme(const EditorTheme& theme, const QString& path);
    
    // ========================================================================
    // Theme Application
    // ========================================================================
    void applyThemeToEditor(class FluxEditor* editor);
    void applyThemeToGutter(class GutterWidget* gutter);
    void applyThemeToMinimap(class MinimapWidget* minimap);
    
    // ========================================================================
    // Color Utilities
    // ========================================================================
    QColor syntaxColor(const QString& category) const;
    QColor editorColor(const QString& element) const;
    QColor diagnosticColor(const QString& level) const;
    QColor debugColor(const QString& element) const;
    
    // ========================================================================
    // Font Utilities
    // ========================================================================
    QFont editorFont() const;
    QFont lineNumberFont() const;
    QFont gutterFont() const;
    
signals:
    void themeChanged(const QString& themeName);
    void themeRegistered(const QString& themeName);
    void themeUnregistered(const QString& themeName);
    
private:
    // ========================================================================
    // Internal Helpers
    // ========================================================================
    void initializeDefaultThemes();
    void loadUserThemes();
    void saveUserThemes();
    
    // ========================================================================
    // Members
    // ========================================================================
    QMap<QString, EditorTheme> m_themes;
    QString m_currentThemeName;
    
    static ThemeManager* s_instance;
};

} // namespace Flux

#endif // FLUX_EDITOR_THEME_H