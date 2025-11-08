#pragma once

#include "Registry.h"

//UGLY BORING TEMPLATING STUFF GOES IN HERE :)

//---------------------------------------------------------------------------------------------------------------------
//ENTITY


template<typename... Components>
auto Registry::createEntity(Components&&... _components) {
    Signature signature;
    (signature.set(getComponentTypeID<Components>()), ...);

    Entity entity = allocateEntity();
    Archetype* archetype = moveEntity(entity, signature);

    (addComponentToArray<Components>(archetype, std::forward<Components>(_components)), ...);

    return entity;
}


template<typename... Components>
void Registry::destroyEntity(const Entity& _entity) {
    assert(isValidEntity(_entity));

    EntityRecord& record = records[_entity.id];
    if (!record.archetype)
        return;

    Signature new_signature = record.signature;
    (new_s.reset(getComponentTypeID<Components>()), ...);

    moveEntity(_entity, new_signature);

    if (new_signature.none()) {
        invalidateEntity(_entity);
    }
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
    assert(isValidEntity(_entity));

    Signature new_signature = records[_entity.id].signature;
    new_signature.set(getComponentTypeID<T>(), false);

    moveEntity(_entity, new_signature);
}


template<typename T>
auto Registry::getComponent(const Entity&) -> T& {
    assert(isValidEntity(_entity));

    EntityRecord& record = records[_entity.id];
    ComponentArray* array = record.archetype->getArray<T>();

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
auto Registry::query() -> std::vector<Archetype*> {
    Signature signature;
    (signature.set(getComponentTypeID<Components>()), ...);
    return query(signature);
}


//QUERIES END
//---------------------------------------------------------------------------------------------------------------------
//HELPERS


template<typename T>
auto Registry::getComponentTypeID() -> ComponentType {
    static const ComponentType type_id = static_cast<ComponentType>(next_type_id++);

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
//---------------------------------------------------------------------------------------------------------------------
//STRUCT METHODS

template<typename T>
auto Registry::Archetype::getArray() -> ComponentArray<T>* {
    auto it = component_arrays.find(Registry::getComponentTypeID<T>());

    if (it != component_arrays.end())
        return static_cast<ComponentArray<T>*>(it->second);

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
void Registry::ComponentArray<T>::swapElements(size_t _a, size_t _b) {
    std::swap(data[_a], data[_b]);
}

template<typename T>
void Registry::ComponentArray<T>::moveElement(size_t _index, sComponentArray* _to) {
    auto* other = static_cast<ComponentArray<T>*>(_to);
    other->data.push_back(std::move(data[_index]));

    size_t last = data.size() -1;
    if (_index != last)
        data[_index] = std::move(data[last]);
    
    data.pop_back();
}

template<typename T>
void Registry::ComponentArray<T>::removeLast() {
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
Registry::sComponentArray* Registry::ComponentArray<T>::cloneEmpty() const {
    return new ComponentArray<T>();
}