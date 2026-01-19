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
    , entities(std::move(_source.entities))
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
        entities = std::move(_source.entities);
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


void Registry::Chunk::pushBack(Entity& _entity, void** _elements) {
    if (count >= capacity)
        return; //chunk split?

    //store entity at the back of entity buffer
    static_cast<Entity*>(entity_buffer)[count] = _entity;

    //move all relevant data
    for (size_t i = 0; i < component_arrays.size(); ++i) {
        void* destination = component_arrays[i].get(count);
        void* source = _elements[i];

        component_arrays[i].info->move(source, destination);
    }

    count++;
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

            array.info->destructor(target); //destroy target element
            array.info->move(last, target); //swap last element into the now empty slot
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
//Registry
/////////////////////////////////////////////////////////////////////////

auto Registry::getComponentInfo(uint32_t _component_id) const -> ComponentInfo* {
    return info_list[_component_id];
}


auto Registry::getArchetype(const Signature _signature) -> Archetype* {
    auto it = signature_map.find(_signature);
    if (it != signature_map.end())
        return it->second;

    std::unique_ptr<Archetype> new_archetype = std::make_unique<Archetype>(_signature, this);

    Archetype* raw_archetype = new_archetype.get();
    signature_map[_signature] = raw_archetype;

    archetypes.push_back(std::move(new_archetype));
    return raw_archetype;
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

    if (!moved_entity.isNull())
        records[moved_entity.id].index = record.index;

    free_ids.push_back(_entity.id);
    versions[_entity.id]++;
    invalidateEntity(_entity);
}

auto Registry::entityExists(const Entity& _entity) -> const bool {
    return _entity.id < versions.size() && versions[_entity.id] == _entity.version;
}

void Registry::moveToArchetype(Entity _entity, Archetype* _archetype, void** _elements) {
    Chunk& chunk = _archetype->ensureChunk();
    chunk.pushBack(_entity, _elements);

    updateEntityRecord(_entity, _archetype, &chunk, chunk.count);
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
    record.index = -1;
}