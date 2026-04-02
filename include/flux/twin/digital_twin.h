#ifndef FLUX_DIGITAL_TWIN_H
#define FLUX_DIGITAL_TWIN_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <chrono>

namespace Flux {
namespace DigitalTwin {

// ============================================================================
// Data Structures
// ============================================================================

// Sensor reading
struct SensorData {
    std::string id;
    std::string type;
    double value;
    double timestamp;
    double uncertainty;
    bool valid;
    
    SensorData() : value(0), timestamp(0), uncertainty(0), valid(true) {}
};

// Actuator command
struct ActuatorCommand {
    std::string id;
    std::string type;
    double setpoint;
    double timestamp;
    bool executed;
    
    ActuatorCommand() : setpoint(0), timestamp(0), executed(false) {}
};

// System state
struct SystemState {
    std::map<std::string, double> variables;
    std::map<std::string, bool> flags;
    double timestamp;
    std::string mode;  // "normal", "fault", "maintenance"
    
    SystemState() : timestamp(0), mode("normal") {}
};

// Fault description
struct FaultDescription {
    std::string id;
    std::string type;
    std::string component;
    double severity;  // 0-1
    double probability;
    std::string description;
    std::vector<std::string> symptoms;
    
    FaultDescription() : severity(0), probability(0) {}
};

// ============================================================================
// Hardware Interface
// ============================================================================

class HardwareInterface {
public:
    HardwareInterface(const std::string& deviceId);
    virtual ~HardwareInterface();
    
    // Connection
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    
    // Data acquisition
    virtual std::vector<SensorData> readSensors() = 0;
    virtual SensorData readSensor(const std::string& sensorId) = 0;
    
    // Control
    virtual void writeActuator(const ActuatorCommand& cmd) = 0;
    virtual void writeActuator(const std::string& actuatorId, double value) = 0;
    
    // Status
    virtual std::string getStatus() const = 0;
    virtual double getTimestamp() const = 0;
    
    // Configuration
    void setSamplingRate(double rate) { m_samplingRate = rate; }
    double getSamplingRate() const { return m_samplingRate; }
    
protected:
    std::string m_deviceId;
    double m_samplingRate;
};

// USB interface implementation
class USBInterface : public HardwareInterface {
public:
    USBInterface(const std::string& deviceId, int baudRate = 115200);
    
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    
    std::vector<SensorData> readSensors() override;
    SensorData readSensor(const std::string& sensorId) override;
    
    void writeActuator(const ActuatorCommand& cmd) override;
    void writeActuator(const std::string& actuatorId, double value) override;
    
    std::string getStatus() const override;
    double getTimestamp() const override;
    
private:
    int m_baudRate;
    int m_portHandle;
};

// Network interface (TCP/IP)
class NetworkInterface : public HardwareInterface {
public:
    NetworkInterface(const std::string& deviceId, const std::string& address, int port);
    
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    
    std::vector<SensorData> readSensors() override;
    SensorData readSensor(const std::string& sensorId) override;
    
    void writeActuator(const ActuatorCommand& cmd) override;
    void writeActuator(const std::string& actuatorId, double value) override;
    
    std::string getStatus() const override;
    double getTimestamp() const override;
    
private:
    std::string m_address;
    int m_port;
    int m_socketHandle;
};

// Simulated hardware for testing
class SimulatedInterface : public HardwareInterface {
public:
    SimulatedInterface(const std::string& deviceId);
    
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;
    
    std::vector<SensorData> readSensors() override;
    SensorData readSensor(const std::string& sensorId) override;
    
    void writeActuator(const ActuatorCommand& cmd) override;
    void writeActuator(const std::string& actuatorId, double value) override;
    
    std::string getStatus() const override;
    double getTimestamp() const override;
    
    // For simulation
    void setModel(std::function<SystemState(double, const SystemState&)> model);
    
private:
    SystemState m_state;
    std::function<SystemState(double, const SystemState&)> m_model;
};

// ============================================================================
// Digital Twin Core
// ============================================================================

class DigitalTwin {
public:
    DigitalTwin(const std::string& twinId);
    ~DigitalTwin();
    
    // Hardware connection
    void setHardwareInterface(std::shared_ptr<HardwareInterface> hw);
    HardwareInterface* getHardwareInterface() { return m_hardware.get(); }
    
    // Model management
    void setModel(std::function<SystemState(double, const SystemState&)> model);
    void setModel(const std::string& modelType);
    
    // Synchronization
    void synchronize();
    void synchronizeWithHardware();
    void synchronizeWithModel();
    
    // Prediction
    SystemState predict(double horizon);
    std::vector<SystemState> predictTrajectory(double horizon, double dt);
    
    // Fault detection
    std::vector<FaultDescription> detectFaults();
    FaultDescription diagnoseFault(const std::string& faultId);
    
    // Prognostics
    double estimateRemainingUsefulLife();
    std::string predictFailureMode();
    
    // Data logging
    void startLogging(const std::string& filename);
    void stopLogging();
    void logState(const SystemState& state);
    
    // State access
    SystemState getCurrentState() const { return m_currentState; }
    SystemState getHardwareState() const { return m_hardwareState; }
    SystemState getModelState() const { return m_modelState; }
    
    // Configuration
    void setSyncInterval(double interval) { m_syncInterval = interval; }
    void setPredictionHorizon(double horizon) { m_predictionHorizon = horizon; }
    
    // Callbacks
    using FaultCallback = std::function<void(const FaultDescription&)>;
    void setFaultCallback(FaultCallback cb) { m_faultCallback = cb; }
    
    using SyncCallback = std::function<void(double error)>;
    void setSyncCallback(SyncCallback cb) { m_syncCallback = cb; }
    
private:
    std::string m_twinId;
    std::shared_ptr<HardwareInterface> m_hardware;
    
    SystemState m_currentState;
    SystemState m_hardwareState;
    SystemState m_modelState;
    
    std::function<SystemState(double, const SystemState&)> m_model;
    
    double m_syncInterval;
    double m_predictionHorizon;
    double m_lastSyncTime;
    
    FaultCallback m_faultCallback;
    SyncCallback m_syncCallback;
    
    FILE* m_logFile;
    
    // Internal methods
    double computeStateError() const;
    void updateFaultDetection();
};

// ============================================================================
// Twin Manager (Multiple Twins)
// ============================================================================

class TwinManager {
public:
    static TwinManager& instance();
    
    // Twin management
    DigitalTwin* createTwin(const std::string& twinId);
    void removeTwin(const std::string& twinId);
    DigitalTwin* getTwin(const std::string& twinId);
    
    // Bulk operations
    void synchronizeAll();
    std::map<std::string, SystemState> getAllStates();
    
    // Monitoring
    std::vector<std::string> getFaultyTwins() const;
    std::vector<FaultDescription> getAllFaults() const;
    
    // Configuration
    void setGlobalSyncInterval(double interval);
    
private:
    TwinManager() = default;
    std::map<std::string, std::unique_ptr<DigitalTwin>> m_twins;
    double m_globalSyncInterval;
};

// ============================================================================
// Pre-built Model Templates
// ============================================================================

class ModelTemplates {
public:
    // Electrical systems
    static std::function<SystemState(double, const SystemState&)> 
    createRLCCircuit(double R, double L, double C);
    
    static std::function<SystemState(double, const SystemState&)> 
    createPowerConverter(const std::string& topology);
    
    // Thermal systems
    static std::function<SystemState(double, const SystemState&)> 
    createThermalModel(double mass, double specificHeat, double h);
    
    // Mechanical systems
    static std::function<SystemState(double, const SystemState&)> 
    createMassSpringDamper(double m, double c, double k);
    
    // Battery model
    static std::function<SystemState(double, const SystemState&)> 
    createBatteryModel(double capacity, double internalResistance);
};

// ============================================================================
// C Interface for FluxScript JIT
// ============================================================================

extern "C" {
    // Twin creation
    void* flux_twin_create(const char* twinId);
    void flux_twin_destroy(void* twin);
    
    // Hardware connection
    void flux_twin_connect_hardware(void* twin, const char* deviceId, 
                                     const char* interfaceType);
    
    // Model setup
    void flux_twin_set_model(void* twin, const char* modelType);
    
    // Synchronization
    void flux_twin_synchronize(void* twin);
    
    // Prediction
    double flux_twin_predict(void* twin, double horizon);
    
    // Fault detection
    int flux_twin_detect_faults(void* twin, char* faultBuffer, int bufferSize);
    
    // RUL estimation
    double flux_twin_get_rul(void* twin);
    
    // State access
    double flux_twin_get_variable(void* twin, const char* name);
    void flux_twin_set_variable(void* twin, const char* name, double value);
}

} // namespace DigitalTwin
} // namespace Flux

#endif // FLUX_DIGITAL_TWIN_H
