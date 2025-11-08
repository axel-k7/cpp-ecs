#pragma once

#include <bitset>
#include <vector>
#include <unordered_map>
#include <cassert>
#include <memory>

#include "RegistryInternal.h"
#include "Entity.h"
#include "Event.h"

//switch to std::unordered_set<uint32_t> if flexibility is wanted
constexpr size_t MAX_COMPONENTS = 64;
using Signature = std::bitset<MAX_COMPONENTS>;

class Registry {
public:

    ~Registry();

    template<typename... Components> 
    auto createEntity(Components&&... _components);
    auto createEntity() -> Entity;
    template<typename... Components> 
    void destroyEntity(const Entity& _entity);

    template<typename T> void addComponent(const Entity& _entity, T _component);
    template<typename T> void removeComponent(const Entity& _entity);
    template<typename T> auto getComponent(const Entity&) -> T&;
    template<typename T> auto hasComponent(const Entity& _entity) -> bool;

    template<typename... Components> 
    auto query() const-> const std::vector<const Archetype*>&;

    auto entityExists(const Entity& _entity) -> const bool;
    

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
    
    //INTERNAL DYNAMIC METHODS------------------------------------------------------------------------------------------------------

    auto createEntity(const Signature& _signature) -> Entity;
    void destroyEntity(const Entity& _entity);
    void addComponent(const Entity& _entity, uint32_t _type, sComponentArray* _component);
    void removeComponent(const Entity& _entity, uint32_t _type);
    auto getComponent(const Entity& _entity, uint32_t _type) -> void*;
    auto hasComponent(const Entity& _entity, uint32_t _type) -> bool;
    auto query(const Signature& _signature) const -> const std::vector<const Archetype*>&;

    //REGISTRY------------------------------------------------------------------------------------------------------------------------

    template<typename T> static auto getComponentTypeID() -> uint32_t;
    auto getArchetype(const Signature& _signature) -> Archetype*;
    void invalidateQueryCache();

    //COMPONENTS-------------------------------------------------------------------------------------------------------------------

    template<typename T> void addComponentToArray(Archetype* _archetype, const T& _component);
    template<typename T> void deleteComponentArray(ComponentArray<T>* _array);
    template<typename T> void removeComponentAt(Archetype* _archetype, size_t _index);
    template<typename... Components> void removeComponentsAt(Archetype* _archetype, size_t _index);

    //ENTITIES-----------------------------------------------------------------------------------------------------------------------
    
    auto moveEntity(Entity _entity, const Signature& _new_signature) -> Archetype*;
    void invalidateEntity(const Entity& _entity);
    void removeEntityAt(Archetype* _archetype, size_t _index) noexcept;
    void updateEntityRecord(Entity _entity, Archetype* _new, size_t _new_index) noexcept;
    void moveToArchetype(Entity _entity, Archetype* _source, size_t _source_index, Archetype* _target);
    auto allocateEntity() -> Entity;
};

#include "Registry.inl"