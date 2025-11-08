#include "SystemManager.h"

template <typename T>
void SystemManager::registerSystem(Registry* _registry) {
    if (systems.find(typeid(T)) != systems.end()) {
        return;
    }

    std::unique_ptr<T> new_system = std::make_unique<T>();
    new_system->setRegistry(_registry);

    new_system->setupListeners(_registry);

    systems[typeid(T)] = std::move(new_system);
};


template<typename T>
auto SystemManager::getSystem() -> T* {
    auto it = systems.find(typeid(T));
    if (it == systems.end())
        return nullptr;

    return static_cast<T*>(it->second.get());
}

void SystemManager::update(const float& _delta_time) {
    for (auto const& [_, system] : systems) {
        system->update(_delta_time);
    }
}