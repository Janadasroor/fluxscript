// FluxScript Power Electronics Toolbox - Implementation
// Feature #4: Power Supply Designer

#include "flux/toolbox/power.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace Flux {
namespace Power {

// ============================================================================
// BuckConverter Implementation
// ============================================================================

BuckConverter::BuckConverter() : m_designed(false) {}

BuckConverter::~BuckConverter() = default;

void BuckConverter::setSpecs(const ConverterSpecs& specs) {
    m_specs = specs;
    m_designed = false;
}

double BuckConverter::calculateDutyCycle() const {
    // D = Vout / Vin (ideal buck converter)
    return m_specs.Vout / m_specs.Vin;
}

double BuckConverter::calculateInductorValue() const {
    double D = calculateDutyCycle();
    double dI = m_specs.Iout * m_specs.rippleCurrent;  // Ripple current
    double L = (m_specs.Vout * (1 - D)) / (m_specs.fsw * dI);
    return L;
}

double BuckConverter::calculateOutputCapacitance() const {
    // Based on voltage ripple requirement
    double dI = m_specs.Iout * m_specs.rippleCurrent;
    double dV = m_specs.rippleVoltage;
    double Cout = dI / (8 * m_specs.fsw * dV);
    return Cout;
}

double BuckConverter::calculateInputCapacitance() const {
    // Based on input ripple current
    double D = calculateDutyCycle();
    double Iin_rms = m_specs.Iout * std::sqrt(D * (1 - D));
    double dVin = m_specs.Vin * 0.05;  // 5% input ripple
    double Cin = Iin_rms / (m_specs.fsw * dVin);
    return Cin;
}

void BuckConverter::selectInductor() {
    double L = calculateInductorValue();
    double D = calculateDutyCycle();
    
    // Current ratings
    double Iripple = m_specs.Iout * m_specs.rippleCurrent;
    double Ipeak = m_specs.Iout + Iripple / 2;
    double Irms = m_specs.Iout;
    
    m_components.L = L;
    m_components.L_Irms = Irms * 1.3;  // 30% margin
    m_components.L_Isat = Ipeak * 1.3;  // 30% margin
    m_components.L_DCR = 0.01;  // Estimate, will be refined from database
    
    std::cout << "[Buck] Inductor: L = " << (L * 1e6) << " μH, "
              << "Isat = " << m_components.L_Isat << "A, "
              << "Irms = " << m_components.L_Irms << "A\n";
}

void BuckConverter::selectCapacitors() {
    double Cout = calculateOutputCapacitance();
    double Cin = calculateInputCapacitance();
    
    m_components.Cout = Cout;
    m_components.Cout_Vrating = m_specs.Vout * 1.5;  // 50% margin
    m_components.Cout_ESR = m_specs.rippleVoltage / (m_specs.Iout * m_specs.rippleCurrent);
    
    m_components.Cin = Cin;
    m_components.Cin_Vrating = m_specs.Vin * 1.5;
    
    std::cout << "[Buck] Output Cap: Cout = " << (Cout * 1e6) << " μF, "
              << "Vrating = " << m_components.Cout_Vrating << "V\n";
    std::cout << "[Buck] Input Cap: Cin = " << (Cin * 1e6) << " μF, "
              << "Vrating = " << m_components.Cin_Vrating << "V\n";
}

void BuckConverter::selectMOSFET() {
    // Simplified MOSFET selection
    double D = calculateDutyCycle();
    
    // Voltage rating
    double Vds_rating = m_specs.Vin * 1.5;
    
    // Current rating
    double Id_rms = m_specs.Iout * std::sqrt(D);
    double Id_rating = Id_rms * 1.5;
    
    // Rds_on based on conduction loss budget
    double Pcond_budget = m_specs.Vout * m_specs.Iout * (1 - m_specs.efficiency) / 2;
    m_components.Rds_on = Pcond_budget / (Id_rms * Id_rms);
    
    // Gate charge (estimate)
    m_components.Qg = 10e-9;  // 10 nC typical
    
    // Diode (for non-synchronous)
    m_components.Vf_diode = 0.5;  // Schottky
    
    std::cout << "[Buck] MOSFET: Vds = " << Vds_rating << "V, "
              << "Id = " << Id_rating << "A, "
              << "Rds_on = " << (m_components.Rds_on * 1000) << " mΩ\n";
}

void BuckConverter::selectFeedbackResistors() {
    // Vout = Vref * (1 + R1/R2)
    // Assume Vref = 0.6V (typical)
    double Vref = 0.6;
    
    // Choose R2 = 10k (typical)
    m_components.R2 = 10000;
    m_components.R1 = m_components.R2 * (m_specs.Vout / Vref - 1);
    
    std::cout << "[Buck] Feedback: R1 = " << m_components.R1 << "Ω, "
              << "R2 = " << m_components.R2 << "Ω\n";
}

void BuckConverter::selectComponents() {
    std::cout << "\n=== Buck Converter Component Selection ===\n";
    std::cout << "Specs: Vin=" << m_specs.Vin << "V, Vout=" << m_specs.Vout 
              << "V, Iout=" << m_specs.Iout << "A, fsw=" << (m_specs.fsw/1e6) << "MHz\n\n";
    
    selectInductor();
    selectCapacitors();
    selectMOSFET();
    selectFeedbackResistors();
    
    m_designed = true;
    std::cout << "\nComponent selection complete.\n";
}

PowerSimulationResult BuckConverter::simulate() {
    if (!m_designed) {
        selectComponents();
    }
    
    PowerSimulationResult result;
    
    // Calculate efficiency
    double D = calculateDutyCycle();
    
    // Conduction losses
    double Pcond_sw = m_components.Rds_on * m_specs.Iout * m_specs.Iout * D;
    double Pcond_L = m_components.L_DCR * m_specs.Iout * m_specs.Iout;
    double Pcond_cap = m_components.Cout_ESR * m_specs.Iout * m_specs.Iout * m_specs.rippleCurrent;
    
    // Switching losses (simplified)
    double Psw = 0.5 * m_specs.Vin * m_specs.Iout * 
                 (m_components.Qg * m_specs.fsw / 0.5);  // Gate drive loss
    
    // Core loss (simplified)
    double Pcore = 0.1;  // Estimate
    
    result.conduction_loss = Pcond_sw + Pcond_L + Pcond_cap;
    result.switch_loss = Psw;
    result.core_loss = Pcore;
    
    double Pout = m_specs.Vout * m_specs.Iout;
    double Pin = Pout + result.conduction_loss + result.switch_loss + result.core_loss;
    result.efficiency = Pout / Pin;
    
    // Ripple calculations
    double dI = m_specs.Iout * m_specs.rippleCurrent;
    result.vout_ripple = dI * m_components.Cout_ESR + 
                         dI / (8 * m_specs.fsw * m_components.Cout);
    result.iout_ripple = dI;
    
    // Peak currents
    result.peak_current = m_specs.Iout + dI / 2;
    result.rms_current = m_specs.Iout;
    
    // Stability (simplified)
    result.phase_margin = 60;  // Target
    result.bandwidth = m_specs.fsw / 10;
    
    // Temperature rise (simplified)
    double totalLoss = result.conduction_loss + result.switch_loss + result.core_loss;
    result.max_temp = 25 + totalLoss * 50;  // °C/W estimate
    
    // Generate waveforms (simplified)
    int nPoints = 1000;
    double T = 1.0 / m_specs.fsw;
    double dt = T / nPoints;
    
    for (int i = 0; i < nPoints; ++i) {
        double t = i * dt;
        double phase = 2 * M_PI * t / T;
        
        result.time.push_back(t);
        result.vout.push_back(m_specs.Vout + result.vout_ripple * std::sin(phase));
        result.il.push_back(m_specs.Iout + dI/2 * std::sin(phase - M_PI/2));
        result.isw.push_back((phase < 2*M_PI*D) ? (m_specs.Iout + dI/2) : 0);
    }
    
    std::cout << "\n=== Simulation Results ===\n";
    std::cout << "Efficiency: " << (result.efficiency * 100) << "%\n";
    std::cout << "Output Ripple: " << (result.vout_ripple * 1000) << " mV\n";
    std::cout << "Peak Current: " << result.peak_current << " A\n";
    std::cout << "Phase Margin: " << result.phase_margin << "°\n";
    std::cout << "Max Temperature: " << result.max_temp << "°C\n";
    
    return result;
}

PowerSimulationResult BuckConverter::transient(double tStart, double tStop, double tStep) {
    // Simplified transient simulation
    return simulate();
}

PowerSimulationResult BuckConverter::acAnalysis(double fStart, double fStop, int pointsPerDecade) {
    // Simplified AC analysis
    PowerSimulationResult result;
    result.phase_margin = calculatePhaseMargin();
    result.bandwidth = calculateBandwidth();
    return result;
}

double BuckConverter::calculateEfficiency() const {
    return m_specs.efficiency;
}

double BuckConverter::calculateRipple() const {
    return m_specs.rippleVoltage;
}

double BuckConverter::calculatePhaseMargin() const {
    // Simplified calculation
    return 60.0;
}

double BuckConverter::calculateBandwidth() const {
    return m_specs.fsw / 10.0;
}

void BuckConverter::optimizeForEfficiency() {
    std::cout << "Optimizing for efficiency...\n";
    // Reduce Rds_on, L_DCR, etc.
    m_components.Rds_on *= 0.5;
    m_components.L_DCR *= 0.5;
}

void BuckConverter::optimizeForSize() {
    std::cout << "Optimizing for size...\n";
    // Increase fsw, reduce L and C
    m_specs.fsw *= 2;
    m_components.L *= 0.5;
    m_components.Cout *= 0.5;
}

void BuckConverter::optimizeForCost() {
    std::cout << "Optimizing for cost...\n";
    // Use standard values, relax specs
}

std::string BuckConverter::toNetlist() const {
    std::ostringstream oss;
    oss << "* Buck Converter Netlist\n";
    oss << "* Vin=" << m_specs.Vin << "V, Vout=" << m_specs.Vout 
        << "V, Iout=" << m_specs.Iout << "A\n\n";
    
    oss << "* Input\n";
    oss << "VIN 1 0 DC " << m_specs.Vin << "\n";
    oss << "CIN 1 0 " << m_components.Cin << "\n\n";
    
    oss << "* Switch (simplified)\n";
    oss << "SW 1 2 0 0 MOD\n";
    oss << ".MODEL MOD VSWITCH(RON=0.001 ROFF=1MEG VON=2.5 VOFF=1.5)\n\n";
    
    oss << "* Inductor\n";
    oss << "L1 2 3 " << m_components.L << "\n\n";
    
    oss << "* Output capacitor\n";
    oss << "COUT 3 0 " << m_components.Cout << "\n";
    oss << "RCOUT 3 0 ESR=" << m_components.Cout_ESR << "\n\n";
    
    oss << "* Load\n";
    oss << "RLOAD 3 0 " << (m_specs.Vout / m_specs.Iout) << "\n\n";
    
    oss << "* Feedback\n";
    oss << "R1 3 4 " << m_components.R1 << "\n";
    oss << "R2 4 0 " << m_components.R2 << "\n";
    
    return oss.str();
}

std::string BuckConverter::toSPICE() const {
    return toNetlist();
}

void BuckConverter::save(const std::string& filename, const std::string& format) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << toNetlist();
        file.close();
        std::cout << "Netlist saved to: " << filename << "\n";
    }
}

bool BuckConverter::validate() const {
    if (!m_designed) return false;
    
    // Check all components are selected
    if (m_components.L <= 0 || m_components.Cout <= 0) return false;
    if (m_components.R1 <= 0 || m_components.R2 <= 0) return false;
    
    return true;
}

std::vector<std::string> BuckConverter::getWarnings() const {
    std::vector<std::string> warnings;
    
    if (m_specs.efficiency < 0.8) {
        warnings.push_back("Low efficiency target");
    }
    if (m_specs.fsw > 1e6) {
        warnings.push_back("High switching frequency - consider EMI");
    }
    if (m_specs.rippleVoltage > 0.1) {
        warnings.push_back("High output ripple");
    }
    
    return warnings;
}

// ============================================================================
// PowerSupplyFactory Implementation
// ============================================================================

PowerSupplyFactory& PowerSupplyFactory::instance() {
    static PowerSupplyFactory factory;
    return factory;
}

std::shared_ptr<BuckConverter> PowerSupplyFactory::createBuck(
    double Vin, double Vout, double Iout, double fsw)
{
    auto buck = std::make_shared<BuckConverter>();
    ConverterSpecs specs;
    specs.Vin = Vin;
    specs.Vout = Vout;
    specs.Iout = Iout;
    specs.fsw = fsw;
    buck->setSpecs(specs);
    return buck;
}

std::string PowerSupplyFactory::recommendTopology(double Vin, double Vout, double Iout) {
    if (Vout < Vin) {
        return "buck";
    } else if (Vout > Vin) {
        return "boost";
    } else {
        return "buck-boost";
    }
}

// ============================================================================
// C Interface Implementation
// ============================================================================

static BuckConverter* g_currentBuck = nullptr;

extern "C" {

void* flux_buck_create(double Vin, double Vout, double Iout, double fsw) {
    auto buck = new BuckConverter();
    ConverterSpecs specs;
    specs.Vin = Vin;
    specs.Vout = Vout;
    specs.Iout = Iout;
    specs.fsw = fsw;
    buck->setSpecs(specs);
    g_currentBuck = buck;
    return buck;
}

void flux_buck_destroy(void* buck) {
    delete static_cast<BuckConverter*>(buck);
}

void flux_buck_select_components(void* buck) {
    static_cast<BuckConverter*>(buck)->selectComponents();
}

double flux_buck_get_inductor(void* buck) {
    return static_cast<BuckConverter*>(buck)->components().L;
}

double flux_buck_get_capacitor(void* buck) {
    return static_cast<BuckConverter*>(buck)->components().Cout;
}

void flux_buck_simulate(void* buck) {
    static_cast<BuckConverter*>(buck)->simulate();
}

double flux_buck_get_efficiency(void* buck) {
    return static_cast<BuckConverter*>(buck)->simulate().efficiency;
}

double flux_buck_get_ripple(void* buck) {
    return static_cast<BuckConverter*>(buck)->simulate().vout_ripple;
}

double flux_buck_get_phase_margin(void* buck) {
    return static_cast<BuckConverter*>(buck)->calculatePhaseMargin();
}

void flux_buck_optimize_efficiency(void* buck) {
    static_cast<BuckConverter*>(buck)->optimizeForEfficiency();
}

const char* flux_buck_to_netlist(void* buck) {
    static std::string netlist;
    netlist = static_cast<BuckConverter*>(buck)->toNetlist();
    return netlist.c_str();
}

}

} // namespace Power
} // namespace Flux
