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

#ifndef FLUX_SIMULATION_PANEL_H
#define FLUX_SIMULATION_PANEL_H

/**
 * @file flux_simulation_panel.h
 * @brief Simulation control and visualization components for VioSpice integration
 */

#include <QWidget>
#include <QDockWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QMap>
#include <QVector>
#include <QPainter>
#include <QPainterPath>
#include <QDateTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QListWidget>
#include <QStringList>

namespace Flux {

/**
 * @brief Simulation state enumeration
 */
enum class SimulationState {
    Stopped,
    Running,
    Paused,
    Error,
    Completed
};

/**
 * @brief Waveform data structure
 */
struct WaveformData {
    QString signalName;
    QVector<double> timePoints;
    QVector<double> values;
    QColor color;
    bool visible;
};

/**
 * @brief Simulation control toolbar
 *
 * Provides run/stop/step controls for simulation execution
 */
class SimulationToolbar : public QToolBar {
    Q_OBJECT

public:
    explicit SimulationToolbar(QWidget* parent = nullptr);
    ~SimulationToolbar() override;

    // State management
    void setSimulationState(SimulationState state);
    SimulationState currentState() const { return m_state; }

    // Enable/disable controls
    void setRunEnabled(bool enabled);
    void setStepEnabled(bool enabled);
    void setPauseEnabled(bool enabled);

Q_SIGNALS:
    void runRequested();
    void pauseRequested();
    void stepRequested();
    void stopRequested();
    void restartRequested();

private Q_SLOTS:
    void onRunClicked();
    void onPauseClicked();
    void onStepClicked();
    void onStopClicked();
    void onRestartClicked();

private:
    void setupActions();
    void updateButtonStates();
    QString buttonStyle(bool enabled, bool active = false);

    SimulationState m_state;
    
    // Actions
    QAction* m_runAction;
    QAction* m_pauseAction;
    QAction* m_stepAction;
    QAction* m_stopAction;
    QAction* m_restartAction;

    // Style sheets
    QString m_runStyle;
    QString m_pauseStyle;
    QString m_stepStyle;
    QString m_stopStyle;
    QString m_disabledStyle;
};

/**
 * @brief Status display for simulation
 */
class SimulationStatusWidget : public QWidget {
    Q_OBJECT

public:
    explicit SimulationStatusWidget(QWidget* parent = nullptr);

    void setSimulationState(SimulationState state);
    void setElapsedTime(double seconds);
    void setCurrentLine(int line);
    void setStatusMessage(const QString& message);

private:
    void updateDisplay();
    QString stateToString(SimulationState state) const;
    QColor stateColor(SimulationState state) const;

    QLabel* m_stateLabel;
    QLabel* m_timeLabel;
    QLabel* m_lineLabel;
    QLabel* m_messageLabel;
    QProgressBar* m_progressBar;

    SimulationState m_state;
    double m_elapsedTime;
    int m_currentLine;
    QString m_message;
};

/**
 * @brief Main simulation control panel
 *
 * Combines toolbar, status, and controls in a dockable panel
 */
class SimulationPanel : public QDockWidget {
    Q_OBJECT

public:
    explicit SimulationPanel(QWidget* parent = nullptr);
    ~SimulationPanel() override;

    // State access
    SimulationState currentState() const { return m_toolbar->currentState(); }
    double elapsedTime() const { return m_elapsedTime; }
    int currentLine() const { return m_currentLine; }

    // Waveform data
    void addWaveform(const WaveformData& waveform);
    void clearWaveforms();
    const QMap<QString, WaveformData>& waveforms() const { return m_waveforms; }

public Q_SLOTS:
    void startSimulation();
    void pauseSimulation();
    void resumeSimulation();
    void stepSimulation();
    void stopSimulation();
    void restartSimulation();

    void onSimulationStarted();
    void onSimulationPaused();
    void onSimulationStopped();
    void onSimulationCompleted();
    void onSimulationError(const QString& error);

    void updateElapsedTime(double seconds);
    void updateCurrentLine(int line);
    void addWaveformSignal(const QString& name, const QVector<double>& time, const QVector<double>& values);

Q_SIGNALS:
    void simulationStateChanged(SimulationState state);
    void runTriggered();
    void pauseTriggered();
    void stepTriggered();
    void stopTriggered();
    void restartTriggered();

private:
    void setupUI();
    void setupConnections();
    void startTimer();
    void stopTimer();

    SimulationToolbar* m_toolbar;
    SimulationStatusWidget* m_statusWidget;
    
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;

    QTimer* m_timer;
    QElapsedTimer m_elapsedTimer;
    double m_elapsedTime;
    int m_currentLine;

    QMap<QString, WaveformData> m_waveforms;
};

/**
 * @brief Waveform visualization widget
 *
 * Displays simulation waveforms using QPainter
 */
class WaveformWidget : public QWidget {
    Q_OBJECT

public:
    explicit WaveformWidget(QWidget* parent = nullptr);
    ~WaveformWidget() override;

    // Waveform management
    void addWaveform(const WaveformData& waveform);
    void removeWaveform(const QString& signalName);
    void clearWaveforms();
    void updateWaveform(const QString& signalName, const QVector<double>& time, const QVector<double>& values);

    // Display options
    void setShowGrid(bool show);
    void setAutoScale(bool autoScale);
    void setXRange(double min, double max);
    void setYRange(double min, double max);

    // Cursors
    void setShowCursors(bool show);
    double cursor1Time() const { return m_cursor1Time; }
    double cursor2Time() const { return m_cursor2Time; }
    double cursorTimeDifference() const { return m_cursor2Time - m_cursor1Time; }

public Q_SLOTS:
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void fitToContents();

Q_SIGNALS:
    void cursorMoved(double time1, double time2, double delta);
    void waveformClicked(const QString& signalName, double time, double value);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void drawWaveforms(QPainter& painter);
    void drawGrid(QPainter& painter);
    void drawCursors(QPainter& painter);
    void drawAxes(QPainter& painter);
    double pixelToTime(int x) const;
    double pixelToValue(int y) const;
    int timeToPixel(double time) const;
    int valueToPixel(double value) const;

    QMap<QString, WaveformData> m_waveformData;

    bool m_showCursors;
    double m_cursor1Time;
    double m_cursor2Time;
    bool m_isDraggingCursor;
    int m_activeCursor;

    bool m_autoScale;
    double m_xMin, m_xMax;
    double m_yMin, m_yMax;
    
    QColor m_gridColor;
    QColor m_axisColor;
    QColor m_waveformColors[6];
    int m_currentColorIndex;
};

/**
 * @brief Waveform dock widget with controls
 */
class WaveformDock : public QDockWidget {
    Q_OBJECT

public:
    explicit WaveformDock(QWidget* parent = nullptr);
    ~WaveformDock() override;

    // Waveform management
    void addWaveform(const WaveformData& waveform);
    void removeWaveform(const QString& signalName);
    void clearWaveforms();
    void updateWaveform(const QString& signalName, const QVector<double>& time, const QVector<double>& values);

    // Signal list
    QStringList availableSignals() const;
    void setAvailableSignals(const QStringList& signalList);

public Q_SLOTS:
    void onSimulationCompleted();
    void addSelectedSignals();

Q_SIGNALS:
    void signalsSelected(const QStringList& selectedSignals);
    void waveformVisibilityChanged(const QString& signal, bool visible);

private:
    void setupUI();
    void setupConnections();
    void updateSignalList();

    WaveformWidget* m_waveformWidget;
    
    QWidget* m_centralWidget;
    QVBoxLayout* m_mainLayout;
    
    QListWidget* m_signalList;
    QToolBar* m_toolbar;
    
    QPushButton* m_addButton;
    QPushButton* m_removeButton;
    QPushButton* m_clearButton;
    QPushButton* m_fitButton;

    QStringList m_availableSignals;
    QStringList m_selectedSignals;
};

/**
 * @brief Measurement display widget
 *
 * Shows cursor measurements and waveform statistics
 */
class MeasurementWidget : public QWidget {
    Q_OBJECT

public:
    explicit MeasurementWidget(QWidget* parent = nullptr);

    // Update measurements
    void setCursorMeasurements(double t1, double t2, double delta, double v1, double v2);
    void setWaveformStatistics(const QString& signal, double min, double max, double avg, double rms, double freq);
    void clearMeasurements();

private:
    void setupUI();
    void updateDisplay();

    QGridLayout* m_layout;
    
    // Cursor measurements
    QLabel* m_t1Label;
    QLabel* m_t2Label;
    QLabel* m_deltaLabel;
    QLabel* m_v1Label;
    QLabel* m_v2Label;
    
    // Statistics
    QLabel* m_minLabel;
    QLabel* m_maxLabel;
    QLabel* m_avgLabel;
    QLabel* m_rmsLabel;
    QLabel* m_freqLabel;

    double m_t1, m_t2, m_delta;
    double m_v1, m_v2;
    double m_min, m_max, m_avg, m_rms, m_freq;
};

} // namespace Flux

#endif // FLUX_SIMULATION_PANEL_H
