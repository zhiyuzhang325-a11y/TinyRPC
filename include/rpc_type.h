#pragma once

#include <functional>
#include <string>
#include <unordered_map>

using HandlerMap = std::unordered_map<std::string, std::unordered_map<std::string, std::function<std::string(const std::string &)>>>;