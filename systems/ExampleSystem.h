#pragma once

#include "../core/System.h"
#include "../components/ExampleComponent.h"

#include <iostream>

class ExampleSystem : public System {
public:
    ExampleSystem(std::shared_ptr<Registry> _registry, std::shared_ptr<EntityCommandBuffer> _buffer)
        : System(_registry, _buffer)
        , query(_registry->query<ExampleComponent>())
    { }

    Registry::QueryResult<ExampleComponent> query;

    void update(const float& /*_dt*/) override {
        for (auto [entity, example_data] : query) {
            example_data.count++;
        }
    }
};