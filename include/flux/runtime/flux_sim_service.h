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

#ifndef FLUX_SIM_SERVICE_H
#define FLUX_SIM_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Interface for simulation services provided by the host (VioSpice).
 */
typedef struct {
    double (*get_voltage)(const char* node);
    double (*get_current)(const char* branch);
    double (*get_parameter)(const char* name);
    void   (*set_parameter)(const char* name, double value);
    
    void   (*sim_run)();
    void   (*sim_stop)();
    void   (*sim_pause)(int pause);
    
    void   (*log_message)(const char* msg);
} FluxSimulationService;

/**
 * @brief Global pointer to the active simulation service.
 * FluxScript shims will call through this pointer.
 */
extern FluxSimulationService* g_flux_sim_service;

#ifdef __cplusplus
}
#endif

#endif // FLUX_SIM_SERVICE_H
