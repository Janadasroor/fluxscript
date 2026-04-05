// ============================================================================
// FluxScript Zero-Copy Data Interface Implementation
// Direct connection between FluxScript V()/I() and ngspice memory buffers
// ============================================================================

#include "flux/jit/zero_copy_data.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cstring>

namespace Flux {

// ============================================================================
// ZeroCopyDataManager Singleton
// ============================================================================

ZeroCopyDataManager& ZeroCopyDataManager::instance() {
    static ZeroCopyDataManager instance;
    return instance;
}

void ZeroCopyDataManager::initialize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_initialized) {
        return;
    }

    m_buffers.clear();
    m_node_mappings.clear();
    m_read_count = 0;
    m_write_count = 0;
    m_total_read_latency_ns = 0.0;
    m_total_write_latency_ns = 0.0;
    m_initialized = true;

    std::cout << "[ZeroCopyDataManager] Initialized" << std::endl;
}

void ZeroCopyDataManager::finalize() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_buffers.clear();
    m_node_mappings.clear();
    m_initialized = false;

    std::cout << "[ZeroCopyDataManager] Finalized" << std::endl;
}

// ============================================================================
// Buffer Management
// ============================================================================

bool ZeroCopyDataManager::registerBuffer(const std::string& name, void* data_ptr, 
                                         size_t element_count, bool is_writeable, 
                                         std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        if (error) *error = "ZeroCopyDataManager not initialized";
        return false;
    }

    if (m_buffers.count(name)) {
        if (error) *error = "Buffer already registered: " + name;
        return false;
    }

    if (!data_ptr) {
        if (error) *error = "Invalid data pointer for buffer: " + name;
        return false;
    }

    BufferDescriptor desc;
    desc.name = name;
    desc.data_ptr = data_ptr;
    desc.element_count = element_count;
    desc.element_size = sizeof(double);
    desc.size_bytes = element_count * sizeof(double);
    desc.is_writeable = is_writeable;
    desc.is_valid = true;

    m_buffers[name] = desc;

    std::cout << "[ZeroCopyDataManager] Registered buffer: " << name 
              << " (" << element_count << " elements, " 
              << (is_writeable ? "RW" : "RO") << ")" << std::endl;

    return true;
}

bool ZeroCopyDataManager::unregisterBuffer(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_buffers.find(name);
    if (it == m_buffers.end()) {
        return false;
    }

    // Invalidate the buffer
    it->second.is_valid = false;
    it->second.data_ptr = nullptr;

    m_buffers.erase(it);

    std::cout << "[ZeroCopyDataManager] Unregistered buffer: " << name << std::endl;
    return true;
}

BufferDescriptor* ZeroCopyDataManager::getBuffer(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_buffers.find(name);
    if (it == m_buffers.end()) {
        return nullptr;
    }
    return &it->second;
}

const BufferDescriptor* ZeroCopyDataManager::getBuffer(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_buffers.find(name);
    if (it == m_buffers.end()) {
        return nullptr;
    }
    return &it->second;
}

std::vector<std::string> ZeroCopyDataManager::getBufferNames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> names;
    for (const auto& [name, desc] : m_buffers) {
        names.push_back(name);
    }
    return names;
}

// ============================================================================
// Node Mapping
// ============================================================================

bool ZeroCopyDataManager::mapNode(const std::string& node_name, const std::string& buffer_name,
                                  int vector_index, std::string* error) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_initialized) {
        if (error) *error = "ZeroCopyDataManager not initialized";
        return false;
    }

    auto buf_it = m_buffers.find(buffer_name);
    if (buf_it == m_buffers.end()) {
        if (error) *error = "Buffer not found: " + buffer_name;
        return false;
    }

    if (vector_index < 0 || static_cast<size_t>(vector_index) >= buf_it->second.element_count) {
        if (error) *error = "Vector index out of range: " + std::to_string(vector_index);
        return false;
    }

    NodeMapping mapping;
    mapping.node_name = node_name;
    mapping.vector_index = vector_index;
    mapping.buffer = &buf_it->second;
    mapping.direct_ptr = reinterpret_cast<double*>(buf_it->second.data_ptr) + vector_index;
    mapping.is_voltage = (buffer_name.find("voltage") != std::string::npos || 
                         buffer_name.find("node") != std::string::npos);

    m_node_mappings[node_name] = mapping;

    std::cout << "[ZeroCopyDataManager] Mapped node: " << node_name 
              << " -> " << buffer_name << "[" << vector_index << "]" << std::endl;

    return true;
}

bool ZeroCopyDataManager::unmapNode(const std::string& node_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_node_mappings.erase(node_name) > 0;
}

NodeMapping* ZeroCopyDataManager::getNodeMapping(const std::string& node_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_node_mappings.find(node_name);
    if (it == m_node_mappings.end()) {
        return nullptr;
    }
    return &it->second;
}

const NodeMapping* ZeroCopyDataManager::getNodeMapping(const std::string& node_name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_node_mappings.find(node_name);
    if (it == m_node_mappings.end()) {
        return nullptr;
    }
    return &it->second;
}

// ============================================================================
// Zero-Copy Read/Write Operations
// ============================================================================

double ZeroCopyDataManager::readVoltage(const std::string& node_name) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_node_mappings.find(node_name);
    if (it == m_node_mappings.end() || !it->second.direct_ptr) {
        return 0.0;
    }

    double value = *it->second.direct_ptr;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double latency = std::chrono::duration<double, std::nano>(end_time - start_time).count();
    {
        std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
        m_total_read_latency_ns += latency;
    }
    m_read_count++;

    return value;
}

double ZeroCopyDataManager::readCurrent(const std::string& branch_name) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_node_mappings.find(branch_name);
    if (it == m_node_mappings.end() || !it->second.direct_ptr) {
        return 0.0;
    }

    double value = *it->second.direct_ptr;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double latency = std::chrono::duration<double, std::nano>(end_time - start_time).count();
    {
        std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
        m_total_read_latency_ns += latency;
    }
    m_read_count++;

    return value;
}

void ZeroCopyDataManager::writeVoltage(const std::string& node_name, double value) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_node_mappings.find(node_name);
    if (it == m_node_mappings.end() || !it->second.direct_ptr) {
        return;
    }

    if (!it->second.buffer->is_writeable) {
        std::cerr << "[ZeroCopyDataManager] Buffer is read-only: " << node_name << std::endl;
        return;
    }

    *it->second.direct_ptr = value;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double latency = std::chrono::duration<double, std::nano>(end_time - start_time).count();
    {
        std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
        m_total_write_latency_ns += latency;
    }
    m_write_count++;
}

void ZeroCopyDataManager::writeCurrent(const std::string& branch_name, double value) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_node_mappings.find(branch_name);
    if (it == m_node_mappings.end() || !it->second.direct_ptr) {
        return;
    }

    if (!it->second.buffer->is_writeable) {
        std::cerr << "[ZeroCopyDataManager] Buffer is read-only: " << branch_name << std::endl;
        return;
    }

    *it->second.direct_ptr = value;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    double latency = std::chrono::duration<double, std::nano>(end_time - start_time).count();
    {
        std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
        m_total_write_latency_ns += latency;
    }
    m_write_count++;
}

// ============================================================================
// Bulk Operations
// ============================================================================

std::vector<double> ZeroCopyDataManager::readAllVoltages() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<double> values;

    // Find voltage buffer
    auto it = m_buffers.find("node_voltages");
    if (it == m_buffers.end() || !it->second.data_ptr) {
        return values;
    }

    const double* data = static_cast<const double*>(it->second.data_ptr);
    values.assign(data, data + it->second.element_count);

    return values;
}

void ZeroCopyDataManager::writeAllVoltages(const std::vector<double>& values) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_buffers.find("node_voltages");
    if (it == m_buffers.end() || !it->second.data_ptr || !it->second.is_writeable) {
        return;
    }

    size_t count = std::min(values.size(), it->second.element_count);
    double* data = static_cast<double*>(it->second.data_ptr);
    std::memcpy(data, values.data(), count * sizeof(double));
}

std::vector<double> ZeroCopyDataManager::readAllCurrents() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<double> values;

    auto it = m_buffers.find("branch_currents");
    if (it == m_buffers.end() || !it->second.data_ptr) {
        return values;
    }

    const double* data = static_cast<const double*>(it->second.data_ptr);
    values.assign(data, data + it->second.element_count);

    return values;
}

void ZeroCopyDataManager::writeAllCurrents(const std::vector<double>& values) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_buffers.find("branch_currents");
    if (it == m_buffers.end() || !it->second.data_ptr || !it->second.is_writeable) {
        return;
    }

    size_t count = std::min(values.size(), it->second.element_count);
    double* data = static_cast<double*>(it->second.data_ptr);
    std::memcpy(data, values.data(), count * sizeof(double));
}

// ============================================================================
// Direct Pointer Access
// ============================================================================

double* ZeroCopyDataManager::getDirectPointer(const std::string& node_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_node_mappings.find(node_name);
    if (it == m_node_mappings.end()) {
        return nullptr;
    }
    return it->second.direct_ptr;
}

BufferDescriptor* ZeroCopyDataManager::getBufferDescriptor(const std::string& buffer_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_buffers.find(buffer_name);
    if (it == m_buffers.end()) {
        return nullptr;
    }
    return &it->second;
}

// ============================================================================
// Statistics
// ============================================================================

void ZeroCopyDataManager::resetStatistics() {
    m_read_count = 0;
    m_write_count = 0;
    m_total_read_latency_ns = 0.0;
    m_total_write_latency_ns = 0.0;
}

double ZeroCopyDataManager::getAverageReadLatencyNs() const {
    if (m_read_count == 0) return 0.0;
    return m_total_read_latency_ns / m_read_count;
}

double ZeroCopyDataManager::getAverageWriteLatencyNs() const {
    if (m_write_count == 0) return 0.0;
    return m_total_write_latency_ns / m_write_count;
}

// ============================================================================
// C Interface - V() and I() Runtime Functions
// ============================================================================

double flux_V(const char* node_name) {
    if (!node_name) return 0.0;
    return ZeroCopyDataManager::instance().readVoltage(node_name);
}

double flux_I(const char* branch_name) {
    if (!branch_name) return 0.0;
    return ZeroCopyDataManager::instance().readCurrent(branch_name);
}

double* flux_V_ptr(const char* node_name) {
    if (!node_name) return nullptr;
    return ZeroCopyDataManager::instance().getDirectPointer(node_name);
}

double* flux_I_ptr(const char* branch_name) {
    if (!branch_name) return nullptr;
    return ZeroCopyDataManager::instance().getDirectPointer(branch_name);
}

int flux_get_all_voltages(double* buffer, int max_count) {
    if (!buffer || max_count <= 0) return 0;
    auto voltages = ZeroCopyDataManager::instance().readAllVoltages();
    int count = std::min(static_cast<int>(voltages.size()), max_count);
    std::memcpy(buffer, voltages.data(), count * sizeof(double));
    return count;
}

int flux_get_all_currents(double* buffer, int max_count) {
    if (!buffer || max_count <= 0) return 0;
    auto currents = ZeroCopyDataManager::instance().readAllCurrents();
    int count = std::min(static_cast<int>(currents.size()), max_count);
    std::memcpy(buffer, currents.data(), count * sizeof(double));
    return count;
}

int flux_set_all_voltages(const double* buffer, int count) {
    if (!buffer || count <= 0) return 0;
    std::vector<double> values(buffer, buffer + count);
    ZeroCopyDataManager::instance().writeAllVoltages(values);
    return count;
}

int flux_set_all_currents(const double* buffer, int count) {
    if (!buffer || count <= 0) return 0;
    std::vector<double> values(buffer, buffer + count);
    ZeroCopyDataManager::instance().writeAllCurrents(values);
    return count;
}

// ============================================================================
// NodeBinder Implementation
// ============================================================================

NodeBinder::NodeBinder(const std::string& node_name) : m_node_name(node_name) {
    m_mapping = ZeroCopyDataManager::instance().getNodeMapping(node_name);
}

NodeBinder::~NodeBinder() {
    // Nothing to clean up - mappings are managed by ZeroCopyDataManager
}

double NodeBinder::read() const {
    if (!m_mapping || !m_mapping->direct_ptr) return 0.0;
    return *m_mapping->direct_ptr;
}

void NodeBinder::write(double value) const {
    if (!m_mapping || !m_mapping->direct_ptr) return;
    if (!m_mapping->buffer->is_writeable) return;
    *m_mapping->direct_ptr = value;
}

double* NodeBinder::ptr() const {
    if (!m_mapping) return nullptr;
    return m_mapping->direct_ptr;
}

bool NodeBinder::isValid() const {
    return m_mapping && m_mapping->direct_ptr && m_mapping->buffer && m_mapping->buffer->is_valid;
}

} // namespace Flux
