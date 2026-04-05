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

#ifndef FLUX_INTERPRETER_H
#define FLUX_INTERPRETER_H

#include <QObject>
#include <QString>
#include <memory>

#ifdef slots
#undef slots
#endif
#include <pybind11/pybind11.h>
#define slots Q_SLOTS

namespace Flux {

/**
 * @brief Standalone FluxScript Interpreter.
 * Embeds Python for high-performance scripting.
 */
class Interpreter : public QObject {
    Q_OBJECT
public:
    static Interpreter& instance();

    /**
     * @brief Initializes the interpreter and pre-loads math modules.
     */
    void initialize();

    /**
     * @brief Stops the interpreter and cleans up resources.
     */
    void finalize();

    /**
     * @brief Returns true if the interpreter is initialized.
     */
    bool isInitialized() const;

    /**
     * @brief Executes a string of FluxScript (Python) code.
     */
    bool executeString(const QString& code, QString* error = nullptr);

    /**
     * @brief Safely validates a script for security and syntax errors.
     */
    bool validateScript(const QString& code, QString* error = nullptr);

    /**
     * @brief Calls a method on a Python object.
     */
    pybind11::object safeCall(pybind11::object object, const char* method, pybind11::tuple args, QString* error = nullptr);

private:
    explicit Interpreter(QObject* parent = nullptr);
    ~Interpreter();

    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_initialized;
};

} // namespace Flux

#endif // FLUX_INTERPRETER_H
