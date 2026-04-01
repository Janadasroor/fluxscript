#ifndef FLUX_MVVM_OBSERVABLE_PROPERTY_H
#define FLUX_MVVM_OBSERVABLE_PROPERTY_H

#include <QObject>
#include <functional>
#include <memory>
#include <vector>

namespace Flux::MVVM {

/**
 * @brief Forward declaration of change listener
 */
class IPropertyChangeListener;

/**
 * @brief Base interface for observable properties
 */
class IObservableProperty {
public:
    virtual ~IObservableProperty() = default;
    virtual const char* propertyName() const = 0;
    virtual void addListener(IPropertyChangeListener* listener) = 0;
    virtual void removeListener(IPropertyChangeListener* listener) = 0;
};

/**
 * @brief Interface for property change listeners
 */
class IPropertyChangeListener {
public:
    virtual ~IPropertyChangeListener() = default;
    virtual void onPropertyChanged(IObservableProperty* property) = 0;
};

/**
 * @brief Observable property wrapper with change notification
 * 
 * Usage:
 *   ObservableProperty<QString> name{"Name", "Default"};
 *   name.setValue("New Value");  // Automatically notifies listeners
 * 
 * @tparam T Type of the property value
 */
template<typename T>
class ObservableProperty : public IObservableProperty {
public:
    using ChangeCallback = std::function<void(const T& oldValue, const T& newValue)>;
    
    ObservableProperty() = default;
    
    ObservableProperty(const char* name, const T& initialValue = T{})
        : m_name(name), m_value(initialValue) {}
    
    ObservableProperty(const char* name, std::function<T()> initializer)
        : m_name(name), m_value(initializer()) {}
    
    // Copy constructor
    ObservableProperty(const ObservableProperty& other)
        : m_name(other.m_name), m_value(other.m_value) {}
    
    // Move constructor
    ObservableProperty(ObservableProperty&& other) noexcept
        : m_name(other.m_name), m_value(std::move(other.m_value)) {}
    
    // Assignment operators
    ObservableProperty& operator=(const T& value) {
        setValue(value);
        return *this;
    }
    
    ObservableProperty& operator=(const ObservableProperty& other) {
        if (this != &other) {
            setValue(other.m_value);
        }
        return *this;
    }
    
    // Get value
    const T& value() const { return m_value; }
    operator const T&() const { return m_value; }
    
    // Set value with notification
    void setValue(const T& newValue) {
        if (m_value != newValue) {
            T oldValue = m_value;
            m_value = newValue;
            notifyListeners(oldValue, m_value);
        }
    }
    
    // Set value without notification (for batch updates)
    void setValueSilent(const T& value) {
        m_value = value;
    }
    
    // Property name
    const char* propertyName() const override { return m_name; }
    
    // Listener management
    void addListener(IPropertyChangeListener* listener) override {
        if (listener && !m_listeners.contains(listener)) {
            m_listeners.append(listener);
        }
    }
    
    void removeListener(IPropertyChangeListener* listener) override {
        m_listeners.removeAll(listener);
    }
    
    // Bind to another observable property
    template<typename U>
    void bindTo(ObservableProperty<U>& other, std::function<T(const U&)> converter) {
        auto* listener = new PropertyBindingListener<U, T>(other, converter, this);
        m_bindings.append(std::unique_ptr<PropertyBindingBase>(listener));
    }
    
    // Signal-style connection for Qt integration
    using ChangedSignal = std::function<void(const T&)>;
    void onChanged(ChangedSignal callback) {
        m_callbacks.append(callback);
    }
    
    // Force notification
    void notifyChanged() {
        notifyListeners(m_value, m_value);
    }

private:
    void notifyListeners(const T& oldValue, const T& newValue) {
        for (IPropertyChangeListener* listener : m_listeners) {
            if (listener) {
                listener->onPropertyChanged(this);
            }
        }
        
        for (const auto& callback : m_callbacks) {
            if (callback) {
                callback(newValue);
            }
        }
    }
    
    // Base class for type-erased bindings
    class PropertyBindingBase {
    public:
        virtual ~PropertyBindingBase() = default;
    };
    
    // Typed binding listener
    template<typename U, typename V>
    class PropertyBindingListener : public IPropertyChangeListener, public PropertyBindingBase {
    public:
        PropertyBindingListener(ObservableProperty<U>& source,
                               std::function<V(const U&)> converter,
                               ObservableProperty<V>* target)
            : m_source(source), m_converter(converter), m_target(target) {
            source.addListener(this);
        }
        
        ~PropertyBindingListener() override {
            m_source.removeListener(this);
        }
        
        void onPropertyChanged(IObservableProperty* property) override {
            Q_UNUSED(property)
            if (m_target && m_converter) {
                m_target->setValue(m_converter(m_source.value()));
            }
        }
        
    private:
        ObservableProperty<U>& m_source;
        std::function<V(const U&)> m_converter;
        ObservableProperty<V>* m_target;
    };
    
    const char* m_name = "Unknown";
    T m_value;
    QList<IPropertyChangeListener*> m_listeners;
    QList<ChangedSignal> m_callbacks;
    QList<std::unique_ptr<PropertyBindingBase>> m_bindings;
};

/**
 * @brief Specialization for boolean properties with toggle support
 */
template<>
class ObservableProperty<bool> : public IObservableProperty {
public:
    ObservableProperty() = default;
    
    ObservableProperty(const char* name, bool initialValue = false)
        : m_name(name), m_value(initialValue) {}
    
    ObservableProperty& operator=(bool value) {
        setValue(value);
        return *this;
    }
    
    const bool& value() const { return m_value; }
    operator bool() const { return m_value; }
    
    void setValue(bool newValue) {
        if (m_value != newValue) {
            bool oldValue = m_value;
            m_value = newValue;
            notifyListeners(oldValue, m_value);
        }
    }
    
    void toggle() {
        setValue(!m_value);
    }
    
    const char* propertyName() const override { return m_name; }
    
    void addListener(IPropertyChangeListener* listener) override {
        if (listener && !m_listeners.contains(listener)) {
            m_listeners.append(listener);
        }
    }
    
    void removeListener(IPropertyChangeListener* listener) override {
        m_listeners.removeAll(listener);
    }
    
    using ChangedSignal = std::function<void(bool)>;
    void onChanged(ChangedSignal callback) {
        m_callbacks.append(callback);
    }
    
    void notifyChanged() {
        notifyListeners(m_value, m_value);
    }

private:
    void notifyListeners(bool oldValue, bool newValue) {
        for (IPropertyChangeListener* listener : m_listeners) {
            if (listener) {
                listener->onPropertyChanged(this);
            }
        }
        for (const auto& callback : m_callbacks) {
            if (callback) {
                callback(newValue);
            }
        }
    }
    
    const char* m_name = "Unknown";
    bool m_value = false;
    QList<IPropertyChangeListener*> m_listeners;
    QList<ChangedSignal> m_callbacks;
};

/**
 * @brief Read-only observable property (for computed values)
 */
template<typename T>
class ReadOnlyObservableProperty {
public:
    using GetterFunc = std::function<T()>;
    
    ReadOnlyObservableProperty(const char* name, GetterFunc getter)
        : m_name(name), m_getter(getter) {}
    
    const T value() const {
        return m_getter ? m_getter() : T{};
    }
    
    operator T() const { return value(); }
    
    const char* name() const { return m_name; }

private:
    const char* m_name;
    GetterFunc m_getter;
};

} // namespace Flux::MVVM

#endif // FLUX_MVVM_OBSERVABLE_PROPERTY_H
