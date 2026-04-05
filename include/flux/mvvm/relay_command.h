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

#ifndef FLUX_MVVM_RELAY_COMMAND_H
#define FLUX_MVVM_RELAY_COMMAND_H

#include <QObject>
#include <functional>
#include <memory>

namespace Flux::MVVM {

/**
 * @brief Command interface for MVVM pattern
 * 
 * Inspired by ICommand in WPF/UWP, adapted for Qt/C++
 */
class ICommand {
public:
    virtual ~ICommand() = default;
    
    virtual bool canExecute() const = 0;
    virtual void execute() = 0;
    virtual void executeWithParam(void* param) = 0;
    
    using CanExecuteChangedHandler = std::function<void(bool)>;
    virtual void onCanExecuteChanged(CanExecuteChangedHandler handler) = 0;
};

/**
 * @brief Simple command without parameters
 * 
 * Usage:
 *   auto cmd = RelayCommand::create([this]() {
 *       // Action here
 *   }, [this]() {
 *       return canExecuteCondition();
 *   });
 */
class RelayCommand : public ICommand {
public:
    using ExecuteFunc = std::function<void()>;
    using CanExecuteFunc = std::function<bool()>;
    
    RelayCommand(ExecuteFunc execute, CanExecuteFunc canExecute = nullptr)
        : m_execute(execute), m_canExecute(canExecute) {}
    
    static std::shared_ptr<RelayCommand> create(ExecuteFunc execute, 
                                                 CanExecuteFunc canExecute = nullptr) {
        return std::make_shared<RelayCommand>(execute, canExecute);
    }
    
    static std::shared_ptr<RelayCommand> createSimple(ExecuteFunc execute) {
        return std::make_shared<RelayCommand>(execute, []() { return true; });
    }
    
    bool canExecute() const override {
        return m_canExecute ? m_canExecute() : true;
    }
    
    void execute() override {
        if (canExecute() && m_execute) {
            m_execute();
        }
    }
    
    void executeWithParam(void* /*param*/) override {
        execute();  // Ignore param for simple command
    }
    
    void onCanExecuteChanged(CanExecuteChangedHandler handler) override {
        m_canExecuteHandlers.append(handler);
    }
    
    // Notify that canExecute state may have changed
    void notifyCanExecuteChanged() {
        bool canExec = canExecute();
        for (const auto& handler : m_canExecuteHandlers) {
            if (handler) {
                handler(canExec);
            }
        }
    }
    
    // Set new canExecute function
    void setCanExecuteFunc(CanExecuteFunc func) {
        m_canExecute = func;
        notifyCanExecuteChanged();
    }

private:
    ExecuteFunc m_execute;
    CanExecuteFunc m_canExecute;
    QList<CanExecuteChangedHandler> m_canExecuteHandlers;
};

/**
 * @brief Command with parameter support
 * 
 * Usage:
 *   auto cmd = RelayCommandT<int>::create([this](int value) {
 *       // Action with parameter
 *   }, [this](int value) {
 *       return value > 0;  // Can execute condition
 *   });
 * 
 * @tparam T Parameter type
 */
template<typename T>
class RelayCommandT : public ICommand {
public:
    using ExecuteFunc = std::function<void(const T&)>;
    using CanExecuteFunc = std::function<bool(const T&)>;
    
    RelayCommandT(ExecuteFunc execute, CanExecuteFunc canExecute = nullptr)
        : m_execute(execute), m_canExecute(canExecute) {}
    
    static std::shared_ptr<RelayCommandT<T>> create(ExecuteFunc execute,
                                                     CanExecuteFunc canExecute = nullptr) {
        return std::make_shared<RelayCommandT<T>>(execute, canExecute);
    }
    
    static std::shared_ptr<RelayCommandT<T>> createSimple(ExecuteFunc execute) {
        return std::make_shared<RelayCommandT<T>>(execute, [](const T&) { return true; });
    }
    
    bool canExecute() const override {
        return true;  // Requires parameter for actual check
    }
    
    bool canExecuteWithParam(const T& param) const {
        return m_canExecute ? m_canExecute(param) : true;
    }
    
    void execute() override {
        // No-op without parameter
    }
    
    void executeWithParam(void* param) override {
        if (param && m_execute) {
            execute(*static_cast<T*>(param));
        }
    }
    
    void execute(const T& param) {
        if (canExecuteWithParam(param) && m_execute) {
            m_execute(param);
        }
    }
    
    void onCanExecuteChanged(CanExecuteChangedHandler handler) override {
        m_canExecuteHandlers.append(handler);
    }
    
    void notifyCanExecuteChanged() {
        for (const auto& handler : m_canExecuteHandlers) {
            if (handler) {
                handler(true);  // Generic notification
            }
        }
    }

private:
    ExecuteFunc m_execute;
    CanExecuteFunc m_canExecute;
    QList<CanExecuteChangedHandler> m_canExecuteHandlers;
};

/**
 * @brief Async command for long-running operations
 * 
 * Usage:
 *   auto cmd = AsyncCommand::createAsync([this]() -> QFuture<void> {
 *       return QtConcurrent::run([this]() {
 *           // Long-running operation
 *       });
 *   });
 */
class AsyncCommand : public ICommand {
public:
    using AsyncExecuteFunc = std::function<QFuture<void>()>;
    using CanExecuteFunc = std::function<bool()>;
    using CompletedFunc = std::function<void(bool)>;  // success
    
    AsyncCommand(AsyncExecuteFunc execute, 
                 CanExecuteFunc canExecute = nullptr,
                 CompletedFunc completed = nullptr)
        : m_execute(execute), m_canExecute(canExecute), m_completed(completed),
          m_isRunning(false) {}
    
    static std::shared_ptr<AsyncCommand> createAsync(
        AsyncExecuteFunc execute,
        CanExecuteFunc canExecute = nullptr,
        CompletedFunc completed = nullptr) {
        return std::make_shared<AsyncCommand>(execute, canExecute, completed);
    }
    
    bool canExecute() const override {
        return !m_isRunning && (m_canExecute ? m_canExecute() : true);
    }
    
    void execute() override {
        if (!canExecute() || !m_execute)
            return;
        
        m_isRunning = true;
        notifyCanExecuteChanged();
        
        // Execute async operation
        QFuture<void> future = m_execute();
        
        // Watch for completion (requires QtConcurrent)
        // This is a simplified version - full implementation would use QFutureWatcher
        Q_UNUSED(future)
    }
    
    void executeWithParam(void* /*param*/) override {
        execute();
    }
    
    void onCanExecuteChanged(CanExecuteChangedHandler handler) override {
        m_canExecuteHandlers.append(handler);
    }
    
    bool isRunning() const { return m_isRunning; }
    
    void setCompleted(bool success) {
        m_isRunning = false;
        notifyCanExecuteChanged();
        if (m_completed) {
            m_completed(success);
        }
    }
    
    void notifyCanExecuteChanged() {
        bool canExec = canExecute();
        for (const auto& handler : m_canExecuteHandlers) {
            if (handler) {
                handler(canExec);
            }
        }
    }

private:
    AsyncExecuteFunc m_execute;
    CanExecuteFunc m_canExecute;
    CompletedFunc m_completed;
    bool m_isRunning;
    QList<CanExecuteChangedHandler> m_canExecuteHandlers;
};

/**
 * @brief Command collection for grouping related commands
 */
class CommandCollection {
public:
    void addCommand(const QString& name, std::shared_ptr<ICommand> command) {
        m_commands[name] = command;
    }
    
    std::shared_ptr<ICommand> command(const QString& name) const {
        return m_commands.value(name);
    }
    
    bool contains(const QString& name) const {
        return m_commands.contains(name);
    }
    
    QStringList commandNames() const {
        return m_commands.keys();
    }

private:
    QMap<QString, std::shared_ptr<ICommand>> m_commands;
};

} // namespace Flux::MVVM

// Qt metatype registration for QML integration
Q_DECLARE_METATYPE(std::shared_ptr<Flux::MVVM::ICommand>)

#endif // FLUX_MVVM_RELAY_COMMAND_H
