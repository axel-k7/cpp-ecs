#pragma once

#include "ECS.h"
#include "components/DummyComponent.h"
#include <iostream>

class DummySystem : public System {
public:
    void update(const float& _delta_time) override {
        const auto& archetypes = registry->query<DummyComponent>();

        for (const auto* archetype : archetypes) {


            for (size_t i = 0; i < archetype->entities.size(); ++i) {
                Entity entity = archetype->entities[i];

                if (registry->hasComponent<DummyComponent>(entity)) {
                    std::cout << "dummy component found on entity" << entity.id << "\n"; 
                }
            }
        }
    }
};