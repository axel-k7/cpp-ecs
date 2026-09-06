#pragma once

#include "Registry.h"

#include <functional>

class EntityCommandBuffer {
public:
    struct EntityChange {
        Signature add;
        Signature remove;
        bool destroy = false;

        struct ComponentConstructor {
            uint32_t type;
            std::function<void(Entity)> construct;
        };

        std::vector<ComponentConstructor> constructors;
    };

    EntityCommandBuffer(std::shared_ptr<Registry> _registry)
        : registry(_registry)
    { }


    template<typename... Components>
    auto createEntity(Components&&... _components) -> Entity {
        Entity entity = registry->allocateEntity();

        (addComponent<Components>(entity, std::forward<Components>(_components)), ...);

        return entity;
    }

    
    void destroyEntity(Entity _entity) {
        changes[_entity.id].destroy = true;
    }


    //should return a temporary component which can be modified before its created in the chunk
    template<typename T, typename... Args>
    void addComponent(Entity _entity, Args&&... _args) {
        EntityChange& change = changes[_entity.id];
        uint32_t type = Registry::getComponentTypeID<T>();

        change.add.set(type);

        change.constructors.emplace_back(EntityChange::ComponentConstructor{
            type,
            [type, this, data = T(std::forward<Args>(_args)...)](Entity _entity) mutable {
                auto& record = registry->records[_entity.id];
                const size_t local_index = record.archetype->getLocalIndex(type);

                //think adding 2 components of same type in 1 frame will make this explode
                //difficult to safety check void*
                record.chunk->createAt(record.index, local_index, std::move(data));
            }
        });
    }

    template<typename T>
    void removeComponent(Entity _entity) {
        EntityChange& change = changes[_entity.id];
        uint32_t type = Registry::getComponentTypeID<T>();

        change.remove.set(type);
    }


    void update() {
        for (auto& [id, change] : changes) {
            Entity entity{ id, registry->versions[id] };

            if (!registry->entityExists(entity))
                continue;

            if (change.destroy) {
                registry->destroyEntity(entity);
                continue;
            }

            auto& prev_record = registry->records[id];

            Signature prev_signature = prev_record.archetype ? prev_record.archetype->signature : Signature{};
            Signature target_signature = prev_signature;

            target_signature |= change.add;
            target_signature &= ~change.remove;

            if (target_signature != prev_signature)
                registry->moveEntity(entity, target_signature);

            for (auto& construction : change.constructors) {
                if (target_signature.test(construction.type))
                    construction.construct(entity);
            }
        }

        changes.clear();
    }

private:
    std::shared_ptr<Registry> registry;

    std::unordered_map<uint32_t, EntityChange> changes;
};

