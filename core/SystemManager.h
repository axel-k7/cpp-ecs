#pragma once

#include <unordered_map>
#include <memory>
#include <typeindex>

#include "System.h"
#include "Registry.h"

class SystemManager {
public:
    template<typename T> void registerSystem(Registry* _registry);
    template<typename T> auto getSystem() -> T*;

    void update(const float& _delta_time);

private:
    std::unordered_map<std::type_index, std::unique_ptr<System>> systems;

};

#include "SystemManager.inl"