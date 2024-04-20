#pragma once
#include <string>
#include <unordered_map>

bool write_header_map(
    const std::unordered_map<std::string, std::string> &mappings,
    const std::string &output_path);