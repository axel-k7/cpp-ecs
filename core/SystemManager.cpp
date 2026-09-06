#include "SystemManager.h"

void SystemManager::update(const float& _delta_time) {
    for (auto const& system : system_order) {
        system->update(_delta_time);
    }

    buffer->update();
}