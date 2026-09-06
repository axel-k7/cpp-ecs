#pragma once

#include "core/EntityCommandBuffer.h"
#include "systems/ExampleSystem.h"


int main() {
    auto registry = std::make_shared<Registry>();
    auto buffer = std::make_shared<EntityCommandBuffer>(registry);
    auto example_system = std::make_unique<ExampleSystem>(registry, buffer);

    constexpr size_t entity_count = 100;

    std::vector<Entity> entities;
    entities.reserve(entity_count);


    for (size_t i = 0; i < entity_count; ++i) {
        Entity entity = registry->allocateEntity();

        entities.push_back(entity);

        registry->addComponents<ExampleComponent>(entity, {});
    }

    std::cout << "created entities\n";


    for (Entity entity : entities) 
        assert(registry->tryGetComponent<ExampleComponent>(entity));

    std::cout << "all entities have the correct components\n";


    constexpr int updates = 10;
    for (int frame = 0; frame < updates; ++frame)
        example_system->update(1.0f / 60.0f); //delta substitute, should update a system manager instead

    std::cout << "cleared initial updates\n";


    Entity last_entity = entities.back();
    registry->destroyEntity(last_entity);

    assert(!registry->entityExists(last_entity));

    entities.pop_back();

    std::cout << "destroyed entity\n";


    for (int frame = 0; frame < updates; ++frame) {
        example_system->update(1.0f / 60.0f);

        buffer->destroyEntity(entities.back());
        entities.pop_back();
        
    
        entities.push_back(buffer->createEntity());
    }

    std::cout << "ran buffer commands\n";

    return 0;
}
