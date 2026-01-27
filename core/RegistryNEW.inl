#pragma once

#include "RegistryNEW.h"

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
    new (component_arrays[_array_index-1].get(_index)) T(std::forward<T>(_data));
}


/////////////////////////////////////////////////////////////////////////
//Archetype
/////////////////////////////////////////////////////////////////////////

template<typename T>
auto Registry::Archetype::getLocalIndex() const -> size_t {
    uint32_t type = Registry::getComponentTypeID<T>();

    if (!signature.test(type))
        return SIZE_MAX;

    if (type == 0)
        return 0;

    //destroy the "type" bit and everything above it, count the surviving active ones
    //local index = amount of active bits before yourself
    //popcount basically, but i thought of it myself ehehe ;)

    Signature mask = signature;
    mask <<= (MAX_COMPONENTS - type);

    return mask.count();
}


/////////////////////////////////////////////////////////////////////////
//Registry
/////////////////////////////////////////////////////////////////////////

template<typename T>
static auto Registry::getComponentTypeID() -> uint32_t {
    //static makes this lambda only run once per type T

    static uint32_t(*create_id)() = []() {
        uint32_t new_id = type_id.fetch_add(1);

        auto* new_info = new ComponentInfo();

        new_info->setInfo<T>(new_id);

        Registry::info_list[new_id] = new_info;

        return new_id;
    };
    

    return create_id();
}

template<typename... Components>
void Registry::addComponents(Entity _entity, Components&&... _data) {
    if (!entityExists(_entity))
        return;

    EntityRecord& record = records[_entity.id];

    //get final signature
    //start with current sig or create new
    Signature target_signature = record.archetype ? record.archetype->signature : Signature();
    (target_signature.set(getComponentTypeID<Components>()), ...);
    
    if (record.archetype && target_signature == record.archetype->signature)
        return; //if trying to add components the entity already has, maybe add warning message here?

    Archetype* target_archetype = ensureArchetype(target_signature);
    Chunk& target_chunk = target_archetype->ensureChunk();
    size_t target_index = target_chunk.count;

    //if entity already had components, move them into the new chunk
    //loop over previous active components -> move
    if (record.archetype) {
        for (const ComponentInfo* info : record.archetype->active_components) {
            target_chunk.moveComponent(
                target_index, target_archetype->getLocalIndex(info->id),
                record.chunk, record.index, record.archetype->getLocalIndex(info->id),
                info
            );
        }
    }

    //construct new components not present in the source archetype
    //running this through a lambda inside a fold expression to handle
    //every component in the _data parameter pack

    //would use templated lambda if c++20

    const auto& create_new = [&](auto&& _data) {
        using T = decltype(_data);
        using Component = std::decay_t<T>;
        uint32_t type = Registry::getComponentTypeID<Component>();

        if (!record.archetype || !record.archetype->signature.test(type)) {
            target_chunk.createAt<Component>(
                target_index,
                target_archetype->getLocalIndex(type),
                std::forward<T>(_data)
            );
        }
    };

    (create_new(std::forward<Components>(_data)), ...);


    target_chunk.pushEntity(_entity);

    if (record.chunk) {
        eraseChunkEntry(record.chunk, record.index);
    }

    updateEntityRecord(_entity, target_archetype, &target_chunk, target_index);
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

    if (target_signature == record.archetype->signature)
        return; //if trying to remove components the entity doesn't have

    Archetype* target_archetype = ensureArchetype(target_signature);
    Chunk& target_chunk = target_archetype->ensureChunk();
    size_t target_index = target_chunk.count;

    //can guarantee final archetype here will only include components which
    //the previous one already had
    for (const ComponentInfo* info : target_archetype->active_components) {
        target_chunk.moveComponent(
            target_index, target_archetype->getLocalIndex(info->id),
            record.chunk, record.index, record.archetype->getLocalIndex(info->id),
            info
        );
    }

    eraseChunkEntry(record.chunk, record.index);

    target_chunk.pushEntity(_entity);

    updateEntityRecord(_entity, target_archetype, &target_chunk, target_index);
}

template<typename Component> 
auto Registry::tryGetComponent(Entity _entity) -> Component* {
    EntityRecord& record = records[_entity.id];
    const auto& type = getComponentTypeID<Component>();

    if (record.archetype->signature.test(type) == 0)
        return nullptr;

    const size_t& local_index = record.archetype->getLocalIndex(type);
    
    return static_cast<Component*>(record.chunk->component_arrays[local_index].get(record.index));
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