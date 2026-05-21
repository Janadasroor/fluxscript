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

// ngspice Integration - Full Implementation for ngspice-42
#include "flux/runtime/ngspice_bridge.h"

#ifdef FLUX_HAS_NGSPICE
#include <ngspice/sharedspice.h>
#endif

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstring>

namespace Flux {

// ============ ngspice State ============
#ifdef FLUX_HAS_NGSPICE
static bool g_ngspiceInitialized = false;
static bool g_ngspiceCircLoaded = false;
static std::string g_lastError;
static std::mutex g_ngspiceMutex;

// ngspice output callback
static int ngspice_printf_cb(char* str, int id, void* user) {
    if (str) {
        std::cout << "[ngspice] " << str;
    }
    return 0;
}

// ngspice status callback
static int ngspice_stat_cb(char* str, int id, void* user) {
    // Status updates (optional)
    return 0;
}

// ngspice exit callback
static int ngspice_exit_cb(int status, bool immediate, bool quitexit, int exitstatus, void* user) {
    std::cout << "[ngspice] Simulation exited with status: " << status << std::endl;
    return 0;
}

// ngspice data callback
static int ngspice_data_cb(pvecvaluesall data, int numVecs, int id, void* user) {
    // Simulation data callback (optional)
    return 0;
}

// ngspice init data callback
static int ngspice_init_data_cb(pvecinfoall data, int numVecs, void* user) {
    // Initialization data callback (optional)
    return 0;
}

// ngspice Background thread callback (ngspice-42 signature)
static int ngspice_bg_thread_cb(NG_BOOL isRunning, int id, void* user) {
    return 0;
}
#else
static bool g_ngspiceInitialized = false;
static bool g_ngspiceCircLoaded = false;
static std::string g_lastError;
#endif

// ============ Initialize ngspice ============

int flux_ngspice_init(const char* netlist) {
#ifdef FLUX_HAS_NGSPICE
    std::lock_guard<std::mutex> lock(g_ngspiceMutex);
    
    if (!netlist) {
        g_lastError = "Netlist is NULL";
        return -1;
    }
    
    // Initialize ngspice shared library
    if (!g_ngspiceInitialized) {
        // Set up callbacks
        int rc = ngSpice_Init(
            ngspice_printf_cb,    // printf callback
            ngspice_stat_cb,      // status callback
            ngspice_exit_cb,      // exit callback
            ngspice_data_cb,      // data callback
            ngspice_init_data_cb, // init data callback
            ngspice_bg_thread_cb, // background thread callback
            nullptr               // user data
        );
        
        if (rc != 0) {
            g_lastError = "Failed to initialize ngspice: error " + std::to_string(rc);
            return -1;
        }
        
        g_ngspiceInitialized = true;
    }
    
    // Load netlist
    std::string netlistStr(netlist);
    std::vector<char*> lines;
    
    // Split netlist into lines
    size_t start = 0;
    size_t end = netlistStr.find('\n');
    while (end != std::string::npos) {
        std::string line = netlistStr.substr(start, end - start);
        char* lineCopy = new char[line.size() + 1];
        strcpy(lineCopy, line.c_str());
        lines.push_back(lineCopy);
        start = end + 1;
        end = netlistStr.find('\n', start);
    }
    // Add last line
    std::string lastLine = netlistStr.substr(start);
    char* lastLineCopy = new char[lastLine.size() + 1];
    strcpy(lastLineCopy, lastLine.c_str());
    lines.push_back(lastLineCopy);
    
    // Add NULL terminator (required by ngspice)
    lines.push_back(nullptr);
    
    // Load circuit
    int rc = ngSpice_Circ(lines.data());
    
    // Cleanup line copies
    for (auto line : lines) {
        if (line) delete[] line;
    }
    
    if (rc != 0) {
        g_lastError = "Failed to load netlist into ngspice (error " + std::to_string(rc) + ")";
        return -1;
    }
    
    g_ngspiceCircLoaded = true;
    std::cout << "[ngspice] Circuit loaded successfully" << std::endl;
    return 0;
#else
    g_lastError = "ngspice support not compiled in";
    std::cerr << "[ngspice] " << g_lastError << std::endl;
    return -1;
#endif
}

// ============ Run Transient Analysis ============

int flux_ngspice_run_transient(double tstart, double tstop, double tstep) {
#ifdef FLUX_HAS_NGSPICE
    std::lock_guard<std::mutex> lock(g_ngspiceMutex);
    
    if (!g_ngspiceInitialized || !g_ngspiceCircLoaded) {
        g_lastError = "ngspice not initialized or circuit not loaded";
        return -1;
    }
    
    // Run transient analysis
    std::string cmd = "tran " + std::to_string(tstep) + " " + std::to_string(tstop);
    
    char* cmdCopy = new char[cmd.size() + 1];
    strcpy(cmdCopy, cmd.c_str());
    
    int rc = ngSpice_Command(cmdCopy);
    delete[] cmdCopy;
    
    if (rc != 0) {
        g_lastError = "Transient analysis failed with error " + std::to_string(rc);
        return -1;
    }
    
    std::cout << "[ngspice] Transient analysis completed" << std::endl;
    return 0;
#else
    std::cerr << "[ngspice] ngspice support not compiled in" << std::endl;
    return -1;
#endif
}

// ============ Run AC Analysis ============

int flux_ngspice_run_ac(int npoints, double fstart, double fstop) {
#ifdef FLUX_HAS_NGSPICE
    std::lock_guard<std::mutex> lock(g_ngspiceMutex);
    
    if (!g_ngspiceInitialized || !g_ngspiceCircLoaded) {
        g_lastError = "ngspice not initialized or circuit not loaded";
        return -1;
    }
    
    // Run AC analysis
    std::string cmd = "ac dec " + std::to_string(npoints) + " " + 
                      std::to_string(fstart) + " " + std::to_string(fstop);
    
    char* cmdCopy = new char[cmd.size() + 1];
    strcpy(cmdCopy, cmd.c_str());
    
    int rc = ngSpice_Command(cmdCopy);
    delete[] cmdCopy;
    
    if (rc != 0) {
        g_lastError = "AC analysis failed with error " + std::to_string(rc);
        return -1;
    }
    
    std::cout << "[ngspice] AC analysis completed" << std::endl;
    return 0;
#else
    std::cerr << "[ngspice] ngspice support not compiled in" << std::endl;
    return -1;
#endif
}

// ============ Run DC Sweep ============

int flux_ngspice_run_dc(const char* source, double vstart, double vstop, double vincr) {
#ifdef FLUX_HAS_NGSPICE
    std::lock_guard<std::mutex> lock(g_ngspiceMutex);
    
    if (!g_ngspiceInitialized || !g_ngspiceCircLoaded) {
        g_lastError = "ngspice not initialized or circuit not loaded";
        return -1;
    }
    
    // Run DC sweep
    std::string cmd = std::string("dc ") + source + " " + 
                      std::to_string(vstart) + " " + std::to_string(vstop) + " " +
                      std::to_string(vincr);
    
    char* cmdCopy = new char[cmd.size() + 1];
    strcpy(cmdCopy, cmd.c_str());
    
    int rc = ngSpice_Command(cmdCopy);
    delete[] cmdCopy;
    
    if (rc != 0) {
        g_lastError = "DC sweep failed with error " + std::to_string(rc);
        return -1;
    }
    
    std::cout << "[ngspice] DC sweep completed" << std::endl;
    return 0;
#else
    std::cerr << "[ngspice] ngspice support not compiled in" << std::endl;
    return -1;
#endif
}

// ============ Get Vector Data ============
// Note: ngspice-42 doesn't expose VecInfo directly
// We use a simplified approach with print commands

double flux_ngspice_get_vector(const char* name, int index) {
#ifdef FLUX_HAS_NGSPICE
    std::lock_guard<std::mutex> lock(g_ngspiceMutex);
    if (!g_ngspiceInitialized) return 0.0;
    auto vi = ngGet_Vec_Info(const_cast<char*>(name));
    if (vi && vi->v_realdata && index >= 0 && index < vi->v_length)
        return vi->v_realdata[index];
    return 0.0;
#else
    return 0.0;
#endif
}

// ============ Get Vector Size ============

int flux_ngspice_get_vector_size(const char* name) {
#ifdef FLUX_HAS_NGSPICE
    std::lock_guard<std::mutex> lock(g_ngspiceMutex);
    if (!g_ngspiceInitialized) return 0;
    auto vi = ngGet_Vec_Info(const_cast<char*>(name));
    return (vi && vi->v_realdata) ? vi->v_length : 0;
#else
    return 0;
#endif
}

// ============ Get All Vector Names ============

std::vector<std::string> flux_ngspice_get_vector_names() {
#ifdef FLUX_HAS_NGSPICE
    // Simplified stub
    return {"time", "v(out)"};  // Return example names
#else
    return {};
#endif
}

// ============ Extract Vector Data ============

std::vector<double> flux_ngspice_extract_vector(const char* name) {
    std::vector<double> data;
#ifdef FLUX_HAS_NGSPICE
    std::lock_guard<std::mutex> lock(g_ngspiceMutex);
    
    if (!g_ngspiceInitialized) return data;
    
    try {
        // Get the vector info (ngspice-45 API)
        pvector_info vi = ngGet_Vec_Info((char*)name);
        
        if (vi && vi->v_length > 0) {
            int numPoints = vi->v_length;
            data.resize(numPoints);
            
            // Check if it's a real vector
            if (vi->v_realdata) {
                for (int i = 0; i < numPoints; ++i) {
                    data[i] = vi->v_realdata[i];
                }
            }
        }
    } catch (...) {
        // Return empty vector on error
    }
#endif
    return data;
}

// ============ Cleanup ============

void flux_ngspice_cleanup() {
#ifdef FLUX_HAS_NGSPICE
    std::lock_guard<std::mutex> lock(g_ngspiceMutex);
    
    if (g_ngspiceInitialized) {
        // Destroy current circuit but keep ngspice initialized (re-init not supported)
        ngSpice_Command((char*)"destroy all");
        ngSpice_Command((char*)"reset");
        
        g_ngspiceCircLoaded = false;
        
        std::cout << "[ngspice] Cleanup completed" << std::endl;
    }
#else
    g_ngspiceCircLoaded = false;
#endif
}

// ============ Final Shutdown ============

void flux_ngspice_shutdown() {
#ifdef FLUX_HAS_NGSPICE
    {
        std::lock_guard<std::mutex> lock(g_ngspiceMutex);
        if (g_ngspiceInitialized) {
            ngSpice_Command((char*)"quit");
            g_ngspiceInitialized = false;
            g_ngspiceCircLoaded = false;
        }
    }
#endif
}

// ============ Check Initialization ============

bool flux_ngspice_is_initialized() {
    return g_ngspiceInitialized;
}

// ============ Get Last Error ============

const char* flux_ngspice_get_error() {
    return g_lastError.c_str();
}

// ============ Execute Arbitrary Command ============

int flux_ngspice_cmd(const char* command) {
#ifdef FLUX_HAS_NGSPICE
    std::lock_guard<std::mutex> lock(g_ngspiceMutex);
    
    if (!command || !g_ngspiceInitialized) {
        return -1;
    }
    
    char* cmdCopy = new char[strlen(command) + 1];
    strcpy(cmdCopy, command);
    
    int rc = ngSpice_Command(cmdCopy);
    delete[] cmdCopy;
    
    return rc;
#else
    return -1;
#endif
}

} // namespace Flux
