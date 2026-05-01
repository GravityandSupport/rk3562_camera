#pragma once

#include <string>
#include <mutex>
#include <stdexcept>
#include "json.hpp"
#include <iostream>

class JsonWrapper{
public:
    template<typename T>
    bool get(const std::string& key1, T& value);

    template<typename T>
    void import(const std::string& key1, const T& value);

    void import(const std::string& key1, const nlohmann::json& value);

    std::string dump(const int indent = -1);

    JsonWrapper() = default;
    JsonWrapper(const std::string& json_str);
    virtual ~JsonWrapper() = default;
private:
    nlohmann::json json;

    std::mutex mutex_;
};

#include "JsonWrapper.inl"

/*  
    =======================================================
    ======================= 使用示例 =======================
    =======================================================

    JsonWrapper js;

    // ===== 写入多层结构 =====
    js.import("device.camera.width", 1920);
    js.import("device.camera.height", 1080);
    js.import("device.camera.format", "NV12");

    js.import("device.audio.sample_rate", 48000);
    js.import("device.audio.channels", 2);
    js.import("device.audio", 123);

    js.import("system.version", "1.0.0");
    js.import("system.accout", {{"id", 1}, {"name", "Admin"}});

    js.import("video.number", {2, 3, 4, 5, 11});
    js.import("video.filename", {"2024.jpg", "2025.jpg", "2026.jpg"});

    std::cout << "生成的 JSON:\n" << js.dump(4) << std::endl;

    // ===== 读取 =====
    int width = 0;
    int height = 0;
    std::string format;

    if (js.get("device.camera.width", width)) {
        std::cout << "width = " << width << std::endl;
    }

    if (js.get("device.camera.height", height)) {
        std::cout << "height = " << height << std::endl;
    }

    if (js.get("device.camera.format", format)) {
        std::cout << "format = " << format << std::endl;
    }

    // ===== 错误测试 =====
    int fps = 0;
    if (!js.get("device.camera.fps", fps)) {
        std::cout << "fps 不存在" << std::endl;
    }

*/
