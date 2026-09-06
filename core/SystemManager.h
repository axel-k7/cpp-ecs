#pragma once

#include <unordered_map>
#include <memory>
#include <typeindex>

#include "System.h"
#include "Registry.h"

//needs major rewrite, maybe look at querires to account for multithreading?

class SystemManager {
public:
    SystemManager(std::shared_ptr<Registry> _registry);

    void update(const float& _delta_time);

    template<typename T> void registerSystem();
    template<typename T> static auto getSystemTypeID() -> uint32_t;
    template<typename T> auto getSystem() -> T*;
private:
    static inline uint32_t next_system_id;

    std::unordered_map<uint32_t, std::unique_ptr<System>> systems;
    std::vector<System*> system_order;
    EntityCommandBuffer* buffer;
};

#include "SystemManager.inl"