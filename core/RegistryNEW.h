#include <bitset>
#include <vector>
#include <unordered_map>
#include <cassert>
#include <memory>
#include <atomic>
#include <array>
#include <algorithm>
#include <bit>

#include "Entity.h"

/////////////////////////////////////////////////////////////////////////

//ComponentInfo is for knowing the size of a certain component
//so size can be reserved for data-chunking in archetypes

struct ComponentInfo {
    uint32_t id;
    size_t element_size; //byte size of the component, sizeof(T)
    size_t element_alignment; //alignment reqs (4-byte or 16-byte), alignof(T)

    //function pointers to real objects destructor and move operators
    void (*destructor)(void* _object_ptr);
    void (*move)(void* _source, void* _destination);

    template<typename T>
    ComponentInfo setInfo(uint32_t _id) {
        id = _id;
        
        element_size = sizeof(T);
        element_alignment = alignof(T);

        destructor = [](void* _object_ptr) {
            static_cast<T*>(_object_ptr)->~T();
        };

        move = [](void* _source, void* _destination) {
            new (_destination) T(std::move(*static_cast<T*>(_source)));
        }
    }
};

//The raw component array, this is used for storage of
//multiple types which may not be known at compile time
//an array of iComponentArrays for example, can hold
//both a ComponentArray<Transform> and ComponentArray<Model>

//raw memory management! SCARY
//"new T()" allocates memory, then it calls the constructor
//for this type erased array, those two steps need to be separated
//due to the fact that we don't necessarily know what the constructor
//is at compile time

//have changed this now, the chunk itself handles the actual memory
//ComponentArray is now more like a view for it

struct ComponentArray {
    ComponentArray(void* _data_buffer, const ComponentInfo* _info)
        :   data_buffer(_data_buffer)
        ,   info(_info)
    {};

    void* data_buffer;  //actual data location
    const ComponentInfo* info;

    //could just use an operator[], but [i].get(i2) looks better than [i][i2]
    void* get(size_t _index) {
        return static_cast<uint8_t*>(data_buffer) + (info->element_size * _index);
    }
};

//component view is what the actual end systems want
//easier to read ComponentView<Position>
//basically just a wrapper that's not entirely necessary

template<typename T>
struct ComponentView {
    ComponentView(ComponentArray& _component_array)
        : component_array(_component_array)
    {};

    ComponentArray& component_array;

    auto operator[](size_t _index) -> T& {
        return *static_cast<T*>(component_array.get(_index));
    }

    auto operator[](size_t _index) const -> const T& {
        return *static_cast<const T*>(component_array.get(_index));
    }
};


/////////////////////////////////////////////////////////////////////////

//chunk size is currently capacity * total size of an entity + components
//should probably change this to a set size in memory for consistency
//or if expected chunk size > limit, use a set size
//if there's like one entity with a massive amount of components
constexpr size_t CHUNK_CAPACITY = 25;

constexpr size_t MAX_COMPONENTS = 64;
using Signature = std::bitset<MAX_COMPONENTS>;

struct SignatureHash {
    size_t operator()(const Signature& _signature) const {
        return std::hash<uint32_t>{}(_signature.to_ullong());
    }
};

class Registry {
public:
    struct Chunk {
        std::vector<Entity> entities;

        //chunk handles memory
        void* chunk_buffer;
        void* entity_buffer; //need to keep track of where the entities are, component arrays know where they are after construction
        std::vector<ComponentArray> component_arrays;
        
        size_t count;
        size_t capacity;

        size_t highest_alignment;

        Chunk(const std::vector<const ComponentInfo*> _components, size_t _capacity) 
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


        ~Chunk() {
            //loop through all arrays and call destructors
            //then free the chunk buffer
            for (auto& component_array : component_arrays) {
                if (!component_array.info->destructor)
                    continue;

                for (size_t i = 0; i < count; ++i) {
                    component_array.info->destructor(component_array.get(i));
                }
            }

            operator delete(chunk_buffer, std::align_val_t(highest_alignment));
        }

        void pushBack(Entity& _entity, void** _elements) {
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

        //swap and pop
        void remove(size_t _index) {
            if (_index >= count)
                return;

            size_t last_index = count - 1;

            if (_index < last_index) {
                //swap entity ids
                static_cast<Entity*>(entity_buffer)[_index] = static_cast<Entity*>(entity_buffer)[last_index];

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
        }
    };

    struct Archetype {
        const Signature& signature;
        std::vector<Chunk> chunks;

        std::vector<const ComponentInfo*> active_components;

        Archetype(const Signature& _signature, const Registry* _registry)
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

        template<typename T>
        auto getLocalIndex() const ->  size_t {
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

        auto ensureChunk() -> Chunk& {
            //find first non-filled chunk
            for (auto& chunk : chunks) {
                if (chunk.count >= chunk.capacity)
                    continue;

                return chunk;
            }

            //or create a new one
            Chunk* new_chunk = new Chunk(active_components, CHUNK_CAPACITY);
            chunks.push_back(*new_chunk);

            return *new_chunk;
        }
    };

    //don't think there's a better way to connect a signature to an archetype
    //systems should not have to do "get archetype" through query each and every frame
    //the pointer should be saved or subscribed to after an inital "ensureArchetype"
    std::unordered_map<Signature, Archetype*, SignatureHash> signature_map;

    struct EntityRecord {
        Archetype* archetype;
        Chunk* chunk;
        size_t index;
    };


    //atomic = multiple threads can use it at the same time
    static inline std::atomic<uint32_t> type_id;
    template<typename T>
    static auto getComponentTypeID() -> uint32_t {
        static uint32_t id = type_id.fetch_add(1);
        return id;
    }

    ComponentInfo* info_list[MAX_COMPONENTS];
    auto getComponentInfo(uint32_t _component_id) const -> ComponentInfo* {
        return info_list[_component_id];
    }


    uint32_t next_id = 0;
    std::vector<uint32_t> free_ids;
    std::vector<uint32_t> versions;
    std::vector<EntityRecord> records;

    auto allocateEntity() -> Entity {
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

    auto entityExists(const Entity& _entity) -> const bool {
        return _entity.id < versions.size() && versions[_entity.id] == _entity.version;
    }


};