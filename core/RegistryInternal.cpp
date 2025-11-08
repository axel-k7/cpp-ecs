#include "RegistryInternal.h"

auto Archetype::ensureComponentArray(uint32_t _type, sComponentArray* _source_array) -> sComponentArray* {
    auto it = component_arrays.find(_type);
    if (it == component_arrays.end()) {
        std::unique_ptr<sComponentArray> new_array(_source_array->cloneEmpty());
        auto [it, _] = component_arrays.emplace(_type, std::move(new_array));

        return it->second.get();
    }

    return it->second.get();
};


void Archetype::transferComponents(const Archetype* _source, size_t _source_index) {
    for (auto& [type, source_array] : _source->component_arrays) {
        if (signature.test(type)) {
            sComponentArray* target = ensureComponentArray(type, source_array.get());
            source_array->moveElement(_source_index, target);
        }
        
    }
};