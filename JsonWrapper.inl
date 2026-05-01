#pragma once

template<typename T>
bool JsonWrapper::get(const std::string& path, T& value) {
    try {
        const nlohmann::json* current = &json;

        size_t start = 0;
        size_t end = 0;

        while ((end = path.find('.', start)) != std::string::npos) {
            std::string key = path.substr(start, end - start);

            if (!current->contains(key)) {
                return false;
            }

            current = &((*current)[key]);
            start = end + 1;
        }

        // 最后一个 key
        std::string last_key = path.substr(start);
        if (!current->contains(last_key)) {
            return false;
        }

        value = current->at(last_key).get<T>();
        return true;

    } catch (const std::exception& e) {
        std::cerr << "Get error (" << path << "): " << e.what() << std::endl;
        return false;
    }
}

template<typename T>
void JsonWrapper::import(const std::string& path, const T& value) {
    nlohmann::json* current = &json;

    size_t start = 0;
    size_t end = 0;

    while ((end = path.find('.', start)) != std::string::npos) {
        std::string key = path.substr(start, end - start);

        // 如果不存在就创建对象
        if (!current->contains(key) || !(*current)[key].is_object()) {
            (*current)[key] = nlohmann::json::object();
        }

        current = &((*current)[key]);
        start = end + 1;
    }

    // 最后一个 key
    std::string last_key = path.substr(start);
    (*current)[last_key] = value;
}
