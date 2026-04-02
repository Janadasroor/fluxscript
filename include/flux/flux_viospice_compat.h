#ifndef FLUX_VIOSPICE_COMPAT_H
#define FLUX_VIOSPICE_COMPAT_H

/**
 * @file flux_viospice_compat.h
 * @brief VioSpice compatibility layer for FluxScript editor
 *
 * This header provides the integration layer between FluxScript editor
 * and VioSpice EDA, enabling seamless schematic-driven simulation automation.
 */

#include "flux/ui/flux_editor.h"
#include "flux/ui/flux_highlighter.h"
#include "flux/ui/flux_completer.h"

#ifdef FLUX_EDITOR_VIOSPICE_COMPAT
#define HAVE_FLUXSCRIPT
#endif

namespace Flux {

/**
 * @brief VioSpice-specific code editor with schematic integration
 *
 * This class extends FluxEditor with VioSpice-specific features:
 * - Schematic context awareness (QGraphicsScene)
 * - Netlist management integration (NetManager)
 * - SPICE directive syntax highlighting
 * - SmartSignal API completion
 * - Simulation control hooks
 */
class VioSpiceEditor : public FluxEditor {
    Q_OBJECT

public:
    explicit VioSpiceEditor(QWidget* parent = nullptr);
    ~VioSpiceEditor() override;

    /**
     * @brief Set the schematic scene context
     * @param scene Pointer to QGraphicsScene (schematic canvas)
     */
    void setSchematicScene(void* scene);

    /**
     * @brief Set the netlist manager context
     * @param netManager Pointer to NetManager
     */
    void setNetlistManager(void* netManager);

    /**
     * @brief Initialize VioSpice-specific editor features
     */
    void initializeVioSpiceFeatures();

    /**
     * @brief Load FluxScript from VioSpice schematic JSON
     * @param jsonContent The JSON content of the schematic file
     * @return true if script was successfully extracted
     */
    bool loadFromSchematicJson(const QString& jsonContent);

    /**
     * @brief Generate FluxScript from schematic elements
     * @param scene The schematic scene
     * @param netManager The netlist manager
     * @return Generated FluxScript code
     */
    static QString generateFromSchematic(void* scene, void* netManager);

    /**
     * @brief Embed FluxScript into schematic JSON
     * @param jsonContent The schematic JSON content
     * @param script The FluxScript code to embed
     * @return Updated JSON content with embedded script
     */
    static QString embedInSchematicJson(const QString& jsonContent, const QString& script);

public Q_SLOTS:
    /**
     * @brief Handle run request from VioSpice
     * Compiles and executes the FluxScript via JIT
     */
    void onVioSpiceRunRequested();

    /**
     * @brief Update error display from ERC
     * @param errors Map of line number to error message
     */
    void displayErcErrors(const QMap<int, QString>& errors);

    /**
     * @brief Highlight simulation probe point
     * @param line Line number to highlight
     */
    void highlightProbePoint(int line);

Q_SIGNALS:
    /**
     * @brief Emitted when script execution starts
     */
    void simulationStarted();

    /**
     * @brief Emitted when script execution completes
     */
    void simulationCompleted();

    /**
     * @brief Emitted when script execution pauses (breakpoint)
     */
    void simulationPaused();

    /**
     * @brief Emitted when script execution stops
     */
    void simulationStopped();

    /**
     * @brief Emitted when waveform data is available
     * @param waveforms Map of signal name to waveform data
     */
    void waveformsAvailable(const QMap<QString, QVector<double>>& waveforms);

protected:
    void setupVioSpiceConnections();
    void setupVioSpiceShortcuts();

private:
    bool m_vioSpiceInitialized;
    QString m_lastScriptHash;
};

/**
 * @brief VioSpice-specific syntax highlighter
 *
 * Extends FluxScript highlighting with:
 * - SPICE directive highlighting
 * - Element line highlighting
 * - SmartSignal API highlighting
 */
class VioSpiceHighlighter : public Highlighter {
public:
    explicit VioSpiceHighlighter(QTextDocument* parent = nullptr);

    /**
     * @brief Initialize VioSpice-specific highlighting rules
     */
    void initializeVioSpiceRules();
};

/**
 * @brief VioSpice-specific code completer
 *
 * Extends FluxScript completion with:
 * - SmartSignal API keywords
 * - SPICE directive completion
 * - Component model completion
 */
class VioSpiceCompleter : public Completer {
public:
    explicit VioSpiceCompleter(QObject* parent = nullptr);

    /**
     * @brief Initialize VioSpice-specific completion keywords
     */
    void initializeVioSpiceCompletions();
};

} // namespace Flux

#endif // FLUX_VIOSPICE_COMPAT_H
