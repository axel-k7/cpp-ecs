#pragma once

class Registry;

class System {
public:
    virtual ~System() = default;
    virtual void update(const float& _delta_time) = 0;
    virtual void setupListeners(Registry* _registry) = 0;
    
    void setRegistry(Registry* _registry) { registry = _registry; };

protected:
    Registry* registry = nullptr;

private:
    //Entity Callbacks
    virtual void creationCallback(Entity _entity) = 0;
    virtual void destructionCallback(Entity _entity) = 0;
    //Component Callbacks
    virtual void additionCallback(Entity _entity, uint32_t _component_type) = 0;
    virtual void removalCallback(Entity _entity, uint32_t _component_type) = 0;

};