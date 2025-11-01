#pragma once
#include <cstdint>

struct Entity {
    uint32_t id;
    uint32_t version;
};

inline bool operator==(const Entity& a, const Entity&b){
    return a.id == b.id && a.version == b.version;
}