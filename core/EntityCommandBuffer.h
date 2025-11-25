#pragma once

#include "App/Ecs/core/Registry.h"

class EntityCommandBuffer {
public:
    struct Command {
        std::vector<uint32_t> component_removals;
        std::vector<uint32_t> component_additions;
        std::vector<std::function<void(const Entity&)>> apply_commands;

        bool entity_destruction = false;
    };


    struct Migration {
        Signature new_signature;
        std::vector<std::function<void(const Entity&)>> apply_commands;
    };


    template<typename... Components>
    Entity createEntity(Components&&... _components) {
        Entity entity = registry->createEntity();

        (addComponent(entity, std::forward<Components>(_components)), ...);

        return entity;
    }

    
    void destroyEntity(Entity _entity) {
        command_buffer[_entity.id].entity_destruction = true;
    }


    template<typename T, typename... Args>
    void addComponent(Entity _entity, Args&&... _args) {
        uint32_t type = Registry::getComponentTypeID<T>();
        Command& command = command_buffer[_entity.id];

        command.component_additions.push_back(type);

        T component(std::forward<Args>(_args)...);
        command.apply_commands.emplace_back(
            [this, data = std::move(component)](const Entity& _entity) mutable {
                registry->addComponent<T>(_entity, std::move(data));
            }
        );
    }

    template<typename T>
    void addComponent(Entity _entity, T&& _component) {
        uint32_t type = Registry::getComponentTypeID<T>();
        Command& command = command_buffer[_entity.id];

        command.component_additions.push_back(type);

        command.apply_commands.emplace_back(
            [this, data = std::forward<T>(_component)](const Entity& _entity) mutable {
                registry->addComponent<T>(_entity, std::move(data));
            }
        );
    }


    template<typename T>
    void removeComponent(Entity _entity) {
        uint32_t type = Registry::getComponentTypeID<T>();
        Command& command = command_buffer[_entity.id];

        command.component_removals.push_back(type);

        command.apply_commands.emplace_back(
            [this](const Entity& _entity) mutable {
                registry->removeComponent<T>(_entity);
            }
        );
    }


    void update() {
        //migrate
        std::unordered_map<uint32_t, Migration> migrations;
        migrations.reserve(command_buffer.size());

        for (auto& [id, command] : command_buffer) {
            if (command.entity_destruction)
                continue; //will destroy last

            if (id >= registry->records.size())
                continue; //check if exists

            Registry::EntityRecord& record = registry->records[id];
            if (!record.archetype)
                continue;

            Migration migrated_entity;
            migrated_entity.new_signature = record.signature;

            for (auto removal : command.component_removals)
                migrated_entity.new_signature.reset(removal);

            for (auto addition : command.component_additions)
                migrated_entity.new_signature.set(addition);

            migrated_entity.apply_commands = command.apply_commands;

            migrations[id] = std::move(migrated_entity);
        }

        //add data to entity
        for (auto& [id, migration] : migrations) {
            Entity entity { id, registry->versions[id] };

            for (auto& command : migration.apply_commands)
                command(entity);

            registry->moveEntity(entity, migration.new_signature);
        }


        //destroy entity
        for (auto& [id, command] : command_buffer) {
            if (command.entity_destruction)
            {
                Entity entity { id, registry->versions[id] };
                registry->destroyEntity(entity);
            }
        }


        command_buffer.clear();
    }


    void setRegistry(Registry* _registry) { registry = _registry; }

private:
    Registry* registry = nullptr;

    std::unordered_map<uint32_t, Command> command_buffer;
};

