#pragma once
#include <memory>
#include <string>

struct EventMsg { 
    std::string topic; 
    std::string payload; 
};

class EventDevice : public std::enable_shared_from_this<EventDevice> {
public:
    virtual ~EventDevice() {}

    // 统一回调入口（子类重写）
    virtual void onMessage(const EventMsg& msg) = 0;

    // 订阅接口（封装EventBus）
    void subscribe(const std::string& topic);
    void unsubscribe(const std::string& topic);

    // 发布接口
    static void publish(const std::string& topic, const std::string& payload);
};