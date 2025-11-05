#include "Registry.h"

//CREATION------------------------------------------------------------------------------------------


auto Registry::createEntity() -> Entity {
    Entity entity = allocateEntity();
    Archetype* empty = getArchetype(Signature{});

    empty->entities.push_back(entity);
    records[entity.id] = {empty, (uint32_t)(empty->entities.size() - 1), {}};

    return entity;
};


auto Registry::createEntity(const Signature& _signature) -> Entity {
    Entity entity = allocateEntity();
    Archetype* target = getArchetype(_signature);

    target->entities.push_back(entity);
    records[entity.id] = {target, (uint32_t)(target->entities.size() - 1), _signature};

    return entity;
}


template<typename... Components>
auto Registry::createEntity(Components&&... _components) {
    Signature signature;
    (signature.set(getComponentTypeID<Components>()), ...);
    Entity entity = createEntity(signature);

    Archetype* archetype = records[e.id].archetype;
    (addComponentToArray<Components>(archetype, std::forward<Components>(_components)), ...);
    return entity;
}


//CREATION END-----------------------------------------------------------------------------------------
//DESTRUCTION------------------------------------------------------------------------------------------


void Registry::destroyEntity(const Entity& _entity) {
    assert(isValidEntity(_entity));
    
    EntityRecord& record = records[_entity.id];
    Archetype* archetype = record.archetype;
    if (!archetype)
        return;

    removeEntityAt(archetype, record.index);

    for (auto& [_, array] : archetype->component_arrays) {
        array->removeLast();
    }

    //invalidate version handle and reset record
    versions[_entity.id]++;
    record.archetype = nullptr;
    record.index = 0;
    record.signature.reset();

    free_ids.push_back(_entity.id);
};


template<typename... Components>
void Registry::destroyEntity(const Entity& _entity) {
    assert(isValidEntity(_entity));

    EntityRecord& record = records[_entity.id];
    Archetype* archetype = record.archetype;
    if (!archetype)
        return;

    size_t index = record.index;

    (removeComponentAt<Components>(archetype, index), ...);
    (record.signature.reset(getComponentTypeID<Components>()), ...);
    archetype->signature = record.signature;

    Archetype* new_archetype = getArchetype(record.signature);
    if (new_archetype != archetype) {
        moveToArchetype(_entity, archetype, index, new_archetype);
    }
};

//DESTRUCTION END--------------------------------------------------------------------------------------
//COMPONENTS-------------------------------------------------------------------------------------------


void Registry::addComponent(const Entity& _entity, ComponentType _type, sComponentArray* _component) {
    assert(isValidEntity(_entity));

    EntityRecord& record = records[_entity.id];
    Signature new_signature = record.signature;
    new_signature.set(_type, true);

    Archetype* target_archetype = getArchetype(new_signature);
    Archetype* prev_archetype = record.archetype;
    size_t prev_index = record.index;

    if (prev_archetype != target_archetype && prev_archetype) {
        moveToArchetype(_entity, prev_archetype, prev_index, target_archetype);
    }
    else if (!prev_archetype) {
        record.index = target_archetype->entities.size();
        target_archetype->entities.push_back(_entity);
        record.archetype = target_archetype;
    }

    record.signature = new_signature;

    sComponentArray* array = ensureComponentArray(target_archetype, _type, _component);
    array->addFrom(_component);
}


template<typename T>
void Registry::addComponent(const Entity& _entity, const T _component) {
    assert(isValidEntity(_entity));

    auto& record = records[_entity.id];

    Signature new_signature = record.signature;
    new_signature.set(getComponentTypeID<T>(), true);
    

    Archetype* target_archetype = getArchetype(new_signature);
    Archetype* prev_archetype = record.archetype;
    size_t prev_index = record.index;

    if (prev_archetype) {
        moveToArchetype(_entity, prev_archetype, prev_index, target_archetype);
    } 
    else if (!prev_archetype) {
        record.index = target_archetype->entities.size();
        target_archetype->entities.push_back(_entity);
        record.archetype = target_archetype;
    }

    
    record.signature = new_signature;

    addComponentToArray(target_archetype, _component)
}


//HELPERS -------------------------------------------------------------------------------------------------------------------------

//REGISTRY

const bool Registry::isValidEntity(const Entity& _entity) {
    bool result = _entity.id < versions.size() && versions[_entity.id] == _entity.version; 
    assert(result && "invalid entity handle");
    return result;
}


template<typename T>
static ComponentType Registry::getComponentTypeID() {
    static ComponentType type_id = static_cast<ComponentType>(next_type_id++);

    static_assert(next_type_id < MAX_COMPONENTS, "max component limit reached");

    return type_id;
}


Registry::Archetype* Registry::getArchetype(const Signature& _signature) {
    auto it = signature_map.find(_signature);
    if (it != signature_map.end())
        return it->second;
        
    Archetype* new_archetype = new Archetype();
    new_archetype->signature = _signature;
    signature_map[_signature] = new_archetype;
    archetypes.push_back(new_archetype);
    return new_archetype;
}


//COMPONENTS-------------------------------------------------------------------------------------------------------------------


void Registry::cleanupComponentArrays(Archetype* _archetype) {
    for (auto& [_, array] : _archetype->component_arrays)
        delete array;
    _archetype->component_arrays.clear();
};


Registry::sComponentArray* Registry::ensureComponentArray(Archetype* _target, ComponentType _type, sComponentArray* _source_array) {
    auto it = _target->component_arrays.find(_type);
    if (it == _target->component_arrays.end()){
        sComponentArray* new_array = _source_array->cloneEmpty();
        _target->component_arrays[_type] = new_array;
        return new_array;
    }
    return it->second;
};


void Registry::transferComponents(Archetype* _source, size_t _source_index, Archetype* _target) {
    for (auto& [type, source_array] : _source->component_arrays) {
        
        sComponentArray* target_array = ensureComponentArray(_target, type, source_array);
        source_array->moveElement(_source_index, target_array);
    }
};


//ENTITIES-----------------------------------------------------------------------


void Registry::removeEntityAt(Archetype* _archetype, size_t _index) noexcept {
    size_t last_index = _archetype->entities.size()-1;

    if (_index != last_index) {
        Entity last_entity = _archetype->entities[last_index];
        _archetype->entities[_index] = last_entity;
        records[last_entity.id].index = _index;
        
        for (auto& [_, array] : _archetype->component_arrays) {
            array->swapElements(_index, last_index);
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
    if (_source == _target || !isValidEntity(_entity))
        return;
    
    size_t new_index = _target->entities.size();
    _target->entities.push_back(_entity);

    transferComponents(_source, _source_index, _target);

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
        records.emplace_back(EntityRecord{ nullptr, 0, {} });
    }

    Entity entity{ id, versions[id] };
    return entity;
}