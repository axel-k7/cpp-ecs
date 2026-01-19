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
    void setInfo(uint32_t _id);
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
    ComponentArray(void* _data_buffer, const ComponentInfo* _info);

    void* data_buffer;  //actual data location
    const ComponentInfo* info;

    //could just use an operator[], but [i].get(i2) looks better than [i][i2]
    void* get(size_t _index);
};

//component view is what the actual end systems want
//easier to read ComponentView<Position>
//basically just a wrapper that's not entirely necessary

template<typename T>
struct ComponentView {
    ComponentView(ComponentArray& _component_array);

    ComponentArray& component_array;

    auto operator[](size_t _index)->T&;
    auto operator[](size_t _index) const -> const T&;
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
    /////////////////////////////////////////////////////////////////////////

    struct Chunk {
        Chunk(const std::vector<const ComponentInfo*> _components, size_t _capacity);
        ~Chunk();

        //disable copying
        Chunk(const Chunk&) = delete;
        Chunk& operator=(const Chunk&) = delete;

        //only allow moving, chunk controls memory on its own
        Chunk(Chunk&& _source);
        auto operator=(Chunk&& _source) -> Chunk&;

        std::vector<Entity> entities;

        //chunk handles memory
        void* chunk_buffer;
        void* entity_buffer; //need to keep track of where the entities are, component arrays know where they are after construction
        std::vector<ComponentArray> component_arrays;

        size_t count;
        size_t capacity;

        size_t highest_alignment;



        void pushBack(Entity& _entity, void** _elements);

        //swap and pop returns swapped entity
        auto swapPop(size_t _index) -> Entity;
    };

    /////////////////////////////////////////////////////////////////////////

    struct Archetype {
        const Signature& signature;
        std::vector<Chunk> chunks;

        std::vector<const ComponentInfo*> active_components;

        Archetype(const Signature& _signature, const Registry* _registry);

        auto ensureChunk() -> Chunk&;

        template<typename T>
        auto getLocalIndex() const -> size_t;
        auto getLocalIndex(uint32_t _id) const -> size_t;
    };

    /////////////////////////////////////////////////////////////////////////

    struct EntityRecord {
        Archetype* archetype;
        Chunk* chunk;
        size_t index;
    };

    /////////////////////////////////////////////////////////////////////////

    //don't think there's a better way to connect a signature to an archetype
    //systems should not have to do "get archetype" through query each and every frame
    //the pointer should be saved or subscribed to after an inital "ensureArchetype"
    std::unordered_map<Signature, Archetype*, SignatureHash> signature_map;

    std::vector<std::unique_ptr<Archetype>> archetypes;

    //atomic = multiple threads can use it at the same time
    static inline std::atomic<uint32_t> type_id;

    ComponentInfo* info_list[MAX_COMPONENTS];

    uint32_t next_id = 0;
    std::vector<uint32_t> free_ids;
    std::vector<uint32_t> versions;
    std::vector<EntityRecord> records;

    template<typename T>
    static auto getComponentTypeID() -> uint32_t;
    auto getComponentInfo(uint32_t _component_id) const -> ComponentInfo*;
    auto getArchetype(const Signature _signature) -> Archetype*;

    auto entityExists(const Entity& _entity) -> const bool;
    auto allocateEntity() -> Entity;
    void destroyEntity(Entity _entity);

    void moveToArchetype(Entity _entity, Archetype* _archetype, void** _elements);

    void updateEntityRecord(Entity _entity, Archetype* _archetype, Chunk* _chunk, size_t _chunk_index);
    void invalidateEntity(const Entity& _entity);

    template<typename... Components>
    void addComponents(Entity _entity, Components&&... _data);
};

#include "RegistryNEW.inl"