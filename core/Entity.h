#pragma once

#include <cstdint>

struct Entity {
    uint32_t id;
    uint32_t version;

    bool operator==(const Entity& _other) const {
        return id == _other.id && version == _other.version;
    }

    bool operator!=(const Entity& _other) const {
        return !(*this == _other);
    }

    operator uint32_t() const {
        return id;
    }

    static auto Null() -> Entity {
        return { UINT32_MAX, UINT32_MAX };
    }

};
