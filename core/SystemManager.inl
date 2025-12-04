#include "SystemManager.h"

template<typename T>
auto SystemManager::getSystemTypeID() -> uint32_t {
    static const uint32_t type_id = static_cast<uint32_t>(next_system_id++);
    return type_id;
}

template <typename T>
void SystemManager::registerSystem(Registry* _registry, EntityCommandBuffer* _buffer) {
    //should probably check if T is system 
    const uint32_t type = getSystemTypeID<T>();

    if (systems.find(type) != systems.end()) {
        return;
    }

    std::unique_ptr<T> new_system = std::make_unique<T>();
    new_system->setRegistry(_registry);
    new_system->setCommandBuffer(_buffer);
        
    new_system->setupListeners(_registry);

    //save pointer before moving it
    System* system_pointer = new_system.get();

    systems[type] = std::move(new_system);
    system_order.push_back(system_pointer);
};


template<typename T>
auto SystemManager::getSystem() -> T* {
    const uint32_t type = getSystemTypeID<T>();
    auto it = systems.find(type);
    if (it == systems.end())
        return nullptr;

    return static_cast<T*>(it->second.get());
}