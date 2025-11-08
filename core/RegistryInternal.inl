#include "RegistryInternal.h"

template<typename T>
auto Archetype::getArray() -> ComponentArray<T>* {
    auto it = component_arrays.find(Registry::getComponentTypeID<T>());

    if (it != component_arrays.end())
        return static_cast<ComponentArray<T>*>(it->second);

    return nullptr;
}


template<typename T>
void ComponentArray<T>::add(const T& _component) {
    data.push_back(_component);
}

template<typename T>
void ComponentArray<T>::remove(size_t _index) {
    data[_index] = data.back();
    data.pop_back();
}

template<typename T>
auto ComponentArray<T>::get(size_t _index) -> T& {
    return data[_index];
}

template<typename T>
size_t ComponentArray<T>::size() const {
    return data.size();
}

template<typename T>
void ComponentArray<T>::swapElements(size_t _a, size_t _b) {
    std::swap(data[_a], data[_b]);
}

template<typename T>
void ComponentArray<T>::moveElement(size_t _index, sComponentArray* _to) {
    auto* other = static_cast<ComponentArray<T>*>(_to);
    other->data.push_back(std::move(data[_index]));

    size_t last = data.size() -1;
    if (_index != last)
        data[_index] = std::move(data[last]);
    
    data.pop_back();
}

template<typename T>
void ComponentArray<T>::removeLast() {
    data.pop_back();
}

template<typename T>
void ComponentArray<T>::addFrom(void* _component) {
    add(*static_cast<T*>(_component));
}


template<typename T>
auto ComponentArray<T>::getRaw(size_t _index) -> void* {
    return &data[_index];
}

template<typename T>
auto ComponentArray<T>::cloneEmpty() const -> sComponentArray* {
    return new ComponentArray<T>();
}