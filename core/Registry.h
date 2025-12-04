#pragma once

#include <bitset>
#include <vector>
#include <unordered_map>
#include <cassert>
#include <memory>

#include "Entity.h"
#include "Event.h"

//switch to std::unordered_set<uint32_t> if flexibility is wanted
constexpr size_t MAX_COMPONENTS = 64;
using Signature = std::bitset<MAX_COMPONENTS>;

struct SignatureHash {
    size_t operator()(const Signature& _signature) const {
        return std::hash<unsigned long long>{}(_signature.to_ullong());
    }
};

class Registry {
public:
    
    struct sComponentArray {
        virtual ~sComponentArray() = default;

        virtual auto size() const -> size_t = 0; 

        virtual void swapElements(size_t _a, size_t _b) = 0;
        virtual void removeLast() = 0;
        virtual void addFrom(void* _component) = 0;
        virtual auto cloneEmpty() const -> sComponentArray* = 0;
        virtual auto getRaw(size_t _index) -> void* = 0;
    };


    template<typename T>
    struct ComponentArray : sComponentArray {
        std::vector<T> data;

        void add(const T& _component);
        void remove(size_t _index);
        auto get(size_t _index) -> T&;
        auto size() const -> size_t override;

        void swapElements(size_t _a, size_t _b) override;
        void removeLast() override;
        void addFrom(void* _component) override;
        auto cloneEmpty() const -> sComponentArray* override;
        auto getRaw(size_t _index) -> void* override;
    };


    struct Archetype {
        Signature signature;
        std::vector<Entity> entities;
        std::unordered_map<uint32_t, std::unique_ptr<sComponentArray>> component_arrays; //could make array if keeping fixed size (prolly wont)

        template<typename T> auto getArray() -> ComponentArray<T>*;

        auto ensureComponentArray(uint32_t _type, sComponentArray* _source_array) -> sComponentArray*;
        void migrateComponents(const Archetype* _source, size_t _source_index);
    };


    struct EntityRecord {
        //Entity entity; could have for debugging
        Archetype* archetype;
        uint32_t index;
        Signature signature;
    };

    struct QueryFilter {
        Signature include;
        Signature exclude;

        bool operator==(const QueryFilter& other) const {
            return include == other.include && exclude == other.exclude;
        }
    };

    struct FilterHash {
        size_t operator()(const QueryFilter _filter) const {
            size_t include_hash = std::hash<Signature>{}(_filter.include);
            size_t exclude_hash = std::hash<Signature>{}(_filter.exclude);

            //boost::hash combine
            return include_hash ^ (exclude_hash + 0x9e3779b9 + (include_hash << 6) + (include_hash >> 2));
        }
    };

    template<typename Excluded>
    struct Exclude{};

    ~Registry();

    //ENTITY MANAGEMENT-----------------------------------------------------------------

    template<typename... Components> 
    auto createEntity(Components&&... _components) -> Entity;
    auto createEntity(const Signature& _signature) -> Entity;
    auto createEntity() -> Entity;

    void destroyEntity(const Entity& _entity);

    auto moveEntity(Entity _entity, const Signature& _new_signature) -> Archetype*;

    //COMPONENT MANAGEMENT-----------------------------------------------------------------

    template<typename T, typename... Args>
    void addComponent(const Entity& _entity, Args&&... args);
    void addComponent(const Entity& _entity, uint32_t _type, sComponentArray* _component);

    template<typename T>
    void removeComponent(const Entity& _entity);
    void removeComponent(const Entity& _entity, uint32_t _type);

    template<typename T>
    auto getComponent(const Entity&) -> T&;
    auto getComponent(const Entity& _entity, uint32_t _type) -> void*;

    template<typename T>
    auto hasComponent(const Entity& _entity) -> bool;
    auto hasComponent(const Entity& _entity, uint32_t _type) -> bool;

    //QUERYING--------------------------------------------------------------------------------

    template<typename... Components> 
    auto query() const-> const std::vector<const Archetype*>&;
    template<typename... Included, typename... Excluded>
    auto query(Exclude<Excluded...>) const -> const std::vector<const Archetype*>&;
    auto query(const QueryFilter _filter) const -> const std::vector<const Archetype*>&;
    auto query(const uint32_t& _entity_id) const -> const EntityRecord&;

    //HELPERS---------------------------------------------------------------------------------

    auto entityExists(const Entity& _entity) -> const bool;
    template<typename T> static auto getComponentTypeID() -> uint32_t;

    //MEMBERS---------------------------------------------------------------------------------

    sEvent<Entity> onEntityCreated;
    sEvent<Entity> onEntityDestroyed;
    std::unordered_map<uint32_t, sEvent<Entity, uint32_t>> onComponentAdded;
    std::unordered_map<uint32_t, sEvent<Entity, uint32_t>> onComponentRemoved;

    static inline uint32_t next_type_id = 0; //should change if multithreading, atomic<uint32_t>

    uint32_t next_id = 0;
    std::vector<uint32_t> free_ids; // = free_ids.reserve(max entity count);
    
    std::vector<uint32_t> versions;
    std::vector<EntityRecord> records;
    std::vector<std::unique_ptr<Archetype>> archetypes;

    std::unordered_map<Signature, Archetype*, SignatureHash> signature_map;

    mutable std::unordered_map<QueryFilter, std::vector<const Archetype*>, FilterHash> query_cache;

private:

    void invalidateQueryCache();
    void invalidateEntity(const Entity& _entity);
    void updateEntityRecord(Entity _entity, Archetype* _new, size_t _new_index);

    void removeEntityAt(Archetype* _archetype, size_t _index);
    void moveToArchetype(Entity _entity, Archetype* _source, size_t _source_index, Archetype* _target);

    auto getArchetype(const Signature& _signature) -> Archetype*;
    
    auto allocateEntity() -> Entity;

    template<typename T> void addComponentToArray(Archetype* _archetype, const T& _component);
};

#include "Registry.inl"