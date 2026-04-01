#ifndef FLUX_MVVM_VIEWMODEL_LOCATOR_H
#define FLUX_MVVM_VIEWMODEL_LOCATOR_H

#include <QObject>
#include <QMap>
#include <QString>
#include <memory>
#include <functional>
#include <typeindex>
#include <typeinfo>

namespace Flux::MVVM {

class ViewModelBase;

/**
 * @brief Service locator and dependency injection container for ViewModels
 * 
 * Features:
 * - ViewModel registration and resolution
 * - Singleton and transient lifetime management
 * - Dependency injection for ViewModel constructors
 * - Hierarchical locators (parent-child)
 * 
 * Usage:
 *   // Register
 *   ViewModelLocator::instance()->registerViewModel<EditorViewModel>(
 *       [](auto* locator) {
 *           return std::make_unique<EditorViewModel>(
 *               locator->resolve<DocumentService>().get()
 *           );
 *       }
 *   );
 *   
 *   // Resolve
 *   auto* editor = ViewModelLocator::instance()->resolve<EditorViewModel>();
 */
class ViewModelLocator : public QObject {
    Q_OBJECT
    
public:
    using FactoryFunc = std::function<std::unique_ptr<ViewModelBase>(ViewModelLocator*)>;
    
    enum class Lifetime {
        Singleton,      // Single instance, reused
        Transient,      // New instance each time
        Scoped          // One per scope (e.g., per window)
    };
    
    explicit ViewModelLocator(QObject* parent = nullptr);
    explicit ViewModelLocator(ViewModelLocator* parent);
    ~ViewModelLocator() override;
    
    // ========================================================================
    // Singleton Access
    // ========================================================================
    
    /**
     * @brief Get the global ViewModelLocator instance
     * @return Global locator instance
     */
    static ViewModelLocator* instance();
    
    /**
     * @brief Set the global ViewModelLocator instance
     * @param instance Instance to set
     */
    static void setInstance(ViewModelLocator* instance);

    // ========================================================================
    // Registration
    // ========================================================================
    
    /**
     * @brief Register a ViewModel type with factory function
     * @tparam TViewModel ViewModel type
     * @param factory Factory function to create instances
     * @param lifetime Lifetime management strategy
     */
    template<typename TViewModel>
    void registerViewModel(FactoryFunc factory, Lifetime lifetime = Lifetime::Singleton) {
        Registration registration;
        registration.factory = factory;
        registration.lifetime = lifetime;
        registration.typeId = typeid(TViewModel);
        
        m_registrations[getTypeName<TViewModel>()] = registration;
    }
    
    /**
     * @brief Register a ViewModel type with explicit type
     * @param typeName Type name (for QML integration)
     * @param factory Factory function
     * @param lifetime Lifetime management
     */
    void registerViewModel(const QString& typeName, FactoryFunc factory, 
                          Lifetime lifetime = Lifetime::Singleton);
    
    /**
     * @brief Register an existing instance as a singleton
     * @tparam TViewModel ViewModel type
     * @param instance Instance to register
     */
    template<typename TViewModel>
    void registerInstance(TViewModel* instance) {
        m_instances[getTypeName<TViewModel>()] = std::unique_ptr<ViewModelBase>(instance);
        
        Registration registration;
        registration.lifetime = Lifetime::Singleton;
        registration.typeId = typeid(TViewModel);
        m_registrations[getTypeName<TViewModel>()] = registration;
    }

    // ========================================================================
    // Resolution
    // ========================================================================
    
    /**
     * @brief Resolve a ViewModel instance
     * @tparam TViewModel ViewModel type
     * @return ViewModel instance (nullptr if not registered)
     */
    template<typename TViewModel>
    TViewModel* resolve() {
        return static_cast<TViewModel*>(resolveViewModel(getTypeName<TViewModel>()));
    }
    
    /**
     * @brief Resolve a ViewModel by type name
     * @param typeName Type name
     * @return ViewModel instance
     */
    ViewModelBase* resolveViewModel(const QString& typeName);
    
    /**
     * @brief Try to resolve a ViewModel, returns nullptr if not found
     * @tparam TViewModel ViewModel type
     * @return ViewModel instance or nullptr
     */
    template<typename TViewModel>
    TViewModel* tryResolve() {
        if (m_instances.contains(getTypeName<TViewModel>())) {
            return static_cast<TViewModel*>(m_instances[getTypeName<TViewModel>()].get());
        }
        return nullptr;
    }
    
    /**
     * @brief Check if a ViewModel type is registered
     * @tparam TViewModel ViewModel type
     * @return true if registered
     */
    template<typename TViewModel>
    bool isRegistered() const {
        return m_registrations.contains(getTypeName<TViewModel>());
    }
    
    /**
     * @brief Check if a ViewModel type is registered by name
     * @param typeName Type name
     * @return true if registered
     */
    bool isRegistered(const QString& typeName) const;

    // ========================================================================
    // Child Locators
    // ========================================================================
    
    /**
     * @brief Create a child locator for scoped ViewModels
     * @return Child locator
     */
    ViewModelLocator* createChildLocator();
    
    /**
     * @brief Get the parent locator
     * @return Parent locator or nullptr
     */
    ViewModelLocator* parentLocator() const { return m_parentLocator; }
    
    /**
     * @brief Get all child locators
     * @return List of child locators
     */
    QList<ViewModelLocator*> childLocators() const { return m_childLocators; }

    // ========================================================================
    // Lifetime Management
    // ========================================================================
    
    /**
     * @brief Clear all singleton instances
     */
    void clearInstances();
    
    /**
     * @brief Clear instances for a specific type
     * @param typeName Type name
     */
    void clearInstance(const QString& typeName);
    
    /**
     * @brief Unregister a ViewModel type
     * @param typeName Type name
     */
    void unregisterViewModel(const QString& typeName);

    // ========================================================================
    // Service Registration (for dependencies)
    // ========================================================================
    
    template<typename TService>
    void registerService(std::unique_ptr<TService> service) {
        m_services[typeid(TService).name()] = 
            std::unique_ptr<void, std::function<void(void*)>>(
                service.release(),
                [](void* p) { delete static_cast<TService*>(p); }
            );
    }
    
    template<typename TService>
    TService* resolveService() {
        auto it = m_services.find(typeid(TService).name());
        if (it != m_services.end()) {
            return static_cast<TService*>(it->second.get());
        }
        return nullptr;
    }

signals:
    // ========================================================================
    // Signals
    // ========================================================================
    
    void viewModelResolved(const QString& typeName, ViewModelBase* viewModel);
    void viewModelRegistered(const QString& typeName);
    void childLocatorCreated(ViewModelLocator* child);

private:
    // ========================================================================
    // Private Types
    // ========================================================================
    
    struct Registration {
        FactoryFunc factory;
        Lifetime lifetime;
        std::type_index typeId;
    };
    
    // ========================================================================
    // Private Helpers
    // ========================================================================
    
    template<typename T>
    static QString getTypeName() {
        return QString::fromStdString(typeid(T).name());
    }
    
    ViewModelBase* createInstance(const Registration& registration);
    ViewModelBase* resolveFromParent(const QString& typeName);

    // ========================================================================
    // Members
    // ========================================================================
    
    static ViewModelLocator* s_instance;
    
    ViewModelLocator* m_parentLocator = nullptr;
    QList<ViewModelLocator*> m_childLocators;
    
    QMap<QString, Registration> m_registrations;
    QMap<QString, std::unique_ptr<ViewModelBase>> m_instances;
    QMap<QString, std::unique_ptr<void, std::function<void(void*)>>> m_services;
};

/**
 * @brief RAII scope guard for child locator lifetime
 * 
 * Usage:
 *   {
 *       ScopedLocatorScope scope(ViewModelLocator::instance());
 *       auto* child = scope.locator();
 *       auto* vm = child->resolve<SomeViewModel>();
 *       // Child locator automatically cleaned up
 *   }
 */
class ScopedLocatorScope {
public:
    explicit ScopedLocatorScope(ViewModelLocator* parent)
        : m_parent(parent), m_child(parent->createChildLocator()) {}
    
    ~ScopedLocatorScope() {
        // Child locator will be deleted by parent
    }
    
    ViewModelLocator* locator() const { return m_child; }
    
    ScopedLocatorScope(const ScopedLocatorScope&) = delete;
    ScopedLocatorScope& operator=(const ScopedLocatorScope&) = delete;

private:
    ViewModelLocator* m_parent;
    ViewModelLocator* m_child;
};

} // namespace Flux::MVVM

#endif // FLUX_MVVM_VIEWMODEL_LOCATOR_H
