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
    assert(entityExists(_entity));

    Signature new_signature = records[_entity.id].signature;
    new_signature.set(getComponentTypeID<T>(), true);
    
    Archetype* target = moveEntity(_entity, new_signature);

    addComponentToArray(target, _component);
}


template<typename T>
void Registry::removeComponent(const Entity& _entity) {
    removeComponent(_entity, getComponentTypeID<T>());
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
    assert(entityExists(_entity));
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

    assert(type_id < MAX_COMPONENTS && "component limit reached");

    return type_id;
}

template<typename T>
void Registry::addComponentToArray(Archetype* _archetype, const T& _component) {
    auto* array = _archetype->getArray<T>();

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
//--------------------------------------------------------------------------------------------
//STRUCT METHODS

template<typename T>
auto Registry::Archetype::getArray() -> ComponentArray<T>* {
    auto it = component_arrays.find(Registry::getComponentTypeID<T>());

    if (it != component_arrays.end())
        return static_cast<ComponentArray<T>*>(it->second.get());

    return nullptr;
}


template<typename T>
void Registry::ComponentArray<T>::add(const T& _component) {
    data.push_back(_component);
}

template<typename T>
void Registry::ComponentArray<T>::remove(size_t _index) {
    data[_index] = data.back();
    data.pop_back();
}

template<typename T>
auto Registry::ComponentArray<T>::get(size_t _index) -> T& {
    return data[_index];
}

template<typename T>
size_t Registry::ComponentArray<T>::size() const {
    return data.size();
}

template<typename T>
void Registry::ComponentArray<T>::swapElements(size_t _a, size_t _b) noexcept {
    std::swap(data[_a], data[_b]);
}

template<typename T>
void Registry::ComponentArray<T>::moveElement(size_t _index, sComponentArray* _to) noexcept {
    auto* other = static_cast<ComponentArray<T>*>(_to);
    other->data.push_back(std::move(data[_index]));

    size_t last = data.size() -1;
    if (_index != last)
        data[_index] = std::move(data[last]);
    
    data.pop_back();
}

template<typename T>
void Registry::ComponentArray<T>::removeLast() noexcept {
    data.pop_back();
}

template<typename T>
void Registry::ComponentArray<T>::addFrom(void* _component) {
    add(*static_cast<T*>(_component));
}


template<typename T>
auto Registry::ComponentArray<T>::getRaw(size_t _index) -> void* {
    return &data[_index];
}

template<typename T>
auto Registry::ComponentArray<T>::cloneEmpty() const -> sComponentArray* {
    return new ComponentArray<T>();
}


//STRUCT METHODS END