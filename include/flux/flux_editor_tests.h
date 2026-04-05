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

#ifndef FLUX_EDITOR_TESTS_H
#define FLUX_EDITOR_TESTS_H

/**
 * @file flux_editor_tests.h
 * @brief Test framework for FluxScript editor
 * 
 * Phase 4: Polish & Optimization - Testing
 */

#include <QObject>
#include <QTest>
#include <QTemporaryFile>
#include <QDir>
#include <QSignalSpy>
#include <QScopedPointer>

namespace Flux {

class FluxEditor;
class Highlighter;
class Completer;
class DocumentManager;

/**
 * @brief Base test class for editor components
 */
class EditorTestBase : public QObject {
    Q_OBJECT

protected Q_SLOTS:
    void init();
    void cleanup();

protected:
    template<typename T>
    T* createEditor() {
        return new T();
    }
    
    QString createTempFile(const QString& content, const QString& suffix = ".flux");
    QString loadTestFile(const QString& testName);
    void compareWithGolden(const QString& actual, const QString& goldenTestName);
};

/**
 * @brief Unit tests for FluxEditor
 */
class FluxEditorTests : public EditorTestBase {
    Q_OBJECT

private Q_SLOTS:
    // Basic functionality
    void testConstructor();
    void testDestructor();
    void testSetPlainText();
    void testToPlainText();
    
    // Navigation
    void testGoToLine();
    void testGoToBeginning();
    void testGoToEnd();
    void testCursorPosition();
    
    // Selection
    void testSelectAll();
    void testSelectLine();
    void testSelectWord();
    void testCopyPaste();
    
    // Editing
    void testInsertText();
    void testDeleteText();
    void testUndoRedo();
    void testModificationTracking();
    
    // Folding
    void testToggleFold();
    void testFoldAll();
    void testUnfoldAll();
    
    // Breakpoints
    void testToggleBreakpoint();
    void testBreakpointLines();
    void testClearBreakpoints();
    
    // Search
    void testFindAndHighlight();
    void testFindNext();
    void testFindPrevious();
    void testCaseSensitiveSearch();
    
    // Context menu
    void testContextMenuEvent();
    void testContextMenuActions();
    
    // Performance
    void testLargeFileLoading();
    void testScrollingPerformance();
    void testHighlightingPerformance();
};

/**
 * @brief Unit tests for Highlighter
 */
class HighlighterTests : public EditorTestBase {
    Q_OBJECT

private Q_SLOTS:
    // Basic highlighting
    void testConstructor();
    void testKeywordHighlighting();
    void testBuiltinHighlighting();
    void testNumberHighlighting();
    void testStringHighlighting();
    void testCommentHighlighting();
    
    // Advanced highlighting
    void testFunctionHighlighting();
    void testTypeHighlighting();
    void testOperatorHighlighting();
    void testMultilineStringHighlighting();
    void testMultilineCommentHighlighting();
    
    // SPICE directives
    void testSpiceDirectiveHighlighting();
    void testElementLineHighlighting();
    void testSubcktHighlighting();
    
    // Theme
    void testSetTheme();
    void testDarkTheme();
    void testLightTheme();
    
    // Performance
    void testIncrementalHighlighting();
    void testLargeFileHighlighting();
    void testHighlightingSpeed();
};

/**
 * @brief Unit tests for Completer
 */
class CompleterTests : public EditorTestBase {
    Q_OBJECT

private Q_SLOTS:
    // Basic completion
    void testConstructor();
    void testKeywordCompletion();
    void testBuiltinCompletion();
    void testContextAwareCompletion();
    
    // Providers
    void testAddProvider();
    void testRemoveProvider();
    void testProviderPriority();
    
    // VioSpice keywords
    void testVioSpiceKeywords();
    void testSmartSignalKeywords();
    void testSpiceDirectiveKeywords();
    
    // Performance
    void testCompletionSpeed();
    void testCompletionCaching();
    void testLargeCompletionList();
};

/**
 * @brief Unit tests for DocumentManager
 */
class DocumentManagerTests : public EditorTestBase {
    Q_OBJECT

private Q_SLOTS:
    // Document operations
    void testOpenDocument();
    void testCloseDocument();
    void testSaveDocument();
    void testSaveAsDocument();
    
    // Multiple documents
    void testMultipleDocuments();
    void testSwitchDocuments();
    void testCloseAllDocuments();
    
    // File watching
    void testFileChangeDetection();
    void testExternalModification();
    
    // Recent files
    void testRecentFilesTracking();
    void testMaxRecentFiles();
    
    // Session
    void testSaveSession();
    void testLoadSession();
    void testRestoreSession();
    
    // Dirty state
    void testDirtyTracking();
    void testSaveModifiedDocuments();
};

/**
 * @brief Unit tests for Simulation Panel
 */
class SimulationPanelTests : public EditorTestBase {
    Q_OBJECT

private Q_SLOTS:
    // Toolbar
    void testToolbarCreation();
    void testRunButton();
    void testPauseButton();
    void testStepButton();
    void testStopButton();
    
    // State management
    void testStateTransitions();
    void testElapsedTimeTracking();
    void testCurrentLineTracking();
    
    // Waveforms
    void testAddWaveform();
    void testRemoveWaveform();
    void testUpdateWaveform();
    void testMultipleWaveforms();
    
    // Measurements
    void testCursorMeasurements();
    void testTimeDifference();
    void testVoltageMeasurement();
};

/**
 * @brief Unit tests for Debugger Panel
 */
class DebuggerPanelTests : public EditorTestBase {
    Q_OBJECT

private Q_SLOTS:
    // Breakpoints
    void testAddBreakpoint();
    void testRemoveBreakpoint();
    void testBreakpointConditions();
    void testBreakpointHitCount();
    
    // Variables
    void testAddVariable();
    void testUpdateVariable();
    void testVariableChangeDetection();
    
    // Call stack
    void testSetCallStack();
    void testFrameSelection();
    void testStackNavigation();
    
    // Stepping
    void testStepOver();
    void testStepInto();
    void testStepOut();
};

/**
 * @brief Unit tests for AI Co-Pilot
 */
class AICopilotTests : public EditorTestBase {
    Q_OBJECT

private Q_SLOTS:
    // Panel
    void testPanelCreation();
    void testApiKeyConfiguration();
    void testModelConfiguration();
    
    // Chat
    void testSendMessage();
    void testReceiveResponse();
    void testChatHistory();
    void testClearChat();
    
    // Code generation
    void testCodeGenerationRequest();
    void testCodeExtraction();
    void testCodeInsertion();
    
    // ERC assistance
    void testERCFixRequest();
    void testFixApplication();
};

/**
 * @brief Unit tests for LTspice Compatibility
 */
class LTspiceCompatibilityTests : public EditorTestBase {
    Q_OBJECT

private Q_SLOTS:
    // Detection
    void testLTspiceDirectiveDetection();
    void testLTspiceFunctionDetection();
    void testBehavioralSourceDetection();
    
    // Conversion
    void testBufConversion();
    void testInvConversion();
    void testUrampConversion();
    void testLimitConversion();
    void testIdtmodConversion();
    
    // Compatibility check
    void testCompatibilityCheck();
    void testWarningGeneration();
    void testSuggestionGeneration();
};

/**
 * @brief Integration tests for editor workflow
 */
class EditorIntegrationTests : public EditorTestBase {
    Q_OBJECT

private Q_SLOTS:
    // Complete workflow
    void testNewFileWorkflow();
    void testOpenFileWorkflow();
    void testEditSaveWorkflow();
    
    // Simulation workflow
    void testSimulationWorkflow();
    void testDebuggingWorkflow();
    void testWaveformVisualization();
    
    // AI workflow
    void testAICodeGenerationWorkflow();
    void testERCFixWorkflow();
    
    // File format
    void testVioSpiceSchematicImport();
    void testVioSpiceSchematicExport();
    void testRoundTripConversion();
};

/**
 * @brief Regression tests for known issues
 */
class RegressionTests : public EditorTestBase {
    Q_OBJECT

private Q_SLOTS:
    // Performance regressions
    void testNoMemoryLeaks();
    void testScrollingSmoothness();
    void testHighlightingLag();
    
    // Functional regressions
    void testUndoRedoConsistency();
    void testFileSaveIntegrity();
    void testBreakpointPersistence();
    
    // UI regressions
    void testDockLayoutPersistence();
    void testThemeApplication();
    void testShortcutConflicts();
};

/**
 * @brief Performance benchmarks
 */
class PerformanceBenchmarks : public EditorTestBase {
    Q_OBJECT

private Q_SLOTS:
    // Highlighting benchmarks
    void benchmarkKeywordHighlighting();
    void benchmarkFullFileHighlighting();
    void benchmarkIncrementalHighlighting();
    
    // Completion benchmarks
    void benchmarkCompletionPopup();
    void benchmarkCompletionSelection();
    
    // File I/O benchmarks
    void benchmarkFileLoad();
    void benchmarkFileSave();
    void benchmarkLargeFileLoad();
    
    // UI benchmarks
    void benchmarkScrolling();
    void benchmarkResizing();
    void benchmarkDockOperations();
};

/**
 * @brief Test runner for all editor tests
 */
class EditorTestRunner : public QObject {
    Q_OBJECT

public:
    static EditorTestRunner* instance();
    
    // Test execution
    int runAllTests();
    int runTests(const QString& pattern);
    
    // Results
    int passedTests() const { return m_passedTests; }
    int failedTests() const { return m_failedTests; }
    int skippedTests() const { return m_skippedTests; }
    
    // Reporting
    void generateReport(const QString& filePath);
    void printSummary();

private:
    EditorTestRunner(QObject* parent = nullptr);
    
    static EditorTestRunner* s_instance;
    
    int m_passedTests;
    int m_failedTests;
    int m_skippedTests;
    
    QStringList m_testResults;
};

// Test macros
#define FLUX_TEST_DECLARE(className, testName) \
    void testName();

#define FLUX_TEST_DEFINE(className, testName) \
    void className::testName()

#define FLUX_TEST_ASSERT(condition, message) \
    QVERIFY2(condition, message)

#define FLUX_TEST_COMPARE(actual, expected) \
    QCOMPARE(actual, expected)

#define FLUX_TEST_BENCHMARK(code) \
    QBENCHMARK { code; }

} // namespace Flux

#endif // FLUX_EDITOR_TESTS_H
