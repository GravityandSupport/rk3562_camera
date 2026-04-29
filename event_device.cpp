#include "event_device.h"
#include "EventBus.h"

void EventDevice::subscribe(const std::string& topic) {
    EventBus::instance().subscribe(topic, shared_from_this());
}

void EventDevice::publish(const std::string& topic, const std::string& payload){
    EventBus::instance().publish(topic, payload);
}

void EventDevice::unsubscribe(const std::string& topic) {
    EventBus::instance().unsubscribe(topic, shared_from_this());
}