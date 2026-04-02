#include "flux/flux_editor_performance.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QThreadPool>
#include <QRunnable>
#include <QCoreApplication>
#include <QResource>
#include <QTimer>

#ifdef Q_OS_LINUX
#include <sys/resource.h>
#elif defined(Q_OS_WIN)
#include <windows.h>
#include <psapi.h>
#endif

namespace Flux {

// ============================================================================
// PerformanceProfiler Implementation
// ============================================================================

PerformanceProfiler* PerformanceProfiler::s_instance = nullptr;

PerformanceProfiler::PerformanceProfiler(QObject* parent)
    : QObject(parent)
{
    Q_UNUSED(parent);
}

PerformanceProfiler* PerformanceProfiler::instance()
{
    if (!s_instance) {
        s_instance = new PerformanceProfiler();
    }
    return s_instance;
}

void PerformanceProfiler::startProfile(const QString& operation)
{
    QElapsedTimer& timer = m_timers[operation];
    if (!timer.isValid()) {
        timer.start();
    } else {
        timer.restart();
    }
}

void PerformanceProfiler::endProfile(const QString& operation)
{
    QElapsedTimer& timer = m_timers[operation];
    if (!timer.isValid()) {
        return;
    }
    
    qint64 elapsed = timer.elapsed();
    timer.invalidate();
    
    PerformanceStats& stats = m_stats[operation];
    stats.totalTimeMs += elapsed;
    stats.minTimeMs = qMin(stats.minTimeMs, elapsed);
    stats.maxTimeMs = qMax(stats.maxTimeMs, elapsed);
    stats.callCount++;
    stats.avgTimeMs = stats.totalTimeMs / stats.callCount;
    
    // Check threshold
    if (m_thresholds.contains(operation) && elapsed > m_thresholds[operation]) {
        Q_EMIT thresholdExceeded(operation, elapsed);
    }
}

PerformanceStats PerformanceProfiler::getStats(const QString& operation) const
{
    return m_stats.value(operation);
}

QMap<QString, PerformanceStats> PerformanceProfiler::getAllStats() const
{
    return m_stats;
}

void PerformanceProfiler::resetStats()
{
    m_stats.clear();
    m_timers.clear();
}

QString PerformanceProfiler::generateReport() const
{
    QString report;
    QTextStream stream(&report);
    
    stream << "=== Performance Report ===\n\n";
    
    for (auto it = m_stats.begin(); it != m_stats.end(); ++it) {
        const QString& op = it.key();
        const PerformanceStats& stats = it.value();
        
        stream << QString("%1:\n").arg(op);
        stream << QString("  Calls: %1\n").arg(stats.callCount);
        stream << QString("  Total: %1 ms\n").arg(stats.totalTimeMs);
        stream << QString("  Min: %1 ms\n").arg(stats.minTimeMs == LLONG_MAX ? 0 : stats.minTimeMs);
        stream << QString("  Max: %1 ms\n").arg(stats.maxTimeMs);
        stream << QString("  Avg: %1 ms\n\n").arg(stats.avgTimeMs);
    }
    
    return report;
}

void PerformanceProfiler::printReport() const
{
    qDebug() << generateReport();
}

void PerformanceProfiler::setWarningThreshold(const QString& operation, qint64 thresholdMs)
{
    m_thresholds[operation] = thresholdMs;
}

qint64 PerformanceProfiler::warningThreshold(const QString& operation) const
{
    return m_thresholds.value(operation, -1);
}

// ============================================================================
// IncrementalHighlighter Implementation
// ============================================================================

IncrementalHighlighter::IncrementalHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
    , m_updateDelay(50)
    , m_maxBlockSize(1000)
    , m_backgroundHighlighting(true)
    , m_pendingHighlight(false)
    , m_highlightedBlocks(0)
    , m_lastHighlightTime(0)
{
    m_highlightTimer = new QTimer(this);
    m_highlightTimer->setSingleShot(true);
    connect(m_highlightTimer, &QTimer::timeout,
            this, &IncrementalHighlighter::processPendingHighlights);
    
    connect(document(), &QTextDocument::contentsChanged,
            this, &IncrementalHighlighter::onDocumentChanged);
}

void IncrementalHighlighter::setUpdateDelay(int ms)
{
    m_updateDelay = ms;
}

void IncrementalHighlighter::setMaxBlockSize(int lines)
{
    m_maxBlockSize = lines;
}

void IncrementalHighlighter::enableBackgroundHighlighting(bool enable)
{
    m_backgroundHighlighting = enable;
}

void IncrementalHighlighter::highlightBlock(const QString& text)
{
    m_timer.start();

    // Base class highlighting is handled by derived classes

    m_highlightedBlocks++;
    m_lastHighlightTime = m_timer.elapsed();
}

void IncrementalHighlighter::onDocumentChanged()
{
    if (!m_backgroundHighlighting) {
        return;
    }
    
    m_pendingHighlight = true;
    m_highlightTimer->start(m_updateDelay);
}

void IncrementalHighlighter::processPendingHighlights()
{
    if (!m_pendingHighlight) {
        return;
    }
    
    m_pendingHighlight = false;
    
    QTextDocument* doc = document();
    if (!doc) {
        return;
    }
    
    int blockCount = doc->blockCount();
    
    // Highlight in chunks
    int startBlock = 0;
    while (startBlock < blockCount) {
        int endBlock = qMin(startBlock + m_maxBlockSize, blockCount);
        highlightRange(startBlock, endBlock);
        
        // Process events to keep UI responsive
        QCoreApplication::processEvents();
        
        startBlock = endBlock;
    }
}

void IncrementalHighlighter::scheduleHighlighting(int blockNumber)
{
    m_pendingBlocks.enqueue(blockNumber);
    
    if (!m_highlightTimer->isActive()) {
        m_highlightTimer->start(m_updateDelay);
    }
}

void IncrementalHighlighter::highlightRange(int startBlock, int endBlock)
{
    QTextDocument* doc = document();
    if (!doc) {
        return;
    }
    
    QTextBlock block = doc->findBlockByNumber(startBlock);
    while (block.isValid() && block.blockNumber() < endBlock) {
        highlightBlock(block.text());
        block = block.next();
    }
}

// ============================================================================
// HighlightWorker Implementation
// ============================================================================

HighlightWorker::HighlightWorker(QObject* parent)
    : QThread(parent)
{
}

void HighlightWorker::enqueueHighlightTask(int startBlock, int endBlock)
{
    QMutexLocker locker(&m_mutex);
    
    HighlightTask task;
    task.startBlock = startBlock;
    task.endBlock = endBlock;
    m_taskQueue.enqueue(task);
    
    m_condition.wakeOne();
}

void HighlightWorker::stop()
{
    m_stopRequested.storeRelaxed(1);
    m_condition.wakeOne();
    wait();
}

void HighlightWorker::run()
{
    while (!m_stopRequested.loadRelaxed()) {
        QMutexLocker locker(&m_mutex);

        if (m_taskQueue.isEmpty()) {
            m_condition.wait(&m_mutex);
            continue;
        }

        HighlightTask task = m_taskQueue.dequeue();
        locker.unlock();

        // Process task (would integrate with highlighter)
        Q_EMIT highlightCompleted(task.startBlock, task.endBlock, QVector<QTextCharFormat>());
    }
}

// ============================================================================
// TextCache Implementation
// ============================================================================

TextCache::TextCache(int maxCost)
    : m_cache(maxCost)
    , m_hits(0)
    , m_misses(0)
{
}

void TextCache::insert(const QString& key, const QString& text, int cost)
{
    m_cache.insert(key, new QString(text), cost);
}

QString TextCache::value(const QString& key) const
{
    QString* value = m_cache.object(key);
    if (value) {
        const_cast<TextCache*>(this)->m_hits++;
        // Q_EMIT cacheHit(key);  // Can't emit signals from const method
        return *value;
    }

    const_cast<TextCache*>(this)->m_misses++;
    // Q_EMIT cacheMiss(key);  // Can't emit signals from const method
    return QString();
}

bool TextCache::contains(const QString& key) const
{
    return m_cache.contains(key);
}

void TextCache::remove(const QString& key)
{
    m_cache.remove(key);
}

void TextCache::clear()
{
    m_cache.clear();
    m_hits = 0;
    m_misses = 0;
}

int TextCache::hitRate() const
{
    int total = m_hits + m_misses;
    if (total == 0) {
        return 0;
    }
    return (m_hits * 100) / total;
}

void TextCache::preload(const QStringList& keys)
{
    for (const QString& key : keys) {
        // Would load from disk or other source
    }
}

// ============================================================================
// CompletionCache Implementation
// ============================================================================

CompletionCache::CompletionCache(int maxEntries)
    : m_maxEntries(maxEntries)
{
}

void CompletionCache::cacheCompletions(const QString& prefix, const QStringList& completions)
{
    m_completionCache[prefix] = completions;
    
    // Trim cache if needed
    while (m_completionCache.size() > m_maxEntries) {
        m_completionCache.remove(m_recentPrefixes.dequeue());
    }
    
    m_recentPrefixes.enqueue(prefix);
}

QStringList CompletionCache::getCachedCompletions(const QString& prefix) const
{
    return m_completionCache.value(prefix);
}

bool CompletionCache::hasCachedCompletions(const QString& prefix) const
{
    return m_completionCache.contains(prefix);
}

void CompletionCache::cacheContext(const QString& context, const QStringList& keywords)
{
    m_contextCache[context] = keywords;
}

QStringList CompletionCache::getCachedContext(const QString& context) const
{
    return m_contextCache.value(context);
}

void CompletionCache::invalidatePrefix(const QString& prefix)
{
    m_completionCache.remove(prefix);
}

void CompletionCache::clear()
{
    m_completionCache.clear();
    m_contextCache.clear();
    m_recentPrefixes.clear();
}

// ============================================================================
// LargeFileHandler Implementation
// ============================================================================

LargeFileHandler::LargeFileHandler(QObject* parent)
    : QObject(parent)
    , m_lazyLoadThreshold(10000)
    , m_totalLines(0)
    , m_viewportSize(50)
    , m_visibleStart(0)
    , m_visibleEnd(0)
    , m_file(nullptr)
{
}

void LargeFileHandler::setLazyLoadThreshold(int lines)
{
    m_lazyLoadThreshold = lines;
}

void LargeFileHandler::setVisibleRange(int startLine, int endLine)
{
    m_visibleStart = startLine;
    m_visibleEnd = endLine;
    
    loadVisibleRange();
}

bool LargeFileHandler::loadFile(const QString& path)
{
    m_filePath = path;
    m_file = new QFile(path);
    
    if (!m_file->open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    // Count lines
    QTextStream in(m_file);
    m_totalLines = 0;
    
    while (!in.atEnd()) {
        in.readLine();
        m_totalLines++;
        
        if (m_totalLines % 1000 == 0) {
            Q_EMIT loadingProgress(m_totalLines, -1);
        }
    }
    
    m_file->close();
    
    // Load initial viewport
    if (m_totalLines > m_lazyLoadThreshold) {
        loadRange(0, qMin(m_viewportSize, m_totalLines));
    } else {
        loadRange(0, m_totalLines);
    }
    
    Q_EMIT loadCompleted();
    return true;
}

bool LargeFileHandler::loadRange(int startLine, int endLine)
{
    if (!m_file) {
        return false;
    }
    
    if (!m_file->open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    
    QTextStream in(m_file);
    int currentLine = 0;
    
    while (!in.atEnd() && currentLine < endLine) {
        QString line = in.readLine();
        
        if (currentLine >= startLine && currentLine < endLine) {
            m_loadedLines[currentLine] = line;
        }
        
        currentLine++;
    }
    
    m_file->close();
    
    m_loadedRanges.enqueue(startLine);
    if (m_loadedRanges.size() > 10) {
        m_loadedRanges.dequeue();
    }
    
    Q_EMIT linesLoaded(startLine, endLine);
    return true;
}

QString LargeFileHandler::lineAt(int lineNumber) const
{
    return m_loadedLines.value(lineNumber);
}

void LargeFileHandler::setViewportSize(int lines)
{
    m_viewportSize = lines;
}

void LargeFileHandler::scrollBy(int delta)
{
    m_visibleStart += delta;
    m_visibleEnd = m_visibleStart + m_viewportSize;
    
    loadVisibleRange();
    preloadAdjacentRanges();
}

void LargeFileHandler::loadVisibleRange()
{
    for (int line = m_visibleStart; line < m_visibleEnd && line < m_totalLines; ++line) {
        if (!m_loadedLines.contains(line)) {
            loadRange(line, qMin(line + 10, m_totalLines));
        }
    }
}

void LargeFileHandler::preloadAdjacentRanges()
{
    // Preload lines above visible range
    int preloadStart = qMax(0, m_visibleStart - m_viewportSize);
    for (int line = preloadStart; line < m_visibleStart; ++line) {
        if (!m_loadedLines.contains(line)) {
            loadRange(line, qMin(line + 10, m_visibleStart));
        }
    }
    
    // Preload lines below visible range
    int preloadEnd = qMin(m_totalLines, m_visibleEnd + m_viewportSize);
    for (int line = m_visibleEnd; line < preloadEnd; ++line) {
        if (!m_loadedLines.contains(line)) {
            loadRange(line, qMin(line + 10, preloadEnd));
        }
    }
}

// ============================================================================
// EditorMemoryManager Implementation
// ============================================================================

EditorMemoryManager* EditorMemoryManager::s_instance = nullptr;

EditorMemoryManager::EditorMemoryManager(QObject* parent)
    : QObject(parent)
    , m_currentMemory(0)
    , m_peakMemory(0)
    , m_maxMemory(500 * 1024 * 1024)  // 500 MB default
    , m_monitorTimer(nullptr)
    , m_monitoring(false)
{
}

EditorMemoryManager* EditorMemoryManager::instance()
{
    if (!s_instance) {
        s_instance = new EditorMemoryManager();
    }
    return s_instance;
}

qint64 EditorMemoryManager::currentMemoryUsage() const
{
#ifdef Q_OS_LINUX
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        return usage.ru_maxrss * 1024;  // Convert KB to bytes
    }
#elif defined(Q_OS_WIN)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
#endif
    return m_currentMemory;
}

qint64 EditorMemoryManager::peakMemoryUsage() const
{
    return m_peakMemory;
}

void EditorMemoryManager::resetPeakMemory()
{
    m_peakMemory = m_currentMemory;
}

void EditorMemoryManager::setMaxMemoryUsage(qint64 bytes)
{
    m_maxMemory = bytes;
}

void EditorMemoryManager::triggerGarbageCollection()
{
    // Trigger Qt's internal cleanup
    QCoreApplication::processEvents();
}

void EditorMemoryManager::clearCaches()
{
    // Would clear all editor caches
}

void EditorMemoryManager::startMonitoring()
{
    if (m_monitoring) {
        return;
    }
    
    m_monitoring = true;
    m_monitorTimer = new QTimer(this);
    connect(m_monitorTimer, &QTimer::timeout,
            this, &EditorMemoryManager::updateMemoryUsage);
    m_monitorTimer->start(1000);  // Update every second
}

void EditorMemoryManager::stopMonitoring()
{
    m_monitoring = false;
    if (m_monitorTimer) {
        m_monitorTimer->stop();
        m_monitorTimer->deleteLater();
        m_monitorTimer = nullptr;
    }
}

void EditorMemoryManager::updateMemoryUsage()
{
    m_currentMemory = currentMemoryUsage();
    
    if (m_currentMemory > m_peakMemory) {
        m_peakMemory = m_currentMemory;
    }
    
    if (m_currentMemory > m_maxMemory * 0.9) {
        Q_EMIT memoryLimitReached(m_currentMemory);
    } else if (m_currentMemory > m_maxMemory * 0.7) {
        Q_EMIT memoryWarning(m_currentMemory);
    }
}

// ============================================================================
// RenderOptimizer Implementation
// ============================================================================

RenderOptimizer::RenderOptimizer(QObject* parent)
    : QObject(parent)
    , m_targetFPS(60)
    , m_skipFrameThreshold(16)  // ~60 FPS
    , m_lastFrameTime(0)
    , m_frameCount(0)
{
    m_timer.start();
}

void RenderOptimizer::setTargetFPS(int fps)
{
    m_targetFPS = fps;
    m_skipFrameThreshold = 1000 / fps;
}

void RenderOptimizer::setSkipFrameThreshold(int ms)
{
    m_skipFrameThreshold = ms;
}

bool RenderOptimizer::shouldRender() const
{
    qint64 elapsed = m_timer.elapsed() - m_lastFrameTime;
    return elapsed >= m_skipFrameThreshold;
}

void RenderOptimizer::markFrameRendered()
{
    m_lastFrameTime = m_timer.elapsed();
    m_frameCount++;
}

void RenderOptimizer::setVisibleRect(const QRect& rect)
{
    m_visibleRect = rect;
}

void RenderOptimizer::markDirty(const QRect& rect)
{
    m_dirtyRegion = m_dirtyRegion.united(rect);
}

void RenderOptimizer::resetDirty()
{
    m_dirtyRegion = QRect();
}

// ============================================================================
// AsyncOperationManager Implementation
// ============================================================================

AsyncOperationManager::AsyncOperationManager(QObject* parent)
    : QObject(parent)
    , m_nextId(1)
{
}

int AsyncOperationManager::startOperation(const QString& name, std::function<void()> operation)
{
    QMutexLocker locker(&m_mutex);
    
    int id = m_nextId.fetchAndAddAcquire(1);

    Operation op;
    op.id = id;
    op.name = name;
    op.function = operation;
    op.priority = 1;  // Normal priority (0=Low, 1=Normal, 2=High)
    op.cancelled = false;

    m_operations[id] = op;
    locker.unlock();

    // Start async operation using QTimer for Qt6 compatibility
    QTimer::singleShot(0, [this, id]() {
        QMutexLocker locker(&m_mutex);
        if (!m_operations.contains(id)) return;
        
        Operation& op = m_operations[id];
        if (op.cancelled) {
            Q_EMIT operationCancelled(id);
            return;
        }

        try {
            op.function();
            Q_EMIT operationCompleted(id, op.name);
        } catch (const std::exception& e) {
            Q_EMIT operationFailed(id, QString::fromStdString(e.what()));
        }

        QMutexLocker locker2(&m_mutex);
        m_operations.remove(id);
    });
    
    return id;
}

void AsyncOperationManager::cancelOperation(int id)
{
    QMutexLocker locker(&m_mutex);
    if (m_operations.contains(id)) {
        m_operations[id].cancelled = true;
    }
}

void AsyncOperationManager::cancelAllOperations()
{
    QMutexLocker locker(&m_mutex);
    for (auto& op : m_operations) {
        op.cancelled = true;
    }
}

bool AsyncOperationManager::isOperationRunning(int id) const
{
    return m_operations.contains(id);
}

int AsyncOperationManager::activeOperationCount() const
{
    return m_operations.size();
}

void AsyncOperationManager::setOperationPriority(int id, int priority)
{
    QMutexLocker locker(&m_mutex);
    if (m_operations.contains(id)) {
        m_operations[id].priority = priority;
    }
}

} // namespace Flux
