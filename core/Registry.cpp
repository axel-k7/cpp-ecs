#include "Registry.h"

Registry::~Registry() {
    signature_map.clear();
    archetypes.clear();
}


//---------------------------------------------------------------------------------------------------------------------
//ENTITY

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

    Archetype* archetype = records[entity.id].archetype;
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

//ENTITY END
//---------------------------------------------------------------------------------------------------------------------
//COMPONENTS

//ADD-------------------------------------------------------------------------------------------


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

    sComponentArray* array = record.archetype->ensureComponentArray(_type, _component);
    array->addFrom(_component);
}


template<typename T>
void Registry::addComponent(const Entity& _entity, const T _component) {
    assert(isValidEntity(_entity));

    EntityRecord& record = records[_entity.id];
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

    addComponentToArray(target_archetype, _component);
}

//ADD END-------------------------------------------------------------------------------------------
//REMOVE--------------------------------------------------------------------------------------------

void Registry::removeComponent(const Entity& _entity, ComponentType _type) {
    assert(isValidEntity(_entity));

    EntityRecord& record = records[_entity.id];
    Archetype* source = record.archetype;
    if (!source)
        return;
    
    Signature new_signature = record.signature;
    new_signature.set(_type, false);

    Archetype* target = getArchetype(new_signature);
    moveToArchetype(_entity, source, record.index, target);

    record.archetype = target;
    record.signature = new_signature;
}

template<typename T>
void Registry::removeComponent(const Entity& _entity) {
    assert(isValidEntity(_entity));

    EntityRecord& record = records[_entity.id];
    Archetype* source = record.archetype;
    if (!source)
        return;

    ComponentType type = getComponentTypeID<T>();
    Signature new_signature = record.signature;
    new_signature.set(type, false);

    Archetype* target = getArchetype(new_signature);
    moveToArchetype(_entity, source, record.index, target);

    record.archetype = target;
    record.signature = new_signature;
}



//REMOVE END----------------------------------------------------------------------------------------
//GET-----------------------------------------------------------------------------------------------


auto Registry::getComponent(const Entity& _entity, ComponentType _type) -> void* {
    assert(isValidEntity(_entity));

    EntityRecord& record = records[_entity.id];
    auto it = record.archetype->component_arrays.find(_type);
    if (it == record.archetype->component_arrays.end()) {
        return nullptr;
    } 

    return it->second->getRaw(record.index);
} 

template<typename T>
auto Registry::getComponent(const Entity&) -> T& {
    assert(isValidEntity(_entity));

    EntityRecord& record = records[_entity.id];
    ComponentArray* array = record.archetype->getArray<T>();

    return array->get(record.index);
} 

//GET END-------------------------------------------------------------------------------------------

//COMPONENTS END
//---------------------------------------------------------------------------------------------------------------------
//QUERIES

    //query archetype of same bitmask
auto Registry::query(const Signature& _signature) const -> const std::vector<const Archetype*>& {
    auto it = query_cache.find(_signature);
    if (it != query_cache.end())
        return it->second;


    std::vector<Archetype*> result;

    for (const auto& archetype_ptr : archetypes) {
        Archetype* archetype = archetype_ptr.get();
        if ((archetype->signature & _signature) == _signature)
            result.push_back(archetype);
    }
    
    auto [_it, _] = query_cache.emplace(_signature, std::move(result));
    return _it->second;
}

    //get bitmask of components and query them
template<typename... Components>
auto Registry::query() -> std::vector<Archetype*> {
    Signature signature;
    (signature.set(getComponentTypeID<Components>()), ...);
    return query(signature);
}

//HELPERS -------------------------------------------------------------------------------------------------------------------------

//REGISTRY

auto Registry::isValidEntity(const Entity& _entity) -> const bool {
    bool result = _entity.id < versions.size() && versions[_entity.id] == _entity.version; 
    assert(result && "invalid entity handle");
    return result;
}


template<typename T>
auto Registry::getComponentTypeID() -> ComponentType {
    ComponentType type_id = static_cast<ComponentType>(next_type_id++);

    static_assert(next_type_id < MAX_COMPONENTS, "max component limit reached");

    return type_id;
}


Registry::Archetype* Registry::getArchetype(const Signature& _signature) {
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


//COMPONENTS-------------------------------------------------------------------------------------------------------------------


template<typename T>
auto Registry::hasComponent(const Entity& _entity) -> bool {
    assert(isValidEntity(_entity));
    const auto& record = records[_entity.id];
    return record.signature.test(getComponentTypeID<T>());
}


auto Registry::hasComponent(const Entity& _entity, ComponentType _type) -> bool {
    assert(isValidEntity(_entity));
    return records[_entity.id].signature.test(_type);
}


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

    _source->transferComponents(_target, _source_index);

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

void Registry::invalidateQueryCache() {
    query_cache.clear();
}

//STRUCT STUFF

template<typename T>
auto Registry::Archetype::getArray() -> ComponentArray<T>* {
    auto it = component_arrays.find(Registry::getComponentTypeID<T>());

    if (it != component_arrays.end())
        return static_cast<ComponentArray<T>*>(it->second);

    return nullptr;
}


auto Registry::Archetype::ensureComponentArray(ComponentType _type, sComponentArray* _source_array) -> Registry::sComponentArray* {
    auto it = component_arrays.find(_type);
    if (it == component_arrays.end()){
        std::unique_ptr<sComponentArray> new_array(_source_array->cloneEmpty());
        auto [it, _] = component_arrays.emplace(_type, std::move(new_array));

        return it->second.get();
    }

    return it->second.get();
};


void Registry::Archetype::transferComponents(const Archetype* _source, size_t _source_index) {
    for (auto& [type, source_array] : _source->component_arrays) {
        sComponentArray* target = ensureComponentArray(type, source_array.get());

        assert(typeid(*source_array) == typeid(*target));

        source_array->moveElement(_source_index, target);
    }
};