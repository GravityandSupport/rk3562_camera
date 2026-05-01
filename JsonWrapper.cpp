#include "JsonWrapper.h"

JsonWrapper::JsonWrapper(const std::string& json_str) {
    try {
        if (!json_str.empty()) {
            json = nlohmann::json::parse(json_str);
        }
    } catch (const nlohmann::json::parse_error& e) {
        // 可以在这里处理解析失败的情况
        std::cerr << "JSON Parse error: " << e.what() << std::endl;
    }
}

std::string JsonWrapper::dump(const int indent) {
    // dump(-1) 表示压缩输出，如果需要美化输出可以传参数（如 4）
    return json.dump(indent);
}

void JsonWrapper::import(const std::string& path, const nlohmann::json& value) {
    nlohmann::json* current = &json;

    size_t start = 0;
    size_t end = 0;

    while ((end = path.find('.', start)) != std::string::npos) {
        std::string key = path.substr(start, end - start);

        // 如果不存在或不是 object，则创建
        if (!current->contains(key) || !(*current)[key].is_object()) {
            (*current)[key] = nlohmann::json::object();
        }

        current = &((*current)[key]);
        start = end + 1;
    }

    // 最后一层
    std::string last_key = path.substr(start);
    (*current)[last_key] = value;
}