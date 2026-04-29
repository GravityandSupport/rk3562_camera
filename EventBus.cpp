#include "EventBus.h"
#include <iostream>

EventBus& EventBus::instance(){
    static EventBus bus;
    return bus;
}

void EventBus::subscribe(const std::string& topic, std::shared_ptr<EventDevice> dev) {
    std::lock_guard<std::mutex> lock(mutex_);
    subscribers_[topic].push_back({dev});
}
void EventBus::unsubscribe(const std::string& topic, std::shared_ptr<EventDevice> dev) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!subscribers_.count(topic)) return;

    auto& vec = subscribers_[topic];

    for (auto it = vec.begin(); it != vec.end(); ) {
        auto sp = it->dev.lock();

        if (!sp || sp == dev) {
            it = vec.erase(it);
        } else {
            ++it;
        }
    }
}

void EventBus::publish(const std::string& topic, const std::string& payload) {
    std::lock_guard<std::mutex> lock(mutex_);

    EventMsg msg{topic, payload};

    if (!subscribers_.count(topic)) return;

    auto& vec = subscribers_[topic];

    for (auto it = vec.begin(); it != vec.end(); ) {
        auto sp = it->dev.lock();

        if (sp) {
            sp->onMessage(msg);
            ++it;
        } else {
            // 自动清理已销毁设备
            it = vec.erase(it);
        }
    }
}
