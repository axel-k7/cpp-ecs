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
    virtual void creationCallback(Entity _entity) = 0;
    virtual void destructionCallback(Entity _entity) = 0;
    //callbacks for component addition and destruction?
};