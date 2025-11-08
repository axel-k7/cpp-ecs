#pragma once

#include "Registry.h"

//---------------------------------------------------------------------------------------------------------------------
//ENTITY


template<typename... Components>
auto Registry::createEntity(Components&&... _components) {
    Signature signature;
    (signature.set(getComponentTypeID<Components>()), ...);

    Entity entity = allocateEntity();
    Archetype* archetype = getArchetype(signature);

    size_t new_index = archetype->entities.size();
    updateEntityRecord(entity, archetype, new_index);

    archetype->entities.push_back(entity);

    (addComponentToArray<Components>(archetype, std::forward<Components>(_components)), ...);

    onEntityCreated.trigger(entity);

    return entity;
}

//ignores template arguments, it's just like this for api consistency
template<typename... Components>
void Registry::destroyEntity(const Entity& _entity) {
    destroyEntity(_entity);
};


//ENTITY END
//---------------------------------------------------------------------------------------------------------------------
//COMPONENTS


template<typename T>
void Registry::addComponent(const Entity& _entity, const T _component) {
    assert(isValidEntity(_entity));

    Signature new_signature = records[_entity.id].signature;
    new_signature.set(getComponentTypeID<T>(), true);
    
    Archetype* target = moveEntity(_entity, new_signature);

    addComponentToArray(target, _component);
}


template<typename T>
void Registry::removeComponent(const Entity& _entity) {
    removeComponent(_entity, getComponentTypeID<T>())
}


template<typename T>
auto Registry::getComponent(const Entity& _entity) -> T& {
    assert(entityExists(_entity));

    EntityRecord& record = records[_entity.id];
    ComponentArray<T>* array = record.archetype->getArray<T>();

    return array->get(record.index);
} 


template<typename T>
auto Registry::hasComponent(const Entity& _entity) -> bool {
    assert(isValidEntity(_entity));
    const auto& record = records[_entity.id];

    return record.signature.test(getComponentTypeID<T>());
}



//COMPONENTS END
//---------------------------------------------------------------------------------------------------------------------
//QUERIES


    //get bitmask of components and query them
template<typename... Components>
auto Registry::query() const -> const std::vector<const Archetype*>& {
    Signature signature;
    (signature.set(getComponentTypeID<Components>()), ...);

    return query(signature);
}


//QUERIES END
//---------------------------------------------------------------------------------------------------------------------
//HELPERS


template<typename T>
auto Registry::getComponentTypeID() -> uint32_t {
    static const uint32_t type_id = static_cast<uint32_t>(next_type_id++);

    assert(type_id < MAX_COMPONENTS && "component limit reached")

    return type_id;
}

template<typename T>
void Registry::addComponentToArray(Archetype* _archetype, const T& _component) {
    auto* array = _archetype->getArray();

    if (!array) {
        auto new_array = std::make_unique<ComponentArray<T>>();
        array = new_array.get();
        _archetype->component_arrays[getComponentTypeID<T>()] = std::move(new_array);
    }

    array->add(_component);
};

template<typename T>
void Registry::removeComponentAt(Archetype* _archetype, size_t _index) {
    auto array = _archetype->getArray();

    if (array)
        array->remove(_index);
};


template<typename... Components>
void Registry::removeComponentsAt(Archetype* _archetype, size_t _index) {
    (removeComponentAt<Components>(_archetype, _index), ...);
};


template<typename T>
void Registry::deleteComponentArray(ComponentArray<T>* _array) {
    delete _array;
};


//HELPERS END