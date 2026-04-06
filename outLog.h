#pragma once

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdarg>

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LEVEL_DEBUG
#endif

// 日志等级定义
#define LOG_LEVEL_DISABLE 0
#define LOG_LEVEL_ERROR   1
#define LOG_LEVEL_WARN    2
#define LOG_LEVEL_INFO    3
#define LOG_LEVEL_DEBUG   4

// 去除首尾空白字符
static inline std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

// 以逗号分割参数名字符串，并trim
static inline std::vector<std::string> splitArgs(const std::string& argsStr) {
    std::vector<std::string> result;
    std::stringstream ss(argsStr);
    std::string token;
    while (std::getline(ss, token, ',')) {
        result.push_back(trim(token));
    }
    return result;
}

// 任意类型转字符串
template<typename T>
static inline std::string toString(const T& value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

// 格式化带名字的参数列表（支持字符串字面量不显示名字）
template<typename... Args>
std::string formatArgs(const std::string& names, Args&&... args) {
    std::ostringstream oss;
    auto argNames = splitArgs(names);
    std::vector<std::string> argValues{toString(std::forward<Args>(args))...};

    size_t count = std::min(argNames.size(), argValues.size());
    for (size_t i = 0; i < count; ++i) {
        if (i > 0) oss << ", ";

        const auto& name = argNames[i];
        // 如果是字符串字面量（"..."），只输出值
        if (!name.empty() && name.front() == '"' && name.back() == '"') {
            oss << argValues[i];
        } else {
            oss << name << " = " << argValues[i];
        }
    }
    return oss.str();
}

// 尝试从 __PRETTY_FUNCTION__ 中提取更友好的函数名（类名::函数名）
static inline std::string extractNiceFuncName(const char* pretty) {
    std::string s = pretty;

    // 常见 gcc/clang 输出形式：
    //   void MyClass::method(int, std::string) const
    //   int Namespace::Class::func()
    //   static void Class::staticMethod()
    //	 void func()

    // 找到最后一个 :: 的位置
    size_t last_scope = s.rfind("::");
    if (last_scope == std::string::npos) {
        // 普通全局函数或lambda，没有类名
        // 提取纯函数名：先截取到'('之前，再取最后一个空格之后的部分
        size_t paren = s.find('(');
        std::string prefix = (paren != std::string::npos) ? s.substr(0, paren) : s;

        // 查找最后一个空格（从右向左搜索）
                size_t last_space = prefix.rfind(' ');

        // 如果找到空格且后面还有字符，返回空格后的内容；否则返回整个前缀
                if (last_space != std::string::npos && last_space + 1 < prefix.length()) {
                    return prefix.substr(last_space + 1);
                }
                return prefix;  // 无空格或空格在末尾时返回原字符串
    }

    // :: 后面的部分 → 函数名（可能带 const/volatile 等）
    std::string func_part = s.substr(last_scope + 2);
    size_t paren = func_part.find('(');
    if (paren != std::string::npos) {
        func_part = func_part.substr(0, paren);
    }

    // :: 前面的部分 → 可能包含返回类型、类名、命名空间
    std::string before = s.substr(0, last_scope);

    // 从后往前找最后一个单词（通常是类名）
    size_t last_space = before.find_last_of(" \t");
    std::string class_or_ns;
    if (last_space != std::string::npos) {
        class_or_ns = before.substr(last_space + 1);
    } else {
        class_or_ns = before;
    }

    // 去掉可能的 const/volatile 等修饰（简单处理）
    size_t const_pos = func_part.find(" const");
    if (const_pos != std::string::npos) {
        func_part.erase(const_pos);
    }

    if (class_or_ns.empty() || class_or_ns == "static") {
        return func_part;
    }

    return class_or_ns + "::" + func_part;
}

class Logger {
public:
    // 带参数名展开的日志（推荐使用）
    template<typename... Args>
    static void log(const char* level,
                    const char* file,
                    const char* pretty_func,
                    int line,
                    const std::string& message,
                    const std::string& arg_names,
                    Args&&... args)
    {
        std::ostringstream oss;

        // 缩短文件名（只保留最后一部分）
        std::string filename = file;
        size_t last_slash = filename.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            filename = filename.substr(last_slash + 1);
        }

        std::string func_name = extractNiceFuncName(pretty_func);

        oss << "[" << level << "] "
            << "[" << filename << ":" << line << "] "
            << "[" << func_name << "] "
            << message;

        if (sizeof...(args) > 0) {
            oss << " : " << formatArgs(arg_names, std::forward<Args>(args)...);
        }

        std::cout << oss.str() << std::endl;
    }

    // 类似 printf 风格（不展开参数名）
    static void logc(const char* level,
                     const char* file,
                     const char* func,
                     int line,
                     const std::string& message,
                     const char* fmt, ...)
    {
        std::string filename = file;
        size_t last_slash = filename.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            filename = filename.substr(last_slash + 1);
        }

        printf("[%s] [%s:%d] [%s] %s : ",
               level, filename.c_str(), line, func, message.c_str());

        va_list ap;
        va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);

        printf("\n");
    }
};

// 宏定义 - 根据 LOG_LEVEL 控制是否编译
#if LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOG_ERROR(msg, ...) \
    Logger::log("ERROR", __FILE__, __PRETTY_FUNCTION__, __LINE__, msg, #__VA_ARGS__, ##__VA_ARGS__)
#define LOG_ERRORC(msg, fmt, ...) \
    Logger::logc("ERROR", __FILE__, __FUNCTION__, __LINE__, msg, fmt, ##__VA_ARGS__)
#else
#define LOG_ERROR(msg, ...)
#define LOG_ERRORC(msg, fmt, ...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
#define LOG_WARN(msg, ...) \
    Logger::log("WARN", __FILE__, __PRETTY_FUNCTION__, __LINE__, msg, #__VA_ARGS__, ##__VA_ARGS__)
#define LOG_WARNC(msg, fmt, ...) \
    Logger::logc("WARN", __FILE__, __FUNCTION__, __LINE__, msg, fmt, ##__VA_ARGS__)
#else
#define LOG_WARN(msg, ...)
#define LOG_WARNC(msg, fmt, ...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
#define LOG_INFO(msg, ...) \
    Logger::log("INFO", __FILE__, __PRETTY_FUNCTION__, __LINE__, msg, #__VA_ARGS__, ##__VA_ARGS__)
#define LOG_INFOC(msg, fmt, ...) \
    Logger::logc("INFO", __FILE__, __FUNCTION__, __LINE__, msg, fmt, ##__VA_ARGS__)
#else
#define LOG_INFO(msg, ...)
#define LOG_INFOC(msg, fmt, ...)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOG_DEBUG(msg, ...) \
    Logger::log("DEBUG", __FILE__, __PRETTY_FUNCTION__, __LINE__, msg, #__VA_ARGS__, ##__VA_ARGS__)
#define LOG_DEBUGC(msg, fmt, ...) \
    Logger::logc("DEBUG", __FILE__, __FUNCTION__, __LINE__, msg, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(msg, ...)
#define LOG_DEBUGC(msg, fmt, ...)
#endif
