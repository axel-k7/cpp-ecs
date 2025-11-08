#pragma once

#include <functional>
#include <vector>

template<typename... Args>
struct sEvent {
    std::vector<std::function<void(Args...)>> listeners;
    
    void subcribe(std::function<void(Args...)> _callback) {
        listeners.push_back(std::forward<std::function<void(Args...)>>(_callback));
    }

    void trigger(Args... _args) {
        for (auto& callback : listeners)
            callback(_args...);
    }

    void clear() {
        listener.clear();
    }
};