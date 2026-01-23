#pragma once

#include "RegistryNEW.h"

#include <iostream>

class System {
public:
    System(std::shared_ptr<Registry> _registry)
        : registry(_registry)
    {}

    virtual ~System() = default;
    virtual void update(const float& _delta_time) = 0;

    std::shared_ptr<Registry> registry;
};

struct ExampleComponent1 {
    int component_data = 0;
};

struct ExampleComponent2 {
    std::string other_data = "hello";
};

class ExampleSystem : public System {
public:
    ExampleSystem(std::shared_ptr<Registry> _registry)
        : System(_registry)
        , query(_registry->query<ExampleComponent1, ExampleComponent2>())
    {}

    Registry::QueryResult<ExampleComponent1, ExampleComponent2> query;

    void update(const float& _dt) override {
        for (auto [component_1, component_2] : query) {
            component_1.component_data++;
            std::cout << component_2.other_data;
        }
    }

};