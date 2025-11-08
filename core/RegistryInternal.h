#pragma once

#include <vector>
#include <unordered_map>
#include <memory>

#include "Entity.h"

struct SignatureHash {
    std::size_t operator()(const Signature& _signature) const noexcept {
        return std::hash<std::string>()(_signature.to_string());
    }
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

#include "RegistryInternal.inl"