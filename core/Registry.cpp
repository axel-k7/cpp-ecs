#include "Registry.h"

Registry::~Registry() {
    signature_map.clear();
    archetypes.clear();
}


//---------------------------------------------------------------------------------------------------------------------
//ENTITY


auto Registry::createEntity() -> Entity {
    Entity entity = allocateEntity();
    moveEntity(entity, Signature{});

    onEntityCreated.trigger(entity);

    return entity;
};


auto Registry::createEntity(const Signature& _signature) -> Entity {
    Entity entity = allocateEntity();
    Archetype* archetype = getArchetype(_signature);

    size_t new_index = archetype->entities.size();
    updateEntityRecord(entity, archetype, new_index);

    onEntityCreated.trigger(entity);

    return entity;
}


void Registry::destroyEntity(const Entity& _entity) {
    assert(entityExists(_entity));
    
    onEntityDestroyed.trigger(_entity);
    moveEntity(_entity, Signature{});


    invalidateEntity(_entity);
};


//ENTITY END
//---------------------------------------------------------------------------------------------------------------------
//COMPONENTS


void Registry::addComponent(const Entity& _entity, uint32_t _type, sComponentArray* _component) {
    assert(entityExists(_entity));

    Signature new_signature = records[_entity.id].signature;
    if (new_signature.test(_type))
        return;

    new_signature.set(_type, true);

    Archetype* target = moveEntity(_entity, new_signature);

    sComponentArray* array = target->ensureComponentArray(_type, _component);
    array->addFrom(_component);

    if (onComponentAdded.count(_type))
        onComponentAdded.at(_type).trigger(_entity, _type);
}


void Registry::removeComponent(const Entity& _entity, uint32_t _type) {
    assert(entityExists(_entity));
    
    Signature new_signature = records[_entity.id].signature;
    new_signature.set(_type, false);

    moveEntity(_entity, new_signature);

    if (onComponentRemoved.count(_type))
        onComponentRemoved.at(_type).trigger(_entity, _type);
}


auto Registry::getComponent(const Entity& _entity, uint32_t _type) -> void* {
    assert(entityExists(_entity));

    EntityRecord& record = records[_entity.id];
    auto it = record.archetype->component_arrays.find(_type);
    if (it == record.archetype->component_arrays.end()) {
        return nullptr;
    } 

    return it->second->getRaw(record.index);
} 


auto Registry::hasComponent(const Entity& _entity, uint32_t _type) -> bool {
    assert(entityExists(_entity));

    return records[_entity.id].signature.test(_type);
}


//COMPONENTS END
//---------------------------------------------------------------------------------------------------------------------
//QUERIES


    //query archetype of same bitmask
auto Registry::query(const Signature& _signature) const -> const std::vector<const Archetype*>& {
    auto it = query_cache.find(_signature);
    if (it != query_cache.end())
        return it->second;


    std::vector<const Archetype*> result;

    for (const auto& archetype_ptr : archetypes) {
        Archetype* archetype = archetype_ptr.get();
        if ((archetype->signature & _signature) == _signature)
            result.push_back(archetype);
    }
    
    auto [_it, _] = query_cache.emplace(_signature, std::move(result));
    return _it->second;
}


//QUERIES END
//---------------------------------------------------------------------------------------------------------------------
//ENTITY HELPERS

auto Registry::moveEntity(Entity _entity, const Signature& _new_signature) -> Archetype* {
    EntityRecord& record = records[_entity.id];
    Archetype* curr_archetype = record.archetype;
    Archetype* target_archetype = getArchetype(_new_signature);

    if (curr_archetype != target_archetype && curr_archetype) {
        moveToArchetype(_entity, curr_archetype, record.index, target_archetype);
    }
    else if (!curr_archetype) {
        record.index = target_archetype->entities.size();
        target_archetype->entities.push_back(_entity);
        record.archetype = target_archetype;
    }

    record.signature = _new_signature;

    invalidateQueryCache();

    return target_archetype;
}


void Registry::invalidateEntity(const Entity& _entity) {
    assert(entityExists(_entity));

    EntityRecord& record = records[_entity.id];
    record.archetype = nullptr;
    record.index = 0;
    record.signature.reset();
    versions[_entity.id]++;

    free_ids.push_back(_entity.id);
    invalidateQueryCache();
}


void Registry::removeEntityAt(Archetype* _archetype, size_t _index) noexcept {
    size_t last_index = _archetype->entities.size()-1;

    if (_index != last_index) {
        Entity last_entity = _archetype->entities[last_index];
        _archetype->entities[_index] = last_entity;
        records[last_entity.id].index = _index;
        
        for (auto& [_, array] : _archetype->component_arrays) {
            if (_index != last_index) 
                array->swapElements(_index, last_index);
            
            array->removeLast();
        }
    }

    _archetype->entities.pop_back();
};


void Registry::updateEntityRecord(Entity _entity, Archetype* _new, size_t _new_index) noexcept {
    auto& record = records[_entity.id];
    record.archetype = _new;
    record.index = _new_index;
    record.signature = _new->signature;
};


void Registry::moveToArchetype(Entity _entity, Archetype* _source, size_t _source_index, Archetype* _target) noexcept {
    if (_source == _target || !entityExists(_entity))
        return;
    
    size_t new_index = _target->entities.size();
    _target->entities.push_back(_entity);

    _target->transferComponents(_source, _source_index);

    removeEntityAt(_source, _source_index);

    updateEntityRecord(_entity, _target, new_index);
};


auto Registry::allocateEntity() -> Entity {
    uint32_t id;

    //get id or create new one
    if (!free_ids.empty()) {
        id = free_ids.back();
        free_ids.pop_back();
    } else {
        id = next_id++;
        versions.push_back(0);
        records.emplace_back(); //EntityRecord{ nullptr, 0, {} }
    }

    return Entity{ id, versions[id] };
}

//ENTITY HELPERS END
//---------------------------------------------------------------------------------------------------------------------
//REGISTRY HELPERS

auto Registry::entityExists(const Entity& _entity) -> const bool {
    bool result = _entity.id < versions.size() && versions[_entity.id] == _entity.version; 
    return result;
}


auto Registry::getArchetype(const Signature& _signature) -> Archetype* {
    auto it = signature_map.find(_signature);
    if (it != signature_map.end())
        return it->second;
        
    std::unique_ptr<Archetype> new_archetype = std::make_unique<Archetype>();
    new_archetype->signature = _signature;

    Archetype* raw_archetype = new_archetype.get();
    signature_map[_signature] = raw_archetype;

    archetypes.push_back(std::move(new_archetype));

    invalidateQueryCache();
    return raw_archetype;
}


void Registry::invalidateQueryCache() {
    query_cache.clear();
    //could maybe track which signatures have have changed and just invalidate those
}


//REGISTRY HELPERS END
//---------------------------------------------------------------------------------------------------------------------
//STRUCT METHODS

auto Registry::Archetype::ensureComponentArray(uint32_t _type, sComponentArray* _source_array) -> sComponentArray* {
    auto it = component_arrays.find(_type);
    if (it == component_arrays.end()) {
        std::unique_ptr<sComponentArray> new_array(_source_array->cloneEmpty());
        auto [it, _] = component_arrays.emplace(_type, std::move(new_array));

        return it->second.get();
    }

    return it->second.get();
};


void Registry::Archetype::transferComponents(const Archetype* _source, size_t _source_index) {
    for (auto& [type, source_array] : _source->component_arrays) {
        if (signature.test(type)) {
            sComponentArray* target = ensureComponentArray(type, source_array.get());
            source_array->moveElement(_source_index, target);
        }
        
    }
};


//STRUCT METHODS END