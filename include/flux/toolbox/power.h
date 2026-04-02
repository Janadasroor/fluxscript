#ifndef FLUX_TOOLBOX_POWER_H
#define FLUX_TOOLBOX_POWER_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cmath>

namespace Flux {
namespace Power {

// ============================================================================
// Power Converter Topologies
// ============================================================================

enum class ConverterType {
    Buck,
    Boost,
    BuckBoost,
    Flyback,
    Forward,
    Cuk,
    Sepic
};

// Converter specifications
struct ConverterSpecs {
    double Vin;           // Input voltage (V)
    double Vout;          // Output voltage (V)
    double Iout;          // Output current (A)
    double fsw;           // Switching frequency (Hz)
    double rippleCurrent; // Inductor ripple current ratio
    double rippleVoltage; // Output voltage ripple (V)
    double efficiency;    // Target efficiency
    
    ConverterSpecs() : Vin(12), Vout(3.3), Iout(3), fsw(500e3),
                       rippleCurrent(0.3), rippleVoltage(0.05), efficiency(0.85) {}
};

// Component values
struct ComponentValues {
    // Inductor
    double L;             // Inductance (H)
    double L_Irms;        // RMS current rating (A)
    double L_Isat;        // Saturation current (A)
    double L_DCR;         // DC resistance (Ω)
    
    // Output capacitor
    double Cout;          // Capacitance (F)
    double Cout_Vrating;  // Voltage rating (V)
    double Cout_ESR;      // Equivalent series resistance (Ω)
    
    // Input capacitor
    double Cin;           // Capacitance (F)
    double Cin_Vrating;   // Voltage rating (V)
    
    // Switching elements
    double Rds_on;        // MOSFET on-resistance (Ω)
    double Qg;            // Gate charge (C)
    double Vf_diode;      // Diode forward voltage (V)
    
    // Feedback
    double R1;            // Feedback resistor 1 (Ω)
    double R2;            // Feedback resistor 2 (Ω)
    
    ComponentValues() : L(0), L_Irms(0), L_Isat(0), L_DCR(0),
                        Cout(0), Cout_Vrating(0), Cout_ESR(0),
                        Cin(0), Cin_Vrating(0),
                        Rds_on(0), Qg(0), Vf_diode(0),
                        R1(0), R2(0) {}
};

// Simulation results
struct PowerSimulationResult {
    double efficiency;
    double vout_ripple;
    double iout_ripple;
    double peak_current;
    double rms_current;
    double switch_loss;
    double conduction_loss;
    double core_loss;
    double max_temp;
    double phase_margin;
    double bandwidth;
    
    std::vector<double> time;
    std::vector<double> vout;
    std::vector<double> il;
    std::vector<double> isw;
    
    PowerSimulationResult() : efficiency(0), vout_ripple(0), iout_ripple(0),
                               peak_current(0), rms_current(0),
                               switch_loss(0), conduction_loss(0), core_loss(0),
                               max_temp(0), phase_margin(0), bandwidth(0) {}
};

// ============================================================================
// Buck Converter Designer
// ============================================================================

class BuckConverter {
public:
    BuckConverter();
    ~BuckConverter();
    
    // Design specification
    void setSpecs(const ConverterSpecs& specs);
    const ConverterSpecs& specs() const { return m_specs; }
    
    // Automatic component selection
    void selectComponents();
    void selectInductor();
    void selectCapacitors();
    void selectMOSFET();
    void selectFeedbackResistors();
    
    // Get component values
    const ComponentValues& components() const { return m_components; }
    
    // Simulation
    PowerSimulationResult simulate();
    PowerSimulationResult transient(double tStart, double tStop, double tStep);
    PowerSimulationResult acAnalysis(double fStart, double fStop, int pointsPerDecade);
    
    // Optimization
    void optimizeForEfficiency();
    void optimizeForSize();
    void optimizeForCost();
    
    // Analysis
    double calculateEfficiency() const;
    double calculateRipple() const;
    double calculatePhaseMargin() const;
    double calculateBandwidth() const;
    
    // Export
    std::string toNetlist() const;
    std::string toSPICE() const;
    void save(const std::string& filename, const std::string& format = "spice");
    
    // Validation
    bool validate() const;
    std::vector<std::string> getWarnings() const;
    
private:
    ConverterSpecs m_specs;
    ComponentValues m_components;
    bool m_designed;
    
    // Design equations
    double calculateDutyCycle() const;
    double calculateInductorValue() const;
    double calculateOutputCapacitance() const;
    double calculateInputCapacitance() const;
};

// ============================================================================
// Boost Converter Designer
// ============================================================================

class BoostConverter {
public:
    BoostConverter();
    ~BoostConverter();
    
    void setSpecs(const ConverterSpecs& specs);
    void selectComponents();
    PowerSimulationResult simulate();
    
    const ComponentValues& components() const { return m_components; }
    
private:
    ConverterSpecs m_specs;
    ComponentValues m_components;
};

// ============================================================================
// Power Supply Factory
// ============================================================================

class PowerSupplyFactory {
public:
    static PowerSupplyFactory& instance();
    
    // Create converter by type
    std::shared_ptr<BuckConverter> createBuck(
        double Vin, double Vout, double Iout, double fsw = 500e3);
    
    std::shared_ptr<BoostConverter> createBoost(
        double Vin, double Vout, double Iout, double fsw = 500e3);
    
    // Auto-select topology based on specs
    std::string recommendTopology(double Vin, double Vout, double Iout);
    
    // Component database
    std::vector<std::map<std::string, double>> getInductorDatabase() const;
    std::vector<std::map<std::string, double>> getCapacitorDatabase() const;
    std::vector<std::map<std::string, double>> getMOSFETDatabase() const;
    
private:
    PowerSupplyFactory() = default;
};

// ============================================================================
// C Interface for FluxScript JIT
// ============================================================================

extern "C" {
    // Buck converter creation
    void* flux_buck_create(double Vin, double Vout, double Iout, double fsw);
    void flux_buck_destroy(void* buck);
    
    // Component selection
    void flux_buck_select_components(void* buck);
    double flux_buck_get_inductor(void* buck);
    double flux_buck_get_capacitor(void* buck);
    
    // Simulation
    void flux_buck_simulate(void* buck);
    double flux_buck_get_efficiency(void* buck);
    double flux_buck_get_ripple(void* buck);
    double flux_buck_get_phase_margin(void* buck);
    
    // Optimization
    void flux_buck_optimize_efficiency(void* buck);
    
    // Export
    const char* flux_buck_to_netlist(void* buck);
}

} // namespace Power
} // namespace Flux

#endif // FLUX_TOOLBOX_POWER_H
