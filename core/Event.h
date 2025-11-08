#pragma once

#include <functional>
#include <vector>
#include <utility>

template<typename... Args>
struct sEvent {
    std::vector<std::function<void(Args...)>> listeners;
    
    void subscribe(std::function<void(Args...)> _callback) {
        listeners.push_back(std::move(_callback));
    }

    void trigger(Args... _args) {
        //should optimize to virtual methods or function pointers
        for (auto& callback : listeners)
            callback(_args...);
    }

    void clear() {
        listeners.clear();
    }
};