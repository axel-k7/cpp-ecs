#pragma once

#include "RegistryNEW.h"
#include "EntityCommandBufferNEW.h"

#include <iostream>

class System {
public:
    System(std::shared_ptr<Registry> _registry, std::shared_ptr<EntityCommandBuffer> _buffer)
        : registry(_registry)
        , buffer(_buffer)
    {}

    virtual ~System() = default;
    virtual void update(const float& _delta_time) = 0;

    std::shared_ptr<Registry> registry;
    std::shared_ptr<EntityCommandBuffer> buffer;
};

struct ExampleComponent1 {
    int component_data = 0;
};

struct ExampleComponent2 {
    std::string other_data = "hello";
};

class ExampleSystem : public System {
public:
    ExampleSystem(std::shared_ptr<Registry> _registry, std::shared_ptr<EntityCommandBuffer> _buffer)
        : System(_registry, _buffer)
        , query(_registry->query<ExampleComponent1, ExampleComponent2>())
    {}

    Registry::QueryResult<ExampleComponent1, ExampleComponent2> query;

    void update(const float& /*_dt*/) override {
        for (auto [entity, component_1, component_2] : query) {
            component_1.component_data++;
            std::cout << component_2.other_data;
        }
    }

};