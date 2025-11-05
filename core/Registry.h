#pragma once
//template and dynamic hybrid handling
//heavy systems should be templated
//light systems -> who care

#include <bitset>
#include <vector>
#include <unordered_map>
#include <cassert>

#include "Entity.h"

//probably change from this
constexpr size_t MAX_COMPONENTS = 64;
using Signature = std::bitset<MAX_COMPONENTS>;
using ComponentType = uint32_t;

class Registry {
public:
    struct sComponentArray {
        virtual ~sComponentArray() = default;

        virtual void swapElements(size_t _a, size_t _b) noexcept = 0;
        virtual void moveElement(size_t _index, sComponentArray* _to) noexcept = 0;
        virtual void removeLast() noexcept = 0;
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
        std::unordered_map<ComponentType, sComponentArray*> component_arrays; //could make array if keeping fixed size (prolly wont)

        ~Archetype();

        template<typename T> auto getArray() -> ComponentArray<T>*;

        auto ensureComponentArray(ComponentType _type, sComponentArray* _source_array) -> sComponentArray*;
        void cleanupComponentArrays();
        void transferComponents(const Archetype* _source, size_t _source_index);
    };

    struct EntityRecord {
        //Entity entity; could have for debugging
        Archetype* archetype;
        uint32_t index;
        Signature signature;
    };

    static inline uint32_t next_type_id = 0; //should change if multithreading

    uint32_t next_id;
    std::vector<uint32_t> free_ids; // = free_ids.reserve(max entity count);
    
    std::vector<uint32_t> versions;
    std::vector<EntityRecord> records;
    std::vector<Archetype*> archetypes;

    std::unordered_map<Signature, Archetype*> signature_map;

    template<typename... Components>
    auto createEntity(Components&&... _components);
    auto createEntity(const Signature& _signature) -> Entity;
    auto createEntity() -> Entity;

    template<typename... Components>
    void destroyEntity(const Entity& _entity);
    void destroyEntity(const Entity& _entity);

    template<typename T>
    void addComponent(const Entity& _entity, T _component);
    void addComponent(const Entity& _entity, ComponentType _type, sComponentArray* _component);

    template<typename T>
    void removeComponent(const Entity& _entity);
    void removeComponent(const Entity& _entity, ComponentType _type);

    template<typename T>
    auto getComponent(const Entity&) -> T&;
    auto getComponent(const Entity& _entity, ComponentType _type) -> void*;
    
    template<typename... Components>
    auto query() -> std::vector<Archetype*>;
    auto query(const Signature& _signature) -> std::vector<Archetype*>;
    
private:
    //HELPERS

    //REGISTRY------------------------------------------------------------------------------------------------------------------------

    const bool isValidEntity(const Entity& _entity);

    template<typename T> static ComponentType getComponentTypeID();

    Archetype* getArchetype(const Signature& _signature);

    //COMPONENTS-------------------------------------------------------------------------------------------------------------------

    template<typename T> void addComponentToArray(Archetype* _archetype, const T& _component);
    template<typename T> void deleteComponentArray(ComponentArray<T>* _array);
    template<typename T> void removeComponentAt(Archetype* _archetype, size_t _index);
    template<typename... Components> void removeComponentsAt(Archetype* _archetype, size_t _index);

    template<typename T>
    auto hasComponent(const Entity& _entity) -> bool;
    auto hasComponent(const Entity& _entity, ComponentType _type) -> bool;
    void removeEntityAt(Archetype* _archetype, size_t _index) noexcept;

    //ENTITIES-----------------------------------------------------------------------------------------------------------------------

    void updateEntityRecord(Entity _entity, Archetype* _new, size_t _new_index) noexcept;
    void moveToArchetype(Entity _entity, Archetype* _source, size_t _source_index, Archetype* _target);
    auto allocateEntity() -> Entity;
};