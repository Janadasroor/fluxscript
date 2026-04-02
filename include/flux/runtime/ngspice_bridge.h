#ifndef FLUX_NGSPICE_BRIDGE_H
#define FLUX_NGSPICE_BRIDGE_H

namespace Flux {

// ============ State Management ============
double flux_state_get(const char* name, double defaultVal);
double flux_state_set(const char* name, double value);

// ============ Initial Conditions ============
double flux_set_ic(const char* node, double value);

// ============ Simulation Globals ============
double flux_get_time();
void flux_set_time(double time);
double flux_get_dt();
void flux_set_dt(double dt);
double flux_get_temp();
void flux_set_temp(double temp);

// ============ ngspice Integration ============
int flux_ngspice_init(const char* netlist);
int flux_ngspice_run_transient(double tstart, double tstop, double tstep);
double flux_ngspice_get_vector(const char* name, int index);
int flux_ngspice_get_vector_size(const char* name);
void flux_ngspice_cleanup();
bool flux_ngspice_is_initialized();

// ============ Parameter Management ============
double flux_get_parameter_value(const char* name);
void flux_set_parameter_value(const char* name, double value);

} // namespace Flux

#endif // FLUX_NGSPICE_BRIDGE_H
