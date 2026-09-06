#pragma once

#include "Registry.h"

/////////////////////////////////////////////////////////////////////////
//ComponentInfo
/////////////////////////////////////////////////////////////////////////

template<typename T>
void ComponentInfo::setInfo(uint32_t _id) {
    id = _id;

    element_size = sizeof(T);
    element_alignment = alignof(T);

    if (!std::is_trivially_destructible<T>::value) {
        destructor = [](void* _object_ptr) {
            static_cast<T*>(_object_ptr)->~T();
            };
    } else { 
        destructor = nullptr; 
    }

    move = [](void* _source, void* _destination) {
        new (_destination) T(std::move(*static_cast<T*>(_source)));
    };
}

/////////////////////////////////////////////////////////////////////////
//ComponentView
/////////////////////////////////////////////////////////////////////////

template<typename T>
ComponentView<T>::ComponentView(ComponentArray& _component_array)
    : component_array(_component_array)
{ }

template<typename T>
auto ComponentView<T>::operator[](size_t _index) -> T& {
    return *static_cast<T*>(component_array.get(_index));
}

template<typename T>
auto ComponentView<T>::operator[](size_t _index) const -> const T& {
    return *static_cast<const T*>(component_array.get(_index));
}

/////////////////////////////////////////////////////////////////////////
//Chunk
/////////////////////////////////////////////////////////////////////////

template<typename T>
void Registry::Chunk::createAt(size_t _index, size_t _array_index, T&& _data) {
    new (component_arrays[_array_index].get(_index)) T(std::forward<T>(_data));
}


/////////////////////////////////////////////////////////////////////////
//Archetype
/////////////////////////////////////////////////////////////////////////

template<typename T>
auto Registry::Archetype::getLocalIndex() const -> size_t {
    uint32_t type = Registry::getComponentTypeID<T>();

    if (!signature.test(type))
        return SIZE_MAX;

    return id_to_index.find(type)->second;
}


/////////////////////////////////////////////////////////////////////////
//Registry
/////////////////////////////////////////////////////////////////////////

template<typename T>
static auto Registry::getComponentTypeID() -> uint32_t {
    //static makes this lambda only run once per type T

    //to "clean" the type, T become same as T&
    using cleanT = std::decay<T>;

    static uint32_t id = type_id.fetch_add(1);

    static bool set_info = []() {
        auto* new_info = new ComponentInfo();
        new_info->setInfo<T>(id);
        Registry::info_list[id] = new_info;

        return true;
    }();
    

    return id;
}

template<typename... Components>
void Registry::addComponents(Entity _entity, Components&&... _data) {
    if (!entityExists(_entity))
        return;

    EntityRecord& prev_record = records[_entity.id];

    //get final signature
    //start with copying current sig or create new
    const Signature prev_signature = prev_record.archetype ? prev_record.archetype->signature : Signature{};
    Signature target_signature = prev_signature;

    (target_signature.set(getComponentTypeID<std::decay_t<Components>>()), ...);

    moveEntity(_entity, target_signature);

    EntityRecord& record = records[_entity.id]; //update record variable after moving entity

    //construct new components not present in the source archetype
    //running this through a lambda inside a fold expression to handle
    //every component in the _data parameter pack

    //would use templated lambda if c++20

    const auto& create_new = [&](auto&& _data) {
        using T = decltype(_data);
        using Component = std::decay_t<T>;
        const uint32_t type = getComponentTypeID<Component>();

        if (prev_signature.test(type))
            return; //if component already exists

        size_t local_index = record.archetype->getLocalIndex(type);
        record.archetype->chunks[record.chunk_index].createAt(record.index, local_index, std::forward<T>(_data));
    };

    (create_new(std::forward<Components>(_data)), ...);
}

template<typename... Components>
void Registry::removeComponents(Entity _entity) {
    if (!entityExists(_entity))
        return;

    EntityRecord& record = records[_entity.id];

    if (!record.archetype)
        return; //if entity doesn't have any components

    //get final 
    Signature target_signature = record.archetype->signature;
    (target_signature.reset(getComponentTypeID<Components>()), ...);

    moveEntity(_entity, target_signature);
}

template<typename Component> 
auto Registry::tryGetComponent(Entity _entity) -> Component* {
    EntityRecord& record = records[_entity.id];

    if (!record.archetype)
        return nullptr;

    const auto& type = getComponentTypeID<Component>();

    if (record.archetype->signature.test(type) == 0)
        return nullptr;

    const size_t& local_index = record.archetype->getLocalIndex(type);
    
    auto a = static_cast<Component*>(
        record.archetype->chunks[record.chunk_index].component_arrays[local_index].get(record.index)
    );

    return a;
};


template<typename... Components>
auto Registry::query() -> Query::QueryView<Components...> {
    Signature signature;
    (signature.set(getComponentTypeID<Components>()), ...);

    return Query::QueryView<Components...>(query(signature));
}

template<typename... Included, typename... Excluded>
auto Registry::query(Exclude<Excluded...>) -> Query::QueryView<Included...> {
    Signature signature;
    (signature.set(getComponentTypeID<Included>()), ...);
    (signature.reset(getComponentTypeID <Excluded>()), ...);

    return Query::QueryView<Included...>(query(signature));
}