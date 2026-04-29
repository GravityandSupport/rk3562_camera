#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <memory>

#include "event_device.h"
#include "json.hpp"

struct Subscriber {
    std::weak_ptr<EventDevice> dev;
};

class EventBus {
public:
    static EventBus& instance();

    void subscribe(const std::string& topic, std::shared_ptr<EventDevice> dev);
    void unsubscribe(const std::string& topic, std::shared_ptr<EventDevice> dev);
    void publish(const std::string& topic, const std::string& payload);
private:
    std::unordered_map<std::string, std::vector<Subscriber>> subscribers_;
    std::mutex mutex_;
};


