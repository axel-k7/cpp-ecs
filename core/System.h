#pragma once

class Registry;

class System {
public:
    virtual ~System() = default;
    virtual void update(const float& _delta_time) = 0;
    
    void setRegistry(Registry* _registry) { registry = _registry; };

protected:
    Registry* registry = nullptr;
};