#ifndef FLUX_OUTPUT_PANEL_H
#define FLUX_OUTPUT_PANEL_H

#include <QWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <QProcess>
#include <QPointer>
#include <QMap>

namespace Flux {

/**
 * @brief Output panel for displaying build, run, and debug output
 */
class OutputPanel : public QWidget {
    Q_OBJECT

public:
    explicit OutputPanel(QWidget* parent = nullptr);
    ~OutputPanel() override;

    void clear();
    void appendText(const QString& text, bool isError = false);
    void appendOutput(const QString& text);
    void appendError(const QString& text);
    void appendWarning(const QString& text);
    void appendInfo(const QString& text);
    void appendSuccess(const QString& text);

    bool isRunning() const { return m_process && m_process->state() == QProcess::Running; }
    void stop();

public Q_SLOTS:
    void runScript(const QString& scriptPath);
    void runCommand(const QString& command, const QStringList& args = {});

Q_SIGNALS:
    void processStarted();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void outputAvailable(const QString& text);

private Q_SLOTS:
    void onProcessReadyReadStandardOutput();
    void onProcessReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onClearClicked();
    void onCategoryChanged(int index);

private:
    void setupUI();
    void setupConnections();
    QString timestamp() const;
    void ensureVisible();

    QTextEdit* m_outputEdit;
    QComboBox* m_categoryCombo;
    QToolButton* m_clearBtn;
    QToolButton* m_stopBtn;
    QToolButton* m_scrollBtn;
    
    QProcess* m_process;
    bool m_autoScroll;
    bool m_showTimestamps;
    
    QMap<QString, QColor> m_colors;
};

} // namespace Flux

#endif // FLUX_OUTPUT_PANEL_H
