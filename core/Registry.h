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
    std::size_t operator()(const Signature& _signature) const {
        return std::hash<std::string>()(_signature.to_string());
    } //to string is inefficient
};

class Registry {
public:
    
    struct sComponentArray {
        virtual ~sComponentArray() = default;

        virtual void swapElements(size_t _a, size_t _b) = 0;
        virtual void moveElement(size_t _index, sComponentArray* _to) = 0;
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
        auto size() const -> size_t;

        void swapElements(size_t _a, size_t _b) override;
        void moveElement(size_t _index, sComponentArray* _to) override;
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
        void transferComponents(const Archetype* _source, size_t _source_index);
    };


    struct EntityRecord {
        //Entity entity; could have for debugging
        Archetype* archetype;
        uint32_t index;
        Signature signature;
    };


    ~Registry();

    //ENTITY MANAGEMENT-----------------------------------------------------------------

    template<typename... Components> 
    auto createEntity(Components&&... _components) -> Entity;
    auto createEntity(const Signature& _signature) -> Entity;
    auto createEntity() -> Entity;

    void destroyEntity(const Entity& _entity);

    //COMPONENT MANAGEMENT-----------------------------------------------------------------

    template<typename T>
    void addComponent(const Entity& _entity, const T& _component);
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
    auto query(const Signature& _signature) const -> const std::vector<const Archetype*>&;

    //HELPERS---------------------------------------------------------------------------------

    auto entityExists(const Entity& _entity) -> const bool;
    template<typename T> static auto getComponentTypeID() -> uint32_t;

    //MEMBERS---------------------------------------------------------------------------------

    sEvent<Entity> onEntityCreated;
    sEvent<Entity> onEntityDestroyed;
    std::unordered_map<uint32_t, sEvent<Entity, uint32_t>> onComponentAdded;
    std::unordered_map<uint32_t, sEvent<Entity, uint32_t>> onComponentRemoved;

    static inline uint32_t next_type_id = 0; //should change if multithreading, atomic<uint32_t>

    uint32_t next_id;
    std::vector<uint32_t> free_ids; // = free_ids.reserve(max entity count);
    
    std::vector<uint32_t> versions;
    std::vector<EntityRecord> records;
    std::vector<std::unique_ptr<Archetype>> archetypes;

    std::unordered_map<Signature, Archetype*, SignatureHash> signature_map;

    mutable std::unordered_map<Signature, std::vector<const Archetype*>> query_cache;

private:

    void invalidateQueryCache();
    void invalidateEntity(const Entity& _entity);
    void updateEntityRecord(Entity _entity, Archetype* _new, size_t _new_index);

    auto moveEntity(Entity _entity, const Signature& _new_signature) -> Archetype*;
    void removeEntityAt(Archetype* _archetype, size_t _index);
    void moveToArchetype(Entity _entity, Archetype* _source, size_t _source_index, Archetype* _target);

    auto getArchetype(const Signature& _signature) -> Archetype*;
    
    auto allocateEntity() -> Entity;

    template<typename T> void addComponentToArray(Archetype* _archetype, const T& _component);
};

#include "Registry.inl"