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

                if (registry->hasComponent<DummyComponent>(entity)) 
                    std::cout << "dummy component found on entity: " << entity.id << "\n"; 
            }
        }
    }

    void setupListeners(Registry* _registry) override {
        _registry->onEntityCreated.subscribe([this](Entity _entity) {
            this->creationCallback(_entity);
        });

        _registry->onEntityDestroyed.subscribe([this](Entity _entity) {
            this->destructionCallback(_entity);
        });
    }

    void creationCallback(Entity _entity) override {
        if (registry->hasComponent<DummyComponent>(_entity)) 
            std::cout << "entity: " << _entity.id << " added!!\n";
    }

    void destructionCallback(Entity _entity) override {
        if (registry->hasComponent<DummyComponent>(_entity))
            std::cout << "entity: " << _entity.id << " destroyed!!\n";
    }
};