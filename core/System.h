#pragma once

#include "Registry.h"
#include "EntityCommandBuffer.h"

#include <iostream>

class System {
public:
    System(std::shared_ptr<Registry> _registry, std::shared_ptr<EntityCommandBuffer> _buffer)
        : registry(_registry)
        , buffer(_buffer)
    {}

    virtual ~System() = default;
    virtual void update(const float& _delta_time) = 0;

    std::shared_ptr<Registry> registry;
    std::shared_ptr<EntityCommandBuffer> buffer;
};