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

#ifndef FLUX_PYTHON_BRIDGE_H
#define FLUX_PYTHON_BRIDGE_H

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

// Forward declare Python objects
struct _object;
typedef _object PyObject;

struct _PyStateDef;
typedef _PyStateDef PyStateDef;

namespace Flux {
namespace Python {

// Python value wrapper
class PyValue {
public:
    PyValue();
    PyValue(PyObject* ptr);
    PyValue(double value);
    PyValue(int value);
    PyValue(const std::string& value);
    PyValue(const std::vector<double>& values);
    ~PyValue();
    
    // Type conversion
    bool isNone() const;
    bool isNumber() const;
    bool isString() const;
    bool isList() const;
    bool isDict() const;
    bool isCallable() const;
    
    // Get as C++ types
    double asDouble() const;
    int asInt() const;
    std::string asString() const;
    std::vector<double> asVector() const;
    
    // Get item/attribute
    PyValue getItem(int index) const;
    PyValue getItem(const std::string& key) const;
    PyValue getAttr(const std::string& name) const;
    
    // Call
    PyValue call() const;
    PyValue call(const PyValue& arg) const;
    PyValue call(const std::vector<PyValue>& args) const;
    
    // Raw pointer (use with caution)
    PyObject* ptr() const { return m_ptr; }
    
private:
    PyObject* m_ptr;
};

// Python module
class PyModule {
public:
    PyModule(const std::string& name);
    ~PyModule();
    
    bool isValid() const { return m_valid; }
    
    // Get attribute
    PyValue getAttr(const std::string& name) const;
    
    // Call function
    PyValue call(const std::string& func) const;
    PyValue call(const std::string& func, const PyValue& arg) const;
    PyValue call(const std::string& func, const std::vector<PyValue>& args) const;
    
    // Set attribute
    void setAttr(const std::string& name, const PyValue& value);
    
private:
    std::string m_name;
    PyObject* m_module;
    bool m_valid;
};

// Python interpreter manager
class PythonBridge {
public:
    static PythonBridge& instance();
    
    // Initialize/finalize
    void initialize();
    void finalize();
    bool isInitialized() const { return m_initialized; }
    
    // Import module
    PyModule import(const std::string& name);
    
    // Execute code
    PyValue eval(const std::string& code);
    void exec(const std::string& code);
    
    // Set path
    void addPath(const std::string& path);
    
    // Get sys.modules
    std::vector<std::string> getLoadedModules() const;
    
    // Error handling
    std::string getLastError() const;
    void clearError();
    bool hasError() const;
    
    // GIL management
    class GILState {
    public:
        GILState();
        ~GILState();
    private:
        void* m_state;
    };
    
private:
    PythonBridge();
    ~PythonBridge();
    PythonBridge(const PythonBridge&) = delete;
    PythonBridge& operator=(const PythonBridge&) = delete;
    
    bool m_initialized;
    bool m_mainThread;
};

// Convenience functions
namespace Py {
    inline PyValue eval(const std::string& code) {
        return PythonBridge::instance().eval(code);
    }
    
    inline void exec(const std::string& code) {
        PythonBridge::instance().exec(code);
    }
    
    inline PyModule import(const std::string& name) {
        return PythonBridge::instance().import(name);
    }
}

// C interface for FluxScript JIT
extern "C" {
    void flux_py_init();
    void flux_py_import(const char* module_name, const char* alias);
    double flux_py_call(const char* module, const char* func, double* args, int nargs);
    void flux_py_call_array(const char* module, const char* func, 
                            double* in_args, int nin, 
                            double* out_args, int nout);
}

} // namespace Python
} // namespace Flux

#endif // FLUX_PYTHON_BRIDGE_H
