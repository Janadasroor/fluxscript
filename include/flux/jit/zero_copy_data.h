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

// ============================================================================
// FluxScript Zero-Copy Data Interface - Ngspice Shared Memory Integration
// Direct connection between FluxScript V()/I() and ngspice memory buffers
// ============================================================================

#ifndef FLUX_ZERO_COPY_DATA_H
#define FLUX_ZERO_COPY_DATA_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>

namespace Flux {

// Memory buffer descriptor for zero-copy access
struct BufferDescriptor {
    std::string name;           // Buffer name (e.g., "node_voltages", "branch_currents")
    void* data_ptr = nullptr;   // Direct pointer to ngspice memory
    size_t size_bytes = 0;      // Buffer size in bytes
    size_t element_count = 0;   // Number of elements
    size_t element_size = 0;    // Size of each element (typically 8 for double)
    bool is_writeable = false;  // Can we write to this buffer?
    bool is_valid = false;      // Is the buffer currently valid?
};

// Node mapping for direct access
struct NodeMapping {
    std::string node_name;      // SPICE node name (e.g., "V(out)", "I(V1)")
    int vector_index = -1;      // Index in ngspice vector
    double* direct_ptr = nullptr; // Direct pointer to memory location
    BufferDescriptor* buffer = nullptr; // Which buffer this belongs to
    bool is_voltage = true;     // true for voltage, false for current
};

// Zero-copy data manager
class ZeroCopyDataManager {
public:
    static ZeroCopyDataManager& instance();

    // Initialize/finalize
    void initialize();
    void finalize();
    bool isInitialized() const { return m_initialized; }

    // Buffer management
    bool registerBuffer(const std::string& name, void* data_ptr, size_t element_count, 
                       bool is_writeable, std::string* error = nullptr);
    bool unregisterBuffer(const std::string& name);
    BufferDescriptor* getBuffer(const std::string& name);
    const BufferDescriptor* getBuffer(const std::string& name) const;
    std::vector<std::string> getBufferNames() const;

    // Node mapping (high-level API)
    bool mapNode(const std::string& node_name, const std::string& buffer_name, 
                int vector_index, std::string* error = nullptr);
    bool unmapNode(const std::string& node_name);
    NodeMapping* getNodeMapping(const std::string& node_name);
    const NodeMapping* getNodeMapping(const std::string& node_name) const;

    // Zero-copy read/write operations
    double readVoltage(const std::string& node_name);
    double readCurrent(const std::string& branch_name);
    void writeVoltage(const std::string& node_name, double value);
    void writeCurrent(const std::string& branch_name, double value);

    // Bulk operations (for vectorized access)
    std::vector<double> readAllVoltages() const;
    void writeAllVoltages(const std::vector<double>& values);
    std::vector<double> readAllCurrents() const;
    void writeAllCurrents(const std::vector<double>& values);

    // Direct pointer access (for advanced users)
    double* getDirectPointer(const std::string& node_name);
    BufferDescriptor* getBufferDescriptor(const std::string& buffer_name);

    // Statistics
    size_t getReadCount() const { return m_read_count; }
    size_t getWriteCount() const { return m_write_count; }
    size_t getMappedNodeCount() const { return m_node_mappings.size(); }
    void resetStatistics();

    // Performance monitoring
    double getAverageReadLatencyNs() const;
    double getAverageWriteLatencyNs() const;

private:
    ZeroCopyDataManager() = default;
    ~ZeroCopyDataManager() = default;

    std::map<std::string, BufferDescriptor> m_buffers;
    std::map<std::string, NodeMapping> m_node_mappings;
    mutable std::mutex m_mutex;
    bool m_initialized = false;
    
    // Statistics
    std::atomic<size_t> m_read_count{0};
    std::atomic<size_t> m_write_count{0};
    mutable std::mutex m_stats_mutex;
    double m_total_read_latency_ns = 0.0;
    double m_total_write_latency_ns = 0.0;
};

// V() and I() runtime functions (called by JIT-compiled code)
extern "C" {
    // Voltage access - zero-copy read from ngspice memory
    double flux_V(const char* node_name);
    
    // Current access - zero-copy read from ngspice memory
    double flux_I(const char* branch_name);
    
    // Direct pointer access (for advanced usage)
    double* flux_V_ptr(const char* node_name);
    double* flux_I_ptr(const char* branch_name);
    
    // Bulk operations
    int flux_get_all_voltages(double* buffer, int max_count);
    int flux_get_all_currents(double* buffer, int max_count);
    int flux_set_all_voltages(const double* buffer, int count);
    int flux_set_all_currents(const double* buffer, int count);
}

// Helper class for automatic node binding
class NodeBinder {
public:
    NodeBinder(const std::string& node_name);
    ~NodeBinder();

    double read() const;
    void write(double value) const;
    double* ptr() const;
    bool isValid() const;

private:
    std::string m_node_name;
    NodeMapping* m_mapping = nullptr;
};

} // namespace Flux

#endif // FLUX_ZERO_COPY_DATA_H
