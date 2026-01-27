#include "RegistryNEW.h"


/////////////////////////////////////////////////////////////////////////
//ComponentArray
/////////////////////////////////////////////////////////////////////////

ComponentArray::ComponentArray(void* _data_buffer, const ComponentInfo* _info)
    : data_buffer(_data_buffer)
    , info(_info)
{

};

auto ComponentArray::get(size_t _index) -> void* {
    return static_cast<uint8_t*>(data_buffer) + (info->element_size * _index);
}

/////////////////////////////////////////////////////////////////////////
//Chunk
/////////////////////////////////////////////////////////////////////////

Registry::Chunk::Chunk(const std::vector<const ComponentInfo*> _components, size_t _capacity)
    : capacity(_capacity)
{
    //calculate size needed for the chunk
    size_t total_size = sizeof(Entity) * capacity;
    highest_alignment = alignof(Entity);
    
    for (const auto& component : _components) {
        if (component->element_alignment > highest_alignment)
            highest_alignment = component->element_alignment;
    
        //check for padding
        const size_t& padding = (component->element_alignment - (total_size % component->element_alignment)) % component->element_alignment;
        total_size += padding + component->element_size * capacity;
    }
    //--
    
    
    //allocate space for chunk and save its address
    chunk_buffer = operator new(total_size, std::align_val_t(highest_alignment));
    uint8_t* curr_address = static_cast<uint8_t*>(chunk_buffer);
    //---
    
    
    //data placement within the chunk
    entity_buffer = curr_address;
    curr_address += sizeof(Entity) * capacity;
    
    for (const auto& component : _components) {
        const std::uintptr_t& address_value = reinterpret_cast<std::uintptr_t>(curr_address);
        const size_t& padding = (component->element_alignment - (address_value % component->element_alignment)) % component->element_alignment;
    
        curr_address += padding;
    
        component_arrays.push_back(
            ComponentArray(curr_address, component)
        );
    
        curr_address += component->element_size * capacity;
    }
    //--
};

Registry::Chunk::Chunk(Chunk&& _source)
    : component_arrays(std::move(_source.component_arrays))
    , chunk_buffer(_source.chunk_buffer)
    , entity_buffer(_source.entity_buffer)
    , count(_source.count)
    , capacity(_source.capacity)
    , highest_alignment(_source.highest_alignment)
{
    //and nullify source chunk
    _source.chunk_buffer = nullptr;
    _source.count = 0;
}

Registry::Chunk::~Chunk(){
    //loop through all arrays and call destructors
    //then free the chunk buffer
    for (auto& component_array : component_arrays) {
        if (!component_array.info->destructor)
            continue;

        for (size_t i = 0; i < count; ++i) {
            component_array.info->destructor(component_array.get(i));
        }
    }

    if (chunk_buffer)
        operator delete(chunk_buffer, std::align_val_t(highest_alignment));
}

//destroy self, steal data, nullify source
auto Registry::Chunk::operator=(Chunk&& _source) -> Chunk& {
    if (this != &_source) {
        this->~Chunk();

        component_arrays = std::move(_source.component_arrays);
        chunk_buffer = _source.chunk_buffer;
        entity_buffer = _source.entity_buffer;
        count = _source.count;
        capacity = _source.capacity;
        highest_alignment = _source.highest_alignment;

        _source.chunk_buffer = nullptr;
        _source.count = 0;
    }

    return *this;
}

void Registry::Chunk::moveComponent(
    size_t _index, size_t _array_index,
    Chunk* _source, size_t _source_index, size_t _source_array_index,
    const ComponentInfo* _info
) {
    void* source = _source->component_arrays[_source_array_index].get(_source_index);

    //move then clean up previous location

    _info->move(
        source,
        component_arrays[_array_index].get(_index)
    );

    if (_info->destructor)
        _info->destructor(source);
}


void Registry::Chunk::destroyAt(size_t _index, const std::vector<const ComponentInfo*>& _components) {
    for (size_t i = 0; i < _components.size(); ++i) {
        if (_components[i]->destructor) {
            _components[i]->destructor(component_arrays[i].get(_index));
        }
    }
}

auto Registry::Chunk::swapPop(size_t _index) -> Entity {
    Entity swapped_entity = Entity::Null();

    if (_index >= count)
        return swapped_entity;

    size_t last_index = count - 1;

    if (_index < last_index) {
        //swap entity ids
        static_cast<Entity*>(entity_buffer)[_index] = swapped_entity;

        //move components
        for (auto& array : component_arrays) {
            void* target = array.get(_index);
            void* last = array.get(last_index);

            if (array.info->destructor)
                array.info->destructor(target); //destroy target element

            array.info->move(last, target); //swap last element into the now empty slot
    
            if (array.info->destructor)
                array.info->destructor(last);   //clean up old position
        }
    }
    else {
        for (auto& array : component_arrays) {
            array.info->destructor(array.get(_index));
        }
    }

    count--;
    return swapped_entity;
}

auto Registry::Chunk::getRaw(size_t _index, size_t _array_index) -> void* {
    return component_arrays[_array_index].get(_index);
};

auto Registry::Chunk::getEntities() -> Entity* {
    return static_cast<Entity*>(entity_buffer);
};

void Registry::Chunk::pushEntity(Entity _entity) {
    getEntities()[count] = _entity;
    count++;
}

/////////////////////////////////////////////////////////////////////////
//Archetype
/////////////////////////////////////////////////////////////////////////

Registry::Archetype::Archetype(const Signature& _signature, const Registry* _registry)
    : signature(_signature)
{
    for (uint32_t i = 0; i < MAX_COMPONENTS; ++i) {
        if (signature.test(i)) {
            active_components.push_back(_registry->getComponentInfo(i));
        }
    }

    std::sort(active_components.begin(), active_components.end(),
        [](const ComponentInfo* _a, const ComponentInfo* _b) {
            return _a->id < _b->id;
        }
    );
}


auto Registry::Archetype::ensureChunk() -> Chunk& {
    //find first non-filled chunk
    for (auto& chunk : chunks) {
        if (chunk.count < chunk.capacity)
            return chunk;
    }

    //or create a new one
    //creating it directly inside of the chunk list through emplace
    return chunks.emplace_back(active_components, CHUNK_CAPACITY);
}


auto Registry::Archetype::getLocalIndex(uint32_t _id) const -> size_t {
    if (!signature.test(_id))
        return SIZE_MAX;

    if (_id == 0)
        return 0;

    Signature mask = signature;
    mask <<= (MAX_COMPONENTS - _id);

    return mask.count();
}

/////////////////////////////////////////////////////////////////////////
//Query
/////////////////////////////////////////////////////////////////////////

Registry::Query::Query(Signature _signature) 
    : signature(_signature)
{}

void Registry::Query::tryMatch(Archetype* _archetype) {
    if ((_archetype->signature & signature) != signature)
        return;
    
    matching_archetypes.push_back(_archetype);

    for (auto& view : views) {
        view->pushArchetype(_archetype);
    }

}

/////////////////////////////////////////////////////////////////////////
//Registry
/////////////////////////////////////////////////////////////////////////

auto Registry::getComponentInfo(uint32_t _component_id) const -> ComponentInfo* {
    return info_list[_component_id];
}


auto Registry::ensureArchetype(const Signature _signature) -> Archetype* {
    auto it = signature_map.find(_signature);
    if (it != signature_map.end())
        return it->second;

    std::unique_ptr<Archetype> new_archetype = std::make_unique<Archetype>(_signature, this);
    Archetype* archetype_ptr = new_archetype.get();

    signature_map[_signature] = archetype_ptr;

    for (auto&& query : query_subscriptions) {
        query->tryMatch(archetype_ptr);
    }

    archetypes.push_back(std::move(new_archetype));
    return archetype_ptr;
}


auto Registry::allocateEntity() -> Entity {
    uint32_t id;

    //get id or create new one
    if (!free_ids.empty()) {
        id = free_ids.back();
        free_ids.pop_back();
    }
    else {
        id = next_id++;
        versions.push_back(0);
        records.emplace_back(); //EntityRecord{ nullptr, 0, {} }
    }

    return Entity{ id, versions[id] };
}

void Registry::destroyEntity(Entity _entity) {
    if (!entityExists(_entity))
        return;

    EntityRecord& record = records[_entity.id];

    Entity moved_entity = record.chunk->swapPop(record.index);

    if (moved_entity == Entity::Null())
        records[moved_entity.id].index = record.index;

    free_ids.push_back(_entity.id);
    versions[_entity.id]++;
    invalidateEntity(_entity);
}

auto Registry::entityExists(const Entity& _entity) -> const bool {
    return _entity.id < versions.size() && versions[_entity.id] == _entity.version;
}

void Registry::updateEntityRecord(Entity _entity, Archetype* _archetype, Chunk* _chunk, size_t _chunk_index) {
    EntityRecord& record = records[_entity.id];

    record.archetype = _archetype;
    record.chunk = _chunk;
    record.index = _chunk_index;
}

void Registry::invalidateEntity(const Entity& _entity) {
    EntityRecord& record = records[_entity.id];

    record.archetype = nullptr;
    record.chunk = nullptr;
    record.index = SIZE_MAX;
}

void Registry::eraseChunkEntry(Chunk* _chunk, size_t _index) {
    Entity swapped_entity = _chunk->swapPop(_index);
    if (swapped_entity != Entity::Null())
        records[swapped_entity.id].index = _index;
}


auto Registry::query(const Signature _signature) -> Query* {
    for (auto&& query : query_subscriptions) {
        if ((query->signature & _signature) == _signature)
            return query.get();
    }

    std::unique_ptr<Query> new_query = std::make_unique<Query>(_signature);

    for (auto& archetype : archetypes) {
        new_query->tryMatch(archetype.get());
    }

    Query* query_ptr = new_query.get();

    query_subscriptions.push_back(std::move(new_query));
    return query_ptr;
}

ComponentInfo* Registry::info_list[MAX_COMPONENTS] = {};