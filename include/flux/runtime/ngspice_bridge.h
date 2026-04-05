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

#ifndef FLUX_NGSPICE_BRIDGE_H
#define FLUX_NGSPICE_BRIDGE_H

#include <string>
#include <vector>

namespace Flux {

// ============ ngspice Integration ============
int flux_ngspice_init(const char* netlist);
int flux_ngspice_run_transient(double tstart, double tstop, double tstep);
int flux_ngspice_run_ac(int npoints, double fstart, double fstop);
int flux_ngspice_run_dc(const char* source, double vstart, double vstop, double vincr);
int flux_ngspice_cmd(const char* command);
double flux_ngspice_get_vector(const char* name, int index);
int flux_ngspice_get_vector_size(const char* name);
std::vector<std::string> flux_ngspice_get_vector_names();
std::vector<double> flux_ngspice_extract_vector(const char* name);
void flux_ngspice_cleanup();
bool flux_ngspice_is_initialized();
const char* flux_ngspice_get_error();

} // namespace Flux

#endif // FLUX_NGSPICE_BRIDGE_H
