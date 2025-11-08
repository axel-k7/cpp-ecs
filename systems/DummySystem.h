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
        _registry->onEntityCreated.subscribe(
         [this](Entity _entity) {
            this->creationCallback(_entity);
         });

        _registry->onEntityDestroyed.subscribe(
        [this](Entity _entity) {
            this->destructionCallback(_entity);
        });

        _registry->onComponentAdded[registry->getComponentTypeID<DummyComponent>()].subscribe
        ([this](Entity _entity, uint32_t _type) {
            this->additionCallback(_entity, _type);
        });

        _registry->onComponentRemoved[registry->getComponentTypeID<DummyComponent>()].subscribe
        ([this](Entity _entity, uint32_t _type) {
            this->removalCallback(_entity, _type);
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

    void additionCallback(Entity _entity, uint32_t _component_type) override {
        if (_component_type == registry->getComponentTypeID<DummyComponent>()) {
            std::cout << "component: " << _component_type << " added to entity: " << _entity.id << "\n";
        }
    };

    void removalCallback(Entity _entity, uint32_t _component_type) override {
        if (_component_type == registry->getComponentTypeID<DummyComponent>()) {
            std::cout << "component: " << _component_type << " removed from entity: " << _entity.id << "\n";
        }
    };
};