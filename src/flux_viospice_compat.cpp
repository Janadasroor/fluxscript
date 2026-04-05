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

#include "flux/flux_viospice_compat.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QShortcut>

namespace Flux {

// ============================================================================
// VioSpiceEditor Implementation
// ============================================================================

VioSpiceEditor::VioSpiceEditor(QWidget* parent)
    : FluxEditor(parent)
    , m_vioSpiceInitialized(false)
{
    initializeVioSpiceFeatures();
}

VioSpiceEditor::~VioSpiceEditor()
{
}

void VioSpiceEditor::initializeVioSpiceFeatures()
{
    if (m_vioSpiceInitialized) {
        return;
    }

    // Setup VioSpice-specific highlighting
    // Note: Would need to cast to VioSpiceHighlighter if it existed
    
    // Setup VioSpice-specific completion
    // Note: Would need to cast to VioSpiceCompleter if it existed

    setupVioSpiceConnections();
    setupVioSpiceShortcuts();

    m_vioSpiceInitialized = true;
}

void VioSpiceEditor::setSchematicScene(void* scene)
{
    setScene(scene);
}

void VioSpiceEditor::setNetlistManager(void* netManager)
{
    setNetManager(netManager);
}

void VioSpiceEditor::setupVioSpiceConnections()
{
    // Connect to simulation signals
    connect(this, &VioSpiceEditor::simulationCompleted,
            this, [this]() {
                // Update UI after simulation completes
            });

    connect(this, &VioSpiceEditor::simulationStopped,
            this, [this]() {
                // Clean up after simulation stops
            });
}

void VioSpiceEditor::setupVioSpiceShortcuts()
{
    // Ctrl+Shift+R for run simulation
    auto* runShortcut = new QShortcut(QKeySequence("Ctrl+Shift+R"), this);
    connect(runShortcut, &QShortcut::activated,
            this, &VioSpiceEditor::onVioSpiceRunRequested);

    // F9 for toggle breakpoint
    auto* breakpointShortcut = new QShortcut(QKeySequence("F9"), this);
    connect(breakpointShortcut, &QShortcut::activated,
            this, [this]() {
                int line = textCursor().blockNumber() + 1;
                toggleBreakpoint(line);
            });

    // Ctrl+Shift+S for step simulation
    auto* stepShortcut = new QShortcut(QKeySequence("Ctrl+Shift+S"), this);
    connect(stepShortcut, &QShortcut::activated,
            this, [this]() {
                Q_EMIT simulationPaused();
            });
}

bool VioSpiceEditor::loadFromSchematicJson(const QString& jsonContent)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonContent.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return false;
    }

    QJsonObject root = doc.object();

    // Extract embedded FluxScript
    if (root.contains("fluxScript")) {
        QString script = root["fluxScript"].toString();
        setPlainText(script);
        return true;
    }

    // Also check for SPICE directives
    if (root.contains("spiceDirectives")) {
        QJsonArray directives = root["spiceDirectives"].toArray();
        QString directiveText;
        for (const QJsonValue& directive : directives) {
            directiveText += directive.toString() + "\n";
        }
        setPlainText(directiveText);
        return true;
    }

    return false;
}

QString VioSpiceEditor::generateFromSchematic(void* scene, void* netManager)
{
    Q_UNUSED(scene);
    Q_UNUSED(netManager);

    // Template code
    QString code = R"(# FluxScript generated from schematic
def main():
    print("Starting simulation...")
    return 0

main()
)";

    return code;
}

QString VioSpiceEditor::embedInSchematicJson(const QString& jsonContent, const QString& script)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonContent.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        return jsonContent;
    }

    QJsonObject root = doc.object();
    root["fluxScript"] = script;

    QJsonDocument newDoc(root);
    return QString::fromUtf8(newDoc.toJson());
}

void VioSpiceEditor::onVioSpiceRunRequested()
{
    Q_EMIT simulationStarted();
    Q_EMIT simulationCompleted();
}

void VioSpiceEditor::displayErcErrors(const QMap<int, QString>& errors)
{
    setErrorLines(errors);
}

void VioSpiceEditor::highlightProbePoint(int line)
{
    setActiveDebugLine(line);
    goToLine(line);
}

} // namespace Flux
