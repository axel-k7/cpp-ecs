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

    if (!std::is_trivially_destructible<T>) {
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
{
};

template<typename T>
auto ComponentView<T>::operator[](size_t _index) -> T& {
    return *static_cast<T*>(component_array.get(_index));
}

template<typename T>
auto ComponentView<T>::operator[](size_t _index) const -> const T& {
    return *static_cast<const T*>(component_array.get(_index));
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
    static uint32_t id = type_id.fetch_add(1);
    return id;
}

template<typename... Components>
void Registry::addComponents(Entity _entity, Components&&... _data) {
    if (!entityExists(_entity))
        return;

    EntityRecord& record = records[_entity.id];
    Archetype* curr_archetype = record.archetype;
    Chunk* curr_chunk = record.chunk;
    size_t curr_index = record.index;

    //get final signature
    //start with current sig or create new
    Signature target_signature = curr_archetype ? curr_archetype->signature : Signature();
    (target_signature.set(getComponentTypeID<Components>()), ...);
    
    if (curr_archetype && target_signature == curr_archetype->signature)
        return; //if trying to add a component the entity already has, maybe add error message here?

    Archetype* target_archetype = getArchetype(target_signature);
    Chunk& target_chunk = target_archetype->ensureChunk();
    size_t target_index = target_chunk.count;

    //if entity already had components, move them into the new chunk
    //loop over previous active components -> move
    if (curr_archetype) {
        for (const ComponentInfo* info : curr_archetype->active_components) {

            size_t curr_array_index     = curr_archetype->getLocalIndex(info->id);
            size_t target_array_index   = target_archetype->getLocalIndex(info->id);

            void* source = curr_chunk->component_arrays[curr_array_index].get(curr_index);
            void* target = target_chunk->component_arrays[target_array_index].get(target_index);

            info->move(source, target);
            info->destructor(source);
        }
    }

    //construct or move new components
    //doing this through a lambda inside a fold expression to handle
    //every component in the _data parameter pack
    (
        [&]<typename T>(T&& _data) {
            using Component = std::decay_t<T>;
            size_t target_array_index = target_archetype->getLocalIndex<Component>();
            void* target = target_chunk->component_arrays[target_array_index].get(target_index);

            new (target) Component(std::forward<T>(_data));
        } 
    (std::forward<Components>(_data)), ...);

    target_chunk.entities[target_index] = _entity;
    target_chunk.count++;

    updateEntityRecord(_entity, target_archetype, &target_chunk, target_index);

    if (curr_chunk) {
        Entity swapped_entity = curr_chunk->swapPop(curr_index);
        if (!swapped_entity.isNull())
            records[swapped_entity.id].index = curr_index;
    }

}