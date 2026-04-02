#ifndef FLUX_EDITOR_PERFORMANCE_H
#define FLUX_EDITOR_PERFORMANCE_H

/**
 * @file flux_editor_performance.h
 * @brief Performance optimization utilities for FluxScript editor
 * 
 * Phase 4: Polish & Optimization
 */

#include <QObject>
#include <QElapsedTimer>
#include <QMap>
#include <QString>
#include <QTextDocument>
#include <QSyntaxHighlighter>
#include <QQueue>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicInt>
#include <QCache>
#include <QSharedPointer>
#include <QFuture>
#include <QFutureWatcher>

namespace Flux {

/**
 * @brief Performance profiling statistics
 */
struct PerformanceStats {
    QString operation;
    qint64 totalTimeMs;
    qint64 minTimeMs;
    qint64 maxTimeMs;
    qint64 avgTimeMs;
    int callCount;
    
    PerformanceStats() 
        : totalTimeMs(0), minTimeMs(LLONG_MAX), maxTimeMs(0), 
          avgTimeMs(0), callCount(0) {}
};

/**
 * @brief Performance profiler for measuring operation timings
 */
class PerformanceProfiler : public QObject {
    Q_OBJECT

public:
    static PerformanceProfiler* instance();
    
    // Profiling
    void startProfile(const QString& operation);
    void endProfile(const QString& operation);
    
    // Statistics
    PerformanceStats getStats(const QString& operation) const;
    QMap<QString, PerformanceStats> getAllStats() const;
    void resetStats();
    
    // Reporting
    QString generateReport() const;
    void printReport() const;
    
    // Thresholds
    void setWarningThreshold(const QString& operation, qint64 thresholdMs);
    qint64 warningThreshold(const QString& operation) const;

Q_SIGNALS:
    void thresholdExceeded(const QString& operation, qint64 timeMs);

private:
    PerformanceProfiler(QObject* parent = nullptr);
    
    QMap<QString, PerformanceStats> m_stats;
    QMap<QString, QElapsedTimer> m_timers;
    QMap<QString, qint64> m_thresholds;
    
    static PerformanceProfiler* s_instance;
};

/**
 * @brief RAII profiler scope guard
 */
class ProfileScope {
public:
    explicit ProfileScope(const QString& operation)
        : m_operation(operation) {
        PerformanceProfiler::instance()->startProfile(operation);
    }
    
    ~ProfileScope() {
        PerformanceProfiler::instance()->endProfile(m_operation);
    }
    
private:
    QString m_operation;
};

/**
 * @brief Incremental syntax highlighter for large files
 */
class IncrementalHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    explicit IncrementalHighlighter(QTextDocument* parent = nullptr);
    
    // Configuration
    void setUpdateDelay(int ms);
    void setMaxBlockSize(int lines);
    void enableBackgroundHighlighting(bool enable);
    
    // Statistics
    int highlightedBlocks() const { return m_highlightedBlocks; }
    qint64 lastHighlightTime() const { return m_lastHighlightTime; }

protected:
    void highlightBlock(const QString& text) override;
    void onDocumentChanged();

private Q_SLOTS:
    void processPendingHighlights();

private:
    void scheduleHighlighting(int blockNumber);
    void highlightRange(int startBlock, int endBlock);
    
    int m_updateDelay;
    int m_maxBlockSize;
    bool m_backgroundHighlighting;
    bool m_pendingHighlight;
    int m_highlightedBlocks;
    qint64 m_lastHighlightTime;
    
    QQueue<int> m_pendingBlocks;
    QTimer* m_highlightTimer;
    
    QElapsedTimer m_timer;
};

/**
 * @brief Background highlighting worker thread
 */
class HighlightWorker : public QThread {
    Q_OBJECT

public:
    explicit HighlightWorker(QObject* parent = nullptr);
    
    void enqueueHighlightTask(int startBlock, int endBlock);
    void stop();

Q_SIGNALS:
    void highlightCompleted(int startBlock, int endBlock, const QVector<QTextCharFormat>& formats);
    void progress(int current, int total);

protected:
    void run() override;

private:
    struct HighlightTask {
        int startBlock;
        int endBlock;
    };
    
    QQueue<HighlightTask> m_taskQueue;
    QMutex m_mutex;
    QWaitCondition m_condition;
    QAtomicInt m_stopRequested;
};

/**
 * @brief Text caching for improved performance
 */
class TextCache : public QObject {
    Q_OBJECT

public:
    explicit TextCache(int maxCost = 1000);
    
    // Cache operations
    void insert(const QString& key, const QString& text, int cost = 1);
    QString value(const QString& key) const;
    bool contains(const QString& key) const;
    void remove(const QString& key);
    void clear();
    
    // Statistics
    int size() const { return m_cache.size(); }
    int maxCost() const { return m_cache.maxCost(); }
    int hitRate() const;  // Percentage
    
    // Preloading
    void preload(const QStringList& keys);

Q_SIGNALS:
    void cacheHit(const QString& key);
    void cacheMiss(const QString& key);

private:
    QCache<QString, QString> m_cache;
    int m_hits;
    int m_misses;
};

/**
 * @brief Completion cache for faster suggestions
 */
class CompletionCache : public QObject {
    Q_OBJECT

public:
    explicit CompletionCache(int maxEntries = 500);
    
    // Completion caching
    void cacheCompletions(const QString& prefix, const QStringList& completions);
    QStringList getCachedCompletions(const QString& prefix) const;
    bool hasCachedCompletions(const QString& prefix) const;
    
    // Context caching
    void cacheContext(const QString& context, const QStringList& keywords);
    QStringList getCachedContext(const QString& context) const;
    
    // Invalidation
    void invalidatePrefix(const QString& prefix);
    void clear();

private:
    QMap<QString, QStringList> m_completionCache;
    QMap<QString, QStringList> m_contextCache;
    QQueue<QString> m_recentPrefixes;
    int m_maxEntries;
};

/**
 * @brief Large file handler with lazy loading
 */
class LargeFileHandler : public QObject {
    Q_OBJECT

public:
    explicit LargeFileHandler(QObject* parent = nullptr);
    
    // Configuration
    void setLazyLoadThreshold(int lines);  // Default: 10000
    void setVisibleRange(int startLine, int endLine);
    
    // File operations
    bool loadFile(const QString& path);
    bool loadRange(int startLine, int endLine);
    QString lineAt(int lineNumber) const;
    int totalLines() const { return m_totalLines; }
    
    // Viewport management
    void setViewportSize(int lines);
    void scrollBy(int delta);

Q_SIGNALS:
    void linesLoaded(int startLine, int endLine);
    void loadingProgress(int current, int total);
    void loadCompleted();

private:
    void loadVisibleRange();
    void preloadAdjacentRanges();
    
    int m_lazyLoadThreshold;
    int m_totalLines;
    int m_viewportSize;
    int m_visibleStart;
    int m_visibleEnd;
    
    QMap<int, QString> m_loadedLines;  // line number -> content
    QQueue<int> m_loadedRanges;  // Recently loaded ranges
    
    QString m_filePath;
    QFile* m_file;
};

/**
 * @brief Memory manager for editor resources
 */
class EditorMemoryManager : public QObject {
    Q_OBJECT

public:
    static EditorMemoryManager* instance();
    
    // Memory tracking
    qint64 currentMemoryUsage() const;
    qint64 peakMemoryUsage() const;
    void resetPeakMemory();
    
    // Limits
    void setMaxMemoryUsage(qint64 bytes);
    qint64 maxMemoryUsage() const { return m_maxMemory; }
    
    // Cleanup
    void triggerGarbageCollection();
    void clearCaches();
    
    // Monitoring
    void startMonitoring();
    void stopMonitoring();

Q_SIGNALS:
    void memoryLimitReached(qint64 usage);
    void memoryWarning(qint64 usage);

private:
    EditorMemoryManager(QObject* parent = nullptr);
    void updateMemoryUsage();
    
    static EditorMemoryManager* s_instance;
    
    qint64 m_currentMemory;
    qint64 m_peakMemory;
    qint64 m_maxMemory;
    
    QTimer* m_monitorTimer;
    bool m_monitoring;
};

/**
 * @brief Render optimization for smooth scrolling
 */
class RenderOptimizer : public QObject {
    Q_OBJECT

public:
    explicit RenderOptimizer(QObject* parent = nullptr);
    
    // Configuration
    void setTargetFPS(int fps);
    void setSkipFrameThreshold(int ms);
    
    // Optimization
    bool shouldRender() const;
    void markFrameRendered();
    
    // Viewport culling
    void setVisibleRect(const QRect& rect);
    QRect visibleRect() const { return m_visibleRect; }
    
    // Dirty region management
    void markDirty(const QRect& rect);
    QRect dirtyRegion() const { return m_dirtyRegion; }
    void resetDirty();

private:
    int m_targetFPS;
    int m_skipFrameThreshold;
    qint64 m_lastFrameTime;
    int m_frameCount;
    
    QRect m_visibleRect;
    QRect m_dirtyRegion;
    
    QElapsedTimer m_timer;
};

/**
 * @brief Async operation manager for non-blocking UI
 */
class AsyncOperationManager : public QObject {
    Q_OBJECT

public:
    explicit AsyncOperationManager(QObject* parent = nullptr);
    
    // Operation management
    int startOperation(const QString& name, std::function<void()> operation);
    void cancelOperation(int id);
    void cancelAllOperations();

    // Status
    bool isOperationRunning(int id) const;
    int activeOperationCount() const;

    // Priority
    void setOperationPriority(int id, int priority);

Q_SIGNALS:
    void operationCompleted(int id, const QString& name);
    void operationFailed(int id, const QString& error);
    void operationCancelled(int id);
    void progressUpdated(int id, int progress);

private:
    struct Operation {
        int id;
        QString name;
        std::function<void()> function;
        int priority;  // 0=Low, 1=Normal, 2=High
        QFuture<void> future;
        bool cancelled;
    };
    
    QMap<int, Operation> m_operations;
    QAtomicInt m_nextId;
    QMutex m_mutex;
};

// Convenience macro for profiling
#define FLUX_PROFILE(operation) \
    Flux::ProfileScope flux_profile_scope_##__LINE__(operation)

} // namespace Flux

#endif // FLUX_EDITOR_PERFORMANCE_H
