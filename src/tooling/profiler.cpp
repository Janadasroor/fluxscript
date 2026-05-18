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

#include "flux/tooling/profiler.h"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <iostream>
#include <ctime>

namespace Flux {
namespace Profiler {

// ============================================================================
// ScopedTimer Implementation
// ============================================================================

Profiler::ScopedTimer::ScopedTimer(const std::string& name, const std::string& file, int line)
    : m_name(name), m_file(file), m_line(line)
    , m_start(std::chrono::steady_clock::now())
{
    Profiler::instance().functionEntry(name, file, line);
}

Profiler::ScopedTimer::~ScopedTimer() {
    Profiler::instance().functionExit(m_name);
}

// ============================================================================
// Profiler Implementation
// ============================================================================

Profiler& Profiler::instance() {
    static Profiler profiler;
    return profiler;
}

Profiler::Profiler() = default;
Profiler::~Profiler() = default;

void Profiler::start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_running = true;
    m_startTime = std::chrono::steady_clock::now();
    reset();
}

void Profiler::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_running = false;
}

void Profiler::functionEntry(const std::string& name, const std::string& file, int line) {
    if (!m_enabled || !m_running) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    CallStackEntry entry;
    entry.functionName = name;
    entry.startTime = std::chrono::steady_clock::now();
    entry.childTime = std::chrono::nanoseconds::zero();
    
    m_callStack.push_back(entry);
    
    // Initialize entry if needed
    if (m_entries.find(name) == m_entries.end()) {
        ProfileEntry pe;
        pe.functionName = name;
        pe.file = file;
        pe.line = line;
        m_entries[name] = pe;
    }
    
    m_entries[name].callCount++;
}

void Profiler::functionExit(const std::string& name) {
    if (!m_enabled || !m_running) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (m_callStack.empty()) return;
    
    CallStackEntry entry = m_callStack.back();
    m_callStack.pop_back();
    
    auto endTime = std::chrono::steady_clock::now();
    auto totalDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        endTime - entry.startTime);
    auto selfTime = totalDuration - entry.childTime;
    
    // Update parent's child time
    if (!m_callStack.empty()) {
        m_callStack.back().childTime += totalDuration;
    }
    
    // Update profile entry
    auto it = m_entries.find(name);
    if (it != m_entries.end()) {
        it->second.totalTime += totalDuration;
        it->second.selfTime += selfTime;
    }
}

ProfileReport Profiler::getReport() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    ProfileReport report;
    report.totalDuration = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - m_startTime);
    report.totalCalls = 0;
    
    for (const auto& pair : m_entries) {
        ProfileEntry entry = pair.second;
        report.entries.push_back(entry);
        report.totalCalls += entry.callCount;
    }
    
    // Sort by total time
    std::sort(report.entries.begin(), report.entries.end(),
        [](const ProfileEntry& a, const ProfileEntry& b) {
            return a.totalTime > b.totalTime;
        });
    
    return report;
}

void Profiler::printReport(int topN) {
    auto report = getReport();
    report.print(topN);
}

void Profiler::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_entries.clear();
    m_callStack.clear();
    m_memoryStats = MemoryStats();
    m_allocations.clear();
}

void Profiler::saveReport(const std::string& filename, int topN) {
    auto report = getReport();
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Cannot open file: " << filename << std::endl;
        return;
    }
    
    file << "FluxScript Profile Report\n";
    file << "========================\n\n";
    file << "Total Duration: " << report.totalDuration.count() / 1e9 << " s\n";
    file << "Total Calls: " << report.totalCalls << "\n\n";
    
    file << std::left << std::setw(40) << "Function"
         << std::right << std::setw(12) << "Calls"
         << std::setw(15) << "Total (ms)"
         << std::setw(15) << "Self (ms)"
         << std::setw(10) << "Avg (s)"
         << "\n";
    file << std::string(92, '-') << "\n";
    
    int count = 0;
    for (const auto& entry : report.entries) {
        if (count++ >= topN) break;
        
        double totalMs = entry.totalTime.count() / 1e6;
        double selfMs = entry.selfTime.count() / 1e6;
        double avgUs = (entry.callCount > 0) ? 
            (totalMs * 1000.0 / entry.callCount) : 0;
        
        file << std::left << std::setw(40) << entry.functionName
             << std::right << std::setw(12) << entry.callCount
             << std::setw(15) << std::fixed << std::setprecision(2) << totalMs
             << std::setw(15) << std::fixed << std::setprecision(2) << selfMs
             << std::setw(10) << std::fixed << std::setprecision(2) << avgUs
             << "\n";
    }
    
    file.close();
    std::cout << "Profile report saved to: " << filename << std::endl;
}

void Profiler::saveFlameGraph(const std::string& filename) {
    auto report = getReport();
    std::string svg = report.toFlameGraph();
    
    std::ofstream file(filename);
    if (file.is_open()) {
        file << svg;
        file.close();
        std::cout << "Flame graph saved to: " << filename << std::endl;
    }
}

// Memory tracking
void Profiler::trackAllocation(void* ptr, size_t size, const std::string& file, int line) {
    if (!m_enabled) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    AllocationRecord record;
    record.address = ptr;
    record.size = size;
    record.file = file;
    record.line = line;
    record.time = std::chrono::steady_clock::now();
    record.freed = false;
    
    m_allocations[ptr] = record;
    
    m_memoryStats.totalAllocated += size;
    m_memoryStats.currentUsage += size;
    m_memoryStats.allocationCount++;
    
    if (m_memoryStats.currentUsage > m_memoryStats.peakUsage) {
        m_memoryStats.peakUsage = m_memoryStats.currentUsage;
    }
}

void Profiler::trackDeallocation(void* ptr) {
    if (!m_enabled) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_allocations.find(ptr);
    if (it != m_allocations.end()) {
        m_memoryStats.totalFreed += it->second.size;
        m_memoryStats.currentUsage -= it->second.size;
        it->second.freed = true;
    }
}

MemoryStats Profiler::getMemoryStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    MemoryStats stats = m_memoryStats;
    stats.leakCount = 0;
    
    for (const auto& pair : m_allocations) {
        if (!pair.second.freed) {
            stats.leakCount++;
        }
    }
    
    return stats;
}

void Profiler::printMemoryReport() {
    auto stats = getMemoryStats();
    
    std::cout << "\nMemory Report\n";
    std::cout << "=============\n";
    std::cout << "Total Allocated: " << stats.totalAllocated / 1024 << " KB\n";
    std::cout << "Total Freed:     " << stats.totalFreed / 1024 << " KB\n";
    std::cout << "Current Usage:   " << stats.currentUsage / 1024 << " KB\n";
    std::cout << "Peak Usage:      " << stats.peakUsage / 1024 << " KB\n";
    std::cout << "Allocations:     " << stats.allocationCount << "\n";
    std::cout << "Leaks:           " << stats.leakCount << "\n\n";
    
    auto leaks = getLeaks();
    if (!leaks.empty()) {
        std::cout << "Memory Leaks:\n";
        for (const auto& leak : leaks) {
            std::cout << "  " << leak.size << " bytes at " << leak.address
                      << " (" << leak.file << ":" << leak.line << ")\n";
        }
    }
}

std::vector<AllocationRecord> Profiler::getLeaks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<AllocationRecord> leaks;
    for (const auto& pair : m_allocations) {
        if (!pair.second.freed) {
            leaks.push_back(pair.second);
        }
    }
    return leaks;
}

// ============================================================================
// ProfileReport Implementation
// ============================================================================

void ProfileReport::print(int topN) const {
    std::cout << "\nFluxScript Profile Report\n";
    std::cout << "========================\n\n";
    std::cout << "Total Duration: " << totalDuration.count() / 1e9 << " s\n";
    std::cout << "Total Calls:    " << totalCalls << "\n\n";
    
    std::cout << std::left << std::setw(40) << "Function"
              << std::right << std::setw(12) << "Calls"
              << std::setw(15) << "Total (ms)"
              << std::setw(15) << "Self (ms)"
              << std::setw(10) << "Avg (s)"
              << "\n";
    std::cout << std::string(92, '-') << "\n";
    
    int count = 0;
    for (const auto& entry : entries) {
        if (count++ >= topN) break;
        if (entry.callCount == 0) continue;
        
        double totalMs = entry.totalTime.count() / 1e6;
        double selfMs = entry.selfTime.count() / 1e6;
        double avgUs = totalMs * 1000.0 / entry.callCount;
        
        std::cout << std::left << std::setw(40) << entry.functionName
                  << std::right << std::setw(12) << entry.callCount
                  << std::setw(15) << std::fixed << std::setprecision(2) << totalMs
                  << std::setw(15) << std::fixed << std::setprecision(2) << selfMs
                  << std::setw(10) << std::fixed << std::setprecision(2) << avgUs
                  << "\n";
    }
    std::cout << "\n";
}

std::string ProfileReport::toFlameGraph() const {
    // Generate simplified SVG flame graph
    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"1200\" height=\"" 
        << (100 + entries.size() * 30) << "\">\n";
    svg << "<style>rect{stroke:#000;stroke-width:1}text{font-family:monospace;font-size:14}</style>\n";
    svg << "<rect x=\"0\" y=\"0\" width=\"1200\" height=\"" << (100 + entries.size() * 30) 
        << "\" fill=\"#f0f0f0\"/>\n";
    svg << "<text x=\"600\" y=\"30\" text-anchor=\"middle\" font-size=\"18\">"
        << "FluxScript Flame Graph</text>\n";
    
    int y = 60;
    double totalTime = totalDuration.count();
    
    for (const auto& entry : entries) {
        if (entry.callCount == 0) continue;
        
        double width = (totalTime > 0) ? (1100.0 * entry.totalTime.count() / totalTime) : 0;
        if (width < 10) width = 10;
        
        // Color based on self time percentage
        double selfPct = (entry.totalTime.count() > 0) ? 
            (double)entry.selfTime.count() / entry.totalTime.count() : 0;
        int r = 200 + (int)(55 * (1 - selfPct));
        int g = 100 + (int)(100 * selfPct);
        int b = 50;
        
        svg << "<rect x=\"50\" y=\"" << y << "\" width=\"" << width 
            << "\" height=\"25\" fill=\"rgb(" << r << "," << g << "," << b << ")\"/>\n";
        svg << "<text x=\"60\" y=\"" << (y + 18) << "\">" << entry.functionName 
            << " (" << (entry.totalTime.count() / 1e6) << "ms, " 
            << entry.callCount << " calls)</text>\n";
        
        y += 30;
    }
    
    svg << "</svg>\n";
    return svg.str();
}

std::string ProfileReport::toJSON() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"totalDuration_ns\": " << totalDuration.count() << ",\n";
    json << "  \"totalCalls\": " << totalCalls << ",\n";
    json << "  \"functions\": [\n";
    
    bool first = true;
    for (const auto& entry : entries) {
        if (!first) json << ",\n";
        first = false;
        
        json << "    {\n";
        json << "      \"name\": \"" << entry.functionName << "\",\n";
        json << "      \"file\": \"" << entry.file << "\",\n";
        json << "      \"line\": " << entry.line << ",\n";
        json << "      \"calls\": " << entry.callCount << ",\n";
        json << "      \"totalTime_ns\": " << entry.totalTime.count() << ",\n";
        json << "      \"selfTime_ns\": " << entry.selfTime.count() << "\n";
        json << "    }";
    }
    
    json << "\n  ]\n";
    json << "}\n";
    return json.str();
}

} // namespace Profiler
} // namespace Flux
