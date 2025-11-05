#pragma once

#include "Registry.h"

//UGLY BORING TEMPLATING STUFF GOES IN HERE :)

//STRUCT METHODS------------------------------------------------------------------------

template<typename T>
void Registry::ComponentArray<T>::add(const T& _component) {
    data.push_back(_component);
}

template<typename T>
void Registry::ComponentArray<T>::remove(size_t _index) {
    data[_index] = data.back();
    data.pop_back();
}

template<typename T>
T& Registry::ComponentArray<T>::get(size_t _index) {
    return data[_index];
}

template<typename T>
size_t Registry::ComponentArray<T>::size() const {
    return data.size();
}

template<typename T>
void Registry::ComponentArray<T>::swapElements(size_t _a, size_t _b) {
    std::swap(data[_a], data[_b]);
}

template<typename T>
void Registry::ComponentArray<T>::moveElement(size_t _index, sComponentArray* _to) {
    auto* other = static_cast<ComponentArray<T>*>(_to);
    other->data.push_back(std::move(data[_index]));
}

template<typename T>
void Registry::ComponentArray<T>::removeLast() {
    data.pop_back();
}

template<typename T>
void Registry::ComponentArray<T>::addFrom(void* _component) {
    add(*static_cast<T*>(_component));
}


template<typename T>
auto Registry::ComponentArray<T>::getRaw(size_t _index) -> void* {
    return &data[_index];
}

template<typename T>
Registry::sComponentArray* Registry::ComponentArray<T>::cloneEmpty() const {
    return new ComponentArray<T>();
}

//HELPER DEFINTIONS--------------------------------------------------------------------


template<typename T>
void Registry::addComponentToArray(Archetype* _archetype, const T& _component) {
    auto* array = _archetype->getArray();

    if (!array) {
        array = new ComponentArray<T>();
        _archetype->component_arrays[getComponentTypeID<T>()] = array;
    }

    array->add(_component);
};

template<typename T>
void Registry::removeComponentAt(Archetype* _archetype, size_t _index) {
    auto array = _archetype->getArray();

    if (array)
        array->remove(_index);
};


template<typename... Components>
void Registry::removeComponentsAt(Archetype* _archetype, size_t _index) {
    (removeComponentAt<Components>(_archetype, _index), ...);
};


template<typename T>
void Registry::deleteComponentArray(ComponentArray<T>* _array) {
    delete _array;
};