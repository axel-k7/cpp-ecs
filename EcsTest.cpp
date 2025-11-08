#pragma once

#include "ECS.h"
#include "systems/DummySystem.h"
#include "components/DummyComponent.h"

#include <iostream>

int main() {
    Registry registry;
    SystemManager sys_manager;

    std::cout << "ecs test :) \n \n";

    sys_manager.registerSystem<DummySystem>(&registry);
    
    Entity e1 = registry.createEntity(DummyComponent{});
    Entity e2 = registry.createEntity(DummyComponent{});

    Entity e3 = registry.createEntity();

    sys_manager.update(0.f);
    
    return 0;
}