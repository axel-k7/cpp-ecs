#pragma once

#include "ECS.h"
#include "systems/DummySystem.h"
#include "components/DummyComponent.h"

#include <random>
#include <iostream>

int main() {
    Registry registry;
    SystemManager sys_manager;

    sys_manager.registerSystem<DummySystem>(&registry);

    constexpr size_t entity_amount = 100'000;
    std::vector<Entity> entities;
    entities.reserve(entity_amount);

    for (size_t i = 0; i < entity_amount; ++i)
        entities.push_back(registry.createEntity(DummyComponent{}));

    std::mt19937 rng(12345);
    std::uniform_int_distribution<size_t> dist(0, entity_amount - 1);
    for (size_t i = 0; i < entity_amount; ++i) {
        size_t idx = dist(rng);
        Entity e = entities[idx];
        if (registry.hasComponent<DummyComponent>(e))
            registry.removeComponent<DummyComponent>(e);
        else
            registry.addComponent(e, DummyComponent{});
    }

    for (size_t i = 0; i < 50'000; ++i) {
        size_t idx = dist(rng);
        if (registry.entityExists(entities[idx]))
            registry.destroyEntity(entities[idx]);
    }

    std::cout << "it works :D\n";
}