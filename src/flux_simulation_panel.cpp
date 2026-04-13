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

#include "flux/flux_simulation_panel.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QListWidget>
#include <QListWidgetItem>
#include <QCheckBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QStyleOption>


namespace Flux {

// ============================================================================
// SimulationToolbar Implementation
// ============================================================================

SimulationToolbar::SimulationToolbar(QWidget* parent)
    : QToolBar(parent)
    , m_state(SimulationState::Stopped)
{
    setMovable(false);
    setFloatable(false);
    setIconSize(QSize(20, 20));

    // Define styles
    m_runStyle = "QPushButton { background: #4CAF50; color: white; border: none; padding: 6px 12px; border-radius: 3px; font-weight: bold; } "
                 "QPushButton:hover { background: #45a049; } "
                 "QPushButton:pressed { background: #3d8b40; }";
    
    m_pauseStyle = "QPushButton { background: #FF9800; color: white; border: none; padding: 6px 12px; border-radius: 3px; font-weight: bold; } "
                   "QPushButton:hover { background: #e68900; } "
                   "QPushButton:pressed { background: #cc7a00; }";
    
    m_stepStyle = "QPushButton { background: #2196F3; color: white; border: none; padding: 6px 12px; border-radius: 3px; font-weight: bold; } "
                  "QPushButton:hover { background: #1e88e5; } "
                  "QPushButton:pressed { background: #1976d2; }";
    
    m_stopStyle = "QPushButton { background: #f44336; color: white; border: none; padding: 6px 12px; border-radius: 3px; font-weight: bold; } "
                  "QPushButton:hover { background: #e53935; } "
                  "QPushButton:pressed { background: #d32f2f; }";
    
    m_disabledStyle = "QPushButton { background: #555555; color: #888888; border: none; padding: 6px 12px; border-radius: 3px; }";

    setupActions();
    updateButtonStates();
}

SimulationToolbar::~SimulationToolbar()
{
}

void SimulationToolbar::setupActions()
{
    // Run action
    m_runAction = new QAction(" Run", this);
    m_runAction->setShortcut(QKeySequence("F5"));
    m_runAction->setToolTip("Run simulation (F5)");
    connect(m_runAction, &QAction::triggered, this, &SimulationToolbar::onRunClicked);
    addAction(m_runAction);

    // Pause action
    m_pauseAction = new QAction(" Pause", this);
    m_pauseAction->setShortcut(QKeySequence("Ctrl+P"));
    m_pauseAction->setToolTip("Pause simulation (Ctrl+P)");
    connect(m_pauseAction, &QAction::triggered, this, &SimulationToolbar::onPauseClicked);
    addAction(m_pauseAction);

    // Step action
    m_stepAction = new QAction(" Step", this);
    m_stepAction->setShortcut(QKeySequence("F10"));
    m_stepAction->setToolTip("Step to next line (F10)");
    connect(m_stepAction, &QAction::triggered, this, &SimulationToolbar::onStepClicked);
    addAction(m_stepAction);

    addSeparator();

    // Stop action
    m_stopAction = new QAction(" Stop", this);
    m_stopAction->setShortcut(QKeySequence("Shift+F5"));
    m_stopAction->setToolTip("Stop simulation (Shift+F5)");
    connect(m_stopAction, &QAction::triggered, this, &SimulationToolbar::onStopClicked);
    addAction(m_stopAction);

    // Restart action
    m_restartAction = new QAction(" Restart", this);
    m_restartAction->setShortcut(QKeySequence("Ctrl+F5"));
    m_restartAction->setToolTip("Restart simulation (Ctrl+F5)");
    connect(m_restartAction, &QAction::triggered, this, &SimulationToolbar::onRestartClicked);
    addAction(m_restartAction);
}

void SimulationToolbar::updateButtonStates()
{
    bool running = (m_state == SimulationState::Running);
    bool paused = (m_state == SimulationState::Paused);
    bool stopped = (m_state == SimulationState::Stopped);

    m_runAction->setEnabled(stopped);
    m_pauseAction->setEnabled(running);
    m_stepAction->setEnabled(paused);
    m_stopAction->setEnabled(running || paused);
    m_restartAction->setEnabled(!stopped);

    // Update widget styles
    if (m_runAction->parentWidget()) {
        m_runAction->parentWidget()->setStyleSheet(running ? m_disabledStyle : m_runStyle);
    }
    if (m_pauseAction->parentWidget()) {
        m_pauseAction->parentWidget()->setStyleSheet(running ? m_pauseStyle : m_disabledStyle);
    }
    if (m_stepAction->parentWidget()) {
        m_stepAction->parentWidget()->setStyleSheet(paused ? m_stepStyle : m_disabledStyle);
    }
    if (m_stopAction->parentWidget()) {
        m_stopAction->parentWidget()->setStyleSheet((running || paused) ? m_stopStyle : m_disabledStyle);
    }
}

void SimulationToolbar::setSimulationState(SimulationState state)
{
    m_state = state;
    updateButtonStates();
}

void SimulationToolbar::setRunEnabled(bool enabled)
{
    m_runAction->setEnabled(enabled);
}

void SimulationToolbar::setStepEnabled(bool enabled)
{
    m_stepAction->setEnabled(enabled);
}

void SimulationToolbar::setPauseEnabled(bool enabled)
{
    m_pauseAction->setEnabled(enabled);
}

void SimulationToolbar::onRunClicked()
{
    Q_EMIT runRequested();
}

void SimulationToolbar::onPauseClicked()
{
    Q_EMIT pauseRequested();
}

void SimulationToolbar::onStepClicked()
{
    Q_EMIT stepRequested();
}

void SimulationToolbar::onStopClicked()
{
    Q_EMIT stopRequested();
}

void SimulationToolbar::onRestartClicked()
{
    Q_EMIT restartRequested();
}

QString SimulationToolbar::buttonStyle(bool enabled, bool active)
{
    Q_UNUSED(active);
    return enabled ? m_runStyle : m_disabledStyle;
}

// ============================================================================
// SimulationStatusWidget Implementation
// ============================================================================

SimulationStatusWidget::SimulationStatusWidget(QWidget* parent)
    : QWidget(parent)
    , m_state(SimulationState::Stopped)
    , m_elapsedTime(0)
    , m_currentLine(0)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // State label
    m_stateLabel = new QLabel("Stopped");
    m_stateLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    layout->addWidget(m_stateLabel);

    // Time label
    m_timeLabel = new QLabel("Time: 0.000s");
    layout->addWidget(m_timeLabel);

    // Line label
    m_lineLabel = new QLabel("Line: 0");
    layout->addWidget(m_lineLabel);

    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFormat("%p%");
    layout->addWidget(m_progressBar);

    // Message label
    m_messageLabel = new QLabel("Ready");
    m_messageLabel->setStyleSheet("color: #888888; font-size: 11px;");
    m_messageLabel->setWordWrap(true);
    layout->addWidget(m_messageLabel);

    updateDisplay();
}

void SimulationStatusWidget::setSimulationState(SimulationState state)
{
    m_state = state;
    updateDisplay();
}

void SimulationStatusWidget::setElapsedTime(double seconds)
{
    m_elapsedTime = seconds;
    updateDisplay();
}

void SimulationStatusWidget::setCurrentLine(int line)
{
    m_currentLine = line;
    updateDisplay();
}

void SimulationStatusWidget::setStatusMessage(const QString& message)
{
    m_message = message;
    updateDisplay();
}

void SimulationStatusWidget::updateDisplay()
{
    m_stateLabel->setText(stateToString(m_state));
    m_stateLabel->setStyleSheet(QString("font-weight: bold; font-size: 14px; color: %1;")
        .arg(stateColor(m_state).name()));

    m_timeLabel->setText(QString("Time: %1s").arg(m_elapsedTime, 0, 'f', 3));
    m_lineLabel->setText(QString("Line: %1").arg(m_currentLine));
    m_messageLabel->setText(m_message);

    // Update progress based on state
    if (m_state == SimulationState::Running) {
        m_progressBar->setFormat("Running...");
    } else if (m_state == SimulationState::Completed) {
        m_progressBar->setValue(100);
        m_progressBar->setFormat("Completed");
    } else {
        m_progressBar->setValue(0);
    }
}

QString SimulationStatusWidget::stateToString(SimulationState state) const
{
    switch (state) {
    case SimulationState::Stopped: return "Stopped";
    case SimulationState::Running: return "Running";
    case SimulationState::Paused: return "Paused";
    case SimulationState::Error: return "Error";
    case SimulationState::Completed: return "Completed";
    default: return "Unknown";
    }
}

QColor SimulationStatusWidget::stateColor(SimulationState state) const
{
    switch (state) {
    case SimulationState::Stopped: return QColor(150, 150, 150);
    case SimulationState::Running: return QColor(76, 175, 80);
    case SimulationState::Paused: return QColor(255, 152, 0);
    case SimulationState::Error: return QColor(244, 67, 54);
    case SimulationState::Completed: return QColor(33, 150, 243);
    default: return QColor(150, 150, 150);
    }
}

// ============================================================================
// SimulationPanel Implementation
// ============================================================================

SimulationPanel::SimulationPanel(QWidget* parent)
    : QDockWidget("Simulation Control", parent)
    , m_toolbar(nullptr)
    , m_statusWidget(nullptr)
    , m_timer(nullptr)
    , m_elapsedTime(0)
    , m_currentLine(0)
{
    setObjectName("SimulationPanel");
    setupUI();
    setupConnections();
}

SimulationPanel::~SimulationPanel()
{
}

void SimulationPanel::setupUI()
{
    m_centralWidget = new QWidget(this);
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Toolbar
    m_toolbar = new SimulationToolbar(m_centralWidget);
    m_mainLayout->addWidget(m_toolbar);

    // Status widget
    m_statusWidget = new SimulationStatusWidget(m_centralWidget);
    m_mainLayout->addWidget(m_statusWidget);

    m_centralWidget->setLayout(m_mainLayout);
    setWidget(m_centralWidget);

    // Timer for elapsed time
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        m_elapsedTime = m_elapsedTimer.elapsed() / 1000.0;
        m_statusWidget->setElapsedTime(m_elapsedTime);
    });
}

void SimulationPanel::setupConnections()
{
    connect(m_toolbar, &SimulationToolbar::runRequested,
            this, &SimulationPanel::startSimulation);
    connect(m_toolbar, &SimulationToolbar::pauseRequested,
            this, &SimulationPanel::pauseSimulation);
    connect(m_toolbar, &SimulationToolbar::stepRequested,
            this, &SimulationPanel::stepSimulation);
    connect(m_toolbar, &SimulationToolbar::stopRequested,
            this, &SimulationPanel::stopSimulation);
    connect(m_toolbar, &SimulationToolbar::restartRequested,
            this, &SimulationPanel::restartSimulation);
}

void SimulationPanel::startSimulation()
{
    m_elapsedTimer.start();
    m_elapsedTime = 0;
    m_currentLine = 0;
    startTimer();
    onSimulationStarted();
}

void SimulationPanel::pauseSimulation()
{
    stopTimer();
    m_toolbar->setSimulationState(SimulationState::Paused);
    m_statusWidget->setSimulationState(SimulationState::Paused);
    onSimulationPaused();
}

void SimulationPanel::resumeSimulation()
{
    startTimer();
    m_toolbar->setSimulationState(SimulationState::Running);
    m_statusWidget->setSimulationState(SimulationState::Running);
}

void SimulationPanel::stepSimulation()
{
    // Step to next line - would integrate with debugger
    m_currentLine++;
    m_statusWidget->setCurrentLine(m_currentLine);
    onSimulationPaused();
}

void SimulationPanel::stopSimulation()
{
    stopTimer();
    m_toolbar->setSimulationState(SimulationState::Stopped);
    m_statusWidget->setSimulationState(SimulationState::Stopped);
    onSimulationStopped();
}

void SimulationPanel::restartSimulation()
{
    stopSimulation();
    startSimulation();
}

void SimulationPanel::startTimer()
{
    m_timer->start(100);  // Update every 100ms
}

void SimulationPanel::stopTimer()
{
    m_timer->stop();
}

void SimulationPanel::onSimulationStarted()
{
    m_toolbar->setSimulationState(SimulationState::Running);
    m_statusWidget->setSimulationState(SimulationState::Running);
    m_statusWidget->setStatusMessage("Simulation running...");
    Q_EMIT simulationStateChanged(SimulationState::Running);
}

void SimulationPanel::onSimulationPaused()
{
    m_toolbar->setSimulationState(SimulationState::Paused);
    m_statusWidget->setSimulationState(SimulationState::Paused);
    m_statusWidget->setStatusMessage("Simulation paused");
    Q_EMIT simulationStateChanged(SimulationState::Paused);
}

void SimulationPanel::onSimulationStopped()
{
    m_toolbar->setSimulationState(SimulationState::Stopped);
    m_statusWidget->setSimulationState(SimulationState::Stopped);
    m_statusWidget->setStatusMessage("Simulation stopped");
    m_elapsedTime = 0;
    m_currentLine = 0;
    Q_EMIT simulationStateChanged(SimulationState::Stopped);
}

void SimulationPanel::onSimulationCompleted()
{
    stopTimer();
    m_toolbar->setSimulationState(SimulationState::Completed);
    m_statusWidget->setSimulationState(SimulationState::Completed);
    m_statusWidget->setStatusMessage(QString("Completed in %1s").arg(m_elapsedTime, 0, 'f', 3));
    Q_EMIT simulationStateChanged(SimulationState::Completed);
}

void SimulationPanel::onSimulationError(const QString& error)
{
    stopTimer();
    m_toolbar->setSimulationState(SimulationState::Error);
    m_statusWidget->setSimulationState(SimulationState::Error);
    m_statusWidget->setStatusMessage(error);
    Q_EMIT simulationStateChanged(SimulationState::Error);
}

void SimulationPanel::updateElapsedTime(double seconds)
{
    m_elapsedTime = seconds;
    m_statusWidget->setElapsedTime(seconds);
}

void SimulationPanel::updateCurrentLine(int line)
{
    m_currentLine = line;
    m_statusWidget->setCurrentLine(line);
}

void SimulationPanel::addWaveform(const WaveformData& waveform)
{
    m_waveforms[waveform.signalName] = waveform;
}

void SimulationPanel::clearWaveforms()
{
    m_waveforms.clear();
}

void SimulationPanel::addWaveformSignal(const QString& name, const QVector<double>& time, const QVector<double>& values)
{
    WaveformData waveform;
    waveform.signalName = name;
    waveform.timePoints = time;
    waveform.values = values;
    waveform.visible = true;
    waveform.color = QColor(Qt::GlobalColor((m_waveforms.size() % 7) + 2));  // Cycle through colors
    addWaveform(waveform);
}

// ============================================================================
// WaveformWidget Implementation
// ============================================================================

WaveformWidget::WaveformWidget(QWidget* parent)
    : QWidget(parent)
    , m_showCursors(false)
    , m_cursor1Time(0)
    , m_cursor2Time(0)
    , m_isDraggingCursor(false)
    , m_activeCursor(0)
    , m_autoScale(true)
    , m_xMin(0), m_xMax(1)
    , m_yMin(-1), m_yMax(1)
    , m_gridColor(60, 60, 60)
    , m_axisColor(120, 120, 120)
    , m_currentColorIndex(0)
{
    // Initialize waveform colors
    m_waveformColors[0] = QColor(255, 99, 132);   // Red
    m_waveformColors[1] = QColor(54, 162, 235);   // Blue
    m_waveformColors[2] = QColor(255, 206, 86);   // Yellow
    m_waveformColors[3] = QColor(75, 192, 192);   // Teal
    m_waveformColors[4] = QColor(153, 102, 255);  // Purple
    m_waveformColors[5] = QColor(255, 159, 64);   // Orange

    setMinimumSize(400, 200);
    setMouseTracking(true);
}

WaveformWidget::~WaveformWidget()
{
}

void WaveformWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Fill background
    painter.fillRect(rect(), QColor(30, 30, 30));
    
    // Draw grid
    drawGrid(painter);
    
    // Draw axes
    drawAxes(painter);
    
    // Draw waveforms
    drawWaveforms(painter);
    
    // Draw cursors
    if (m_showCursors) {
        drawCursors(painter);
    }
}

void WaveformWidget::drawGrid(QPainter& painter)
{
    painter.setPen(QPen(m_gridColor, 1));
    
    int gridSpacingX = width() / 10;
    int gridSpacingY = height() / 8;
    
    for (int x = 0; x <= width(); x += gridSpacingX) {
        painter.drawLine(x, 0, x, height());
    }
    for (int y = 0; y <= height(); y += gridSpacingY) {
        painter.drawLine(0, y, width(), y);
    }
}

void WaveformWidget::drawAxes(QPainter& painter)
{
    painter.setPen(QPen(m_axisColor, 2));
    
    // Draw X axis
    painter.drawLine(0, height() / 2, width(), height() / 2);
    
    // Draw Y axis
    painter.drawLine(0, 0, 0, height());
    
    // Draw labels
    painter.setPen(QColor(200, 200, 200));
    painter.drawText(5, 15, QString("Max: %1V").arg(m_yMax, 0, 'f', 2));
    painter.drawText(5, height() - 5, QString("Min: %1V").arg(m_yMin, 0, 'f', 2));
}

void WaveformWidget::drawWaveforms(QPainter& painter)
{
    for (auto it = m_waveformData.begin(); it != m_waveformData.end(); ++it) {
        const WaveformData& waveform = it.value();
        if (!waveform.visible) continue;
        
        int colorIndex = m_currentColorIndex % 6;
        QPen pen(m_waveformColors[colorIndex], 2);
        painter.setPen(pen);
        
        QPainterPath path;
        bool first = true;
        
        for (int i = 0; i < waveform.timePoints.size() && i < waveform.values.size(); ++i) {
            double t = waveform.timePoints[i];
            double v = waveform.values[i];
            
            int x = timeToPixel(t);
            int y = valueToPixel(v);
            
            if (first) {
                path.moveTo(x, y);
                first = false;
            } else {
                path.lineTo(x, y);
            }
        }
        
        painter.drawPath(path);
        m_currentColorIndex++;
    }
}

void WaveformWidget::drawCursors(QPainter& painter)
{
    QPen cursorPen(QColor(255, 255, 0), 1, Qt::DashLine);
    painter.setPen(cursorPen);
    
    int x1 = timeToPixel(m_cursor1Time);
    int x2 = timeToPixel(m_cursor2Time);
    
    painter.drawLine(x1, 0, x1, height());
    painter.drawLine(x2, 0, x2, height());
    
    // Draw cursor labels
    painter.setPen(QColor(255, 255, 0));
    painter.drawText(x1 + 5, 15, QString("T1: %1s").arg(m_cursor1Time, 0, 'f', 6));
    painter.drawText(x2 + 5, 30, QString("T2: %1s").arg(m_cursor2Time, 0, 'f', 6));
    painter.drawText(x2 + 5, 45, QString("T: %1s").arg(m_cursor2Time - m_cursor1Time, 0, 'f', 6));
}

double WaveformWidget::pixelToTime(int x) const
{
    return m_xMin + (x / static_cast<double>(width())) * (m_xMax - m_xMin);
}

double WaveformWidget::pixelToValue(int y) const
{
    return m_yMax - (y / static_cast<double>(height())) * (m_yMax - m_yMin);
}

int WaveformWidget::timeToPixel(double time) const
{
    return static_cast<int>((time - m_xMin) / (m_xMax - m_xMin) * width());
}

int WaveformWidget::valueToPixel(double value) const
{
    return static_cast<int>((m_yMax - value) / (m_yMax - m_yMin) * height());
}

void WaveformWidget::addWaveform(const WaveformData& waveform)
{
    m_waveformData[waveform.signalName] = waveform;
    update();
}

void WaveformWidget::removeWaveform(const QString& signalName)
{
    m_waveformData.remove(signalName);
    update();
}

void WaveformWidget::clearWaveforms()
{
    m_waveformData.clear();
    m_currentColorIndex = 0;
    update();
}

void WaveformWidget::updateWaveform(const QString& signalName, const QVector<double>& time, const QVector<double>& values)
{
    if (!m_waveformData.contains(signalName)) {
        WaveformData waveform;
        waveform.signalName = signalName;
        waveform.timePoints = time;
        waveform.values = values;
        waveform.visible = true;
        addWaveform(waveform);
        return;
    }

    WaveformData& waveform = m_waveformData[signalName];
    waveform.timePoints = time;
    waveform.values = values;
    update();
}

void WaveformWidget::setShowGrid(bool show)
{
    m_gridColor = show ? QColor(60, 60, 60) : QColor(30, 30, 30);
    update();
}

void WaveformWidget::setAutoScale(bool autoScale)
{
    m_autoScale = autoScale;
    if (autoScale) {
        fitToContents();
    }
    update();
}

void WaveformWidget::setXRange(double min, double max)
{
    m_autoScale = false;
    m_xMin = min;
    m_xMax = max;
    update();
}

void WaveformWidget::setYRange(double min, double max)
{
    m_autoScale = false;
    m_yMin = min;
    m_yMax = max;
    update();
}

void WaveformWidget::fitToContents()
{
    if (m_waveformData.isEmpty()) {
        return;
    }

    double minX = 0, maxX = 0;
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();

    for (const auto& waveform : m_waveformData) {
        if (!waveform.timePoints.isEmpty()) {
            minX = qMin(minX, waveform.timePoints.first());
            maxX = qMax(maxX, waveform.timePoints.last());
        }
        for (double v : waveform.values) {
            minY = qMin(minY, v);
            maxY = qMax(maxY, v);
        }
    }

    // Add some padding
    double xPad = (maxX - minX) * 0.05;
    double yPad = (maxY - minY) * 0.1;

    m_xMin = minX - xPad;
    m_xMax = maxX + xPad;
    m_yMin = minY - yPad;
    m_yMax = maxY + yPad;
    update();
}

void WaveformWidget::setShowCursors(bool show)
{
    m_showCursors = show;
    update();
}

void WaveformWidget::zoomIn()
{
    double center = (m_xMin + m_xMax) / 2;
    double range = (m_xMax - m_xMin) * 0.4;
    m_xMin = center - range;
    m_xMax = center + range;
    update();
}

void WaveformWidget::zoomOut()
{
    double center = (m_xMin + m_xMax) / 2;
    double range = (m_xMax - m_xMin) * 0.6;
    m_xMin = center - range;
    m_xMax = center + range;
    update();
}

void WaveformWidget::resetZoom()
{
    fitToContents();
}

void WaveformWidget::mousePressEvent(QMouseEvent* event)
{
    if (m_showCursors && event->button() == Qt::LeftButton) {
        int x = event->pos().x();
        double time = pixelToTime(x);

        // Check if clicking near cursor
        double cursor1X = timeToPixel(m_cursor1Time);
        double cursor2X = timeToPixel(m_cursor2Time);

        if (qAbs(x - cursor1X) < 10) {
            m_isDraggingCursor = true;
            m_activeCursor = 1;
        } else if (qAbs(x - cursor2X) < 10) {
            m_isDraggingCursor = true;
            m_activeCursor = 2;
        }
    }

    QWidget::mousePressEvent(event);
}

void WaveformWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isDraggingCursor) {
        double time = pixelToTime(event->pos().x());
        if (m_activeCursor == 1) {
            m_cursor1Time = time;
        } else {
            m_cursor2Time = time;
        }
        update();
        Q_EMIT cursorMoved(m_cursor1Time, m_cursor2Time, m_cursor2Time - m_cursor1Time);
    }

    QWidget::mouseMoveEvent(event);
}

void WaveformWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (m_showCursors) {
        m_showCursors = false;
        update();
    }
    QWidget::mouseDoubleClickEvent(event);
}

void WaveformWidget::wheelEvent(QWheelEvent* event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->angleDelta().y() > 0) {
            zoomIn();
        } else {
            zoomOut();
        }
        event->accept();
    } else {
        QWidget::wheelEvent(event);
    }
}

void WaveformWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
}

// ============================================================================
// WaveformDock Implementation
// ============================================================================

WaveformDock::WaveformDock(QWidget* parent)
    : QDockWidget("Waveforms", parent)
{
    setObjectName("WaveformDock");
    setupUI();
    setupConnections();
}

WaveformDock::~WaveformDock()
{
}

void WaveformDock::setupUI()
{
    m_centralWidget = new QWidget(this);
    m_mainLayout = new QVBoxLayout(m_centralWidget);
    m_mainLayout->setContentsMargins(0, 0, 0, 0);
    m_mainLayout->setSpacing(0);

    // Toolbar
    m_toolbar = new QToolBar(m_centralWidget);
    m_toolbar->setMovable(false);
    
    m_addButton = new QPushButton("Add");
    m_removeButton = new QPushButton("Remove");
    m_clearButton = new QPushButton("Clear");
    m_fitButton = new QPushButton("Fit");
    
    m_toolbar->addWidget(m_addButton);
    m_toolbar->addWidget(m_removeButton);
    m_toolbar->addWidget(m_clearButton);
    m_toolbar->addWidget(m_fitButton);
    
    m_mainLayout->addWidget(m_toolbar);

    // Signal list
    m_signalList = new QListWidget();
    m_signalList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_mainLayout->addWidget(m_signalList);

    // Waveform widget
    m_waveformWidget = new WaveformWidget(m_centralWidget);
    m_waveformWidget->setMinimumHeight(200);
    m_mainLayout->addWidget(m_waveformWidget);

    m_centralWidget->setLayout(m_mainLayout);
    setWidget(m_centralWidget);
}

void WaveformDock::setupConnections()
{
    connect(m_addButton, &QPushButton::clicked,
            this, &WaveformDock::addSelectedSignals);
    connect(m_removeButton, &QPushButton::clicked,
            this, [this]() {
                auto* item = m_signalList->currentItem();
                if (item) {
                    m_waveformWidget->removeWaveform(item->text());
                }
            });
    connect(m_clearButton, &QPushButton::clicked,
            this, [this]() {
                m_waveformWidget->clearWaveforms();
            });
    connect(m_fitButton, &QPushButton::clicked,
            this, [this]() {
                m_waveformWidget->fitToContents();
            });
}

void WaveformDock::addWaveform(const WaveformData& waveform)
{
    m_waveformWidget->addWaveform(waveform);
}

void WaveformDock::removeWaveform(const QString& signalName)
{
    m_waveformWidget->removeWaveform(signalName);
}

void WaveformDock::clearWaveforms()
{
    m_waveformWidget->clearWaveforms();
}

void WaveformDock::updateWaveform(const QString& signalName, const QVector<double>& time, const QVector<double>& values)
{
    m_waveformWidget->updateWaveform(signalName, time, values);
}

QStringList WaveformDock::availableSignals() const
{
    return m_availableSignals;
}

void WaveformDock::setAvailableSignals(const QStringList& signalList)
{
    m_availableSignals = signalList;
    updateSignalList();
}

void WaveformDock::updateSignalList()
{
    m_signalList->clear();
    for (const QString& signal : m_availableSignals) {
        auto* item = new QListWidgetItem(signal);
        item->setCheckState(Qt::Unchecked);
        m_signalList->addItem(item);
    }
}

void WaveformDock::onSimulationCompleted()
{
    // Auto-add all signals or prompt user
}

void WaveformDock::addSelectedSignals()
{
    auto items = m_signalList->selectedItems();
    for (auto* item : items) {
        Q_EMIT signalsSelected(QStringList() << item->text());
    }
}

// ============================================================================
// MeasurementWidget Implementation
// ============================================================================

MeasurementWidget::MeasurementWidget(QWidget* parent)
    : QWidget(parent)
    , m_t1(0), m_t2(0), m_delta(0)
    , m_v1(0), m_v2(0)
    , m_min(0), m_max(0), m_avg(0), m_rms(0), m_freq(0)
{
    setupUI();
}

void MeasurementWidget::setupUI()
{
    m_layout = new QGridLayout(this);

    // Cursor measurements group
    auto* cursorGroup = new QGroupBox("Cursor Measurements");
    auto* cursorLayout = new QFormLayout(cursorGroup);

    m_t1Label = new QLabel("T1: 0.000s");
    m_t2Label = new QLabel("T2: 0.000s");
    m_deltaLabel = new QLabel("T: 0.000s");
    m_v1Label = new QLabel("V1: 0.000V");
    m_v2Label = new QLabel("V2: 0.000V");

    cursorLayout->addRow("T1:", m_t1Label);
    cursorLayout->addRow("T2:", m_t2Label);
    cursorLayout->addRow("T:", m_deltaLabel);
    cursorLayout->addRow("V1:", m_v1Label);
    cursorLayout->addRow("V2:", m_v2Label);

    m_layout->addWidget(cursorGroup, 0, 0);

    // Statistics group
    auto* statsGroup = new QGroupBox("Statistics");
    auto* statsLayout = new QFormLayout(statsGroup);

    m_minLabel = new QLabel("Min: 0.000V");
    m_maxLabel = new QLabel("Max: 0.000V");
    m_avgLabel = new QLabel("Avg: 0.000V");
    m_rmsLabel = new QLabel("RMS: 0.000V");
    m_freqLabel = new QLabel("Freq: 0.000Hz");

    statsLayout->addRow("Min:", m_minLabel);
    statsLayout->addRow("Max:", m_maxLabel);
    statsLayout->addRow("Avg:", m_avgLabel);
    statsLayout->addRow("RMS:", m_rmsLabel);
    statsLayout->addRow("Freq:", m_freqLabel);

    m_layout->addWidget(statsGroup, 0, 1);
}

void MeasurementWidget::setCursorMeasurements(double t1, double t2, double delta, double v1, double v2)
{
    m_t1 = t1;
    m_t2 = t2;
    m_delta = delta;
    m_v1 = v1;
    m_v2 = v2;
    updateDisplay();
}

void MeasurementWidget::setWaveformStatistics(const QString& signal, double min, double max, double avg, double rms, double freq)
{
    Q_UNUSED(signal);
    m_min = min;
    m_max = max;
    m_avg = avg;
    m_rms = rms;
    m_freq = freq;
    updateDisplay();
}

void MeasurementWidget::clearMeasurements()
{
    m_t1 = m_t2 = m_delta = 0;
    m_v1 = m_v2 = 0;
    m_min = m_max = m_avg = m_rms = m_freq = 0;
    updateDisplay();
}

void MeasurementWidget::updateDisplay()
{
    m_t1Label->setText(QString("T1: %1s").arg(m_t1, 0, 'f', 3));
    m_t2Label->setText(QString("T2: %2s").arg(m_t2, 0, 'f', 3));
    m_deltaLabel->setText(QString("T: %1s").arg(m_delta, 0, 'f', 3));
    m_v1Label->setText(QString("V1: %1V").arg(m_v1, 0, 'f', 3));
    m_v2Label->setText(QString("V2: %1V").arg(m_v2, 0, 'f', 3));

    m_minLabel->setText(QString("Min: %1V").arg(m_min, 0, 'f', 3));
    m_maxLabel->setText(QString("Max: %1V").arg(m_max, 0, 'f', 3));
    m_avgLabel->setText(QString("Avg: %1V").arg(m_avg, 0, 'f', 3));
    m_rmsLabel->setText(QString("RMS: %1V").arg(m_rms, 0, 'f', 3));
    m_freqLabel->setText(QString("Freq: %1Hz").arg(m_freq, 0, 'f', 3));
}

} // namespace Flux
