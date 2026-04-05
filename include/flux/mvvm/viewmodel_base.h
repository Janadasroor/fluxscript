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

#ifndef FLUX_MVVM_VIEWMODEL_BASE_H
#define FLUX_MVVM_VIEWMODEL_BASE_H

#include <QObject>
#include <QString>
#include <QMap>
#include <memory>
#include <functional>
#include "flux/mvvm/observable_property.h"
#include "flux/mvvm/relay_command.h"

namespace Flux::MVVM {

/**
 * @brief Base class for all ViewModels
 * 
 * Provides:
 * - Property change notification (INotifyPropertyChanged pattern)
 * - Command storage and access
 * - Lifecycle management
 * - Validation support
 */
class ViewModelBase : public QObject, public IPropertyChangeListener {
    Q_OBJECT
    
public:
    explicit ViewModelBase(QObject* parent = nullptr);
    ~ViewModelBase() override;

    // ========================================================================
    // Property Change Notification
    // ========================================================================
    
    /**
     * @brief Notify that a property has changed
     * @param propertyName Name of the property
     */
    void notifyPropertyChanged(const QString& propertyName);
    
    /**
     * @brief Notify that all properties have changed
     */
    void notifyAllPropertiesChanged();
    
    /**
     * @brief Register an observable property for automatic notification
     * @param propertyName Name of the property
     * @param property The observable property
     */
    template<typename T>
    void registerProperty(const QString& propertyName, ObservableProperty<T>& property) {
        property.addListener(this);
        m_propertyNames[&property] = propertyName;
    }
    
    // ========================================================================
    // Command Management
    // ========================================================================
    
    /**
     * @brief Register a command with the ViewModel
     * @param name Command name
     * @param command Command instance
     */
    void registerCommand(const QString& name, std::shared_ptr<ICommand> command);
    
    /**
     * @brief Get a registered command
     * @param name Command name
     * @return Command instance, or nullptr if not found
     */
    std::shared_ptr<ICommand> getCommand(const QString& name) const;
    
    /**
     * @brief Check if a command is registered
     * @param name Command name
     * @return true if command exists
     */
    bool hasCommand(const QString& name) const;
    
    /**
     * @brief Get all registered command names
     * @return List of command names
     */
    QStringList commandNames() const;

    // ========================================================================
    // Validation
    // ========================================================================
    
    /**
     * @brief Check if the ViewModel has validation errors
     * @return true if has errors
     */
    virtual bool hasErrors() const;
    
    /**
     * @brief Get validation errors for a property
     * @param propertyName Property name
     * @return List of error messages
     */
    virtual QStringList getErrors(const QString& propertyName) const;
    
    /**
     * @brief Get all validation errors
     * @return Map of property name to error list
     */
    virtual QMap<QString, QStringList> allErrors() const;
    
    /**
     * @brief Validate the ViewModel
     * @return true if valid
     */
    virtual bool validate();
    
    /**
     * @brief Clear all validation errors
     */
    void clearErrors();
    
    /**
     * @brief Add a validation error
     * @param propertyName Property name
     * @param error Error message
     */
    void addError(const QString& propertyName, const QString& error);
    
    /**
     * @brief Remove validation errors for a property
     * @param propertyName Property name
     */
    void removeErrors(const QString& propertyName);

    // ========================================================================
    // Lifecycle
    // ========================================================================
    
    /**
     * @brief Called when the ViewModel is loaded
     */
    virtual void onLoad() {}
    
    /**
     * @brief Called when the ViewModel is about to be unloaded
     */
    virtual void onUnload() {}
    
    /**
     * @brief Check if the ViewModel is loaded
     * @return true if loaded
     */
    bool isLoaded() const { return m_isLoaded; }
    
    /**
     * @brief Set the loaded state
     * @param loaded Loaded state
     */
    void setLoaded(bool loaded);

    // ========================================================================
    // IPropertyChangeListener Implementation
    // ========================================================================
    
    void onPropertyChanged(IObservableProperty* property) override;

signals:
    // ========================================================================
    // Qt Signals
    // ========================================================================
    
    /**
     * @brief Emitted when a property changes
     * @param propertyName Name of the property
     */
    void propertyChanged(const QString& propertyName);
    
    /**
     * @brief Emitted when validation errors change
     */
    void errorsChanged();
    
    /**
     * @brief Emitted when the ViewModel is loaded
     */
    void loaded();
    
    /**
     * @brief Emitted when the ViewModel is unloaded
     */
    void unloaded();

protected:
    // ========================================================================
    // Protected Helpers
    // ========================================================================
    
    /**
     * @brief Set a property value and notify if changed
     * @param propertyName Property name
     * @param property Reference to property
     * @param newValue New value
     * @return true if value was changed
     */
    template<typename T>
    bool setProperty(const QString& propertyName, ObservableProperty<T>& property, const T& newValue) {
        if (property.value() != newValue) {
            property.setValue(newValue);
            return true;
        }
        return false;
    }
    
    /**
     * @brief Get a property value
     * @param property Property reference
     * @return Property value
     */
    template<typename T>
    const T& get(const ObservableProperty<T>& property) const {
        return property.value();
    }

private:
    // ========================================================================
    // Private Members
    // ========================================================================
    
    bool m_isLoaded = false;
    QMap<QString, QStringList> m_validationErrors;
    QMap<IObservableProperty*, QString> m_propertyNames;
    QMap<QString, std::shared_ptr<ICommand>> m_commands;
};

/**
 * @brief Generic ViewModel base with model type
 * 
 * @tparam ModelType Type of the underlying model
 */
template<typename ModelType>
class ViewModelT : public ViewModelBase {
public:
    explicit ViewModelT(ModelType* model, QObject* parent = nullptr)
        : ViewModelBase(parent), m_model(model) {}
    
    explicit ViewModelT(std::unique_ptr<ModelType> model, QObject* parent = nullptr)
        : ViewModelBase(parent), m_model(std::move(model)) {}
    
    ModelType* model() const { return m_model.get(); }
    ModelType* modelRaw() const { return m_model.get(); }
    
    void setModel(std::unique_ptr<ModelType> model) {
        m_model = std::move(model);
    }

protected:
    std::unique_ptr<ModelType> m_model;
};

/**
 * @brief View-Only ViewModel (no underlying model)
 */
class ViewOnlyViewModel : public ViewModelBase {
public:
    explicit ViewOnlyViewModel(QObject* parent = nullptr)
        : ViewModelBase(parent) {}
};

} // namespace Flux::MVVM

#endif // FLUX_MVVM_VIEWMODEL_BASE_H
