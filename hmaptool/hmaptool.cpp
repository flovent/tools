#include "hmaptool.h"
#include <bitset>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

size_t hmap_hash(const std::string &str) {
  int hash = 0;
  for (char c : str) {
    // Convert to lowercase
    char lower_c = std::tolower(c);
    // Multiply by 13 and add to the hash
    hash += static_cast<int>(lower_c) * 13;
  }
  return hash;
}

size_t next_power_of_two(size_t n) {
  if (n < 0) {
    throw std::invalid_argument("sda");
  }
  if (n == 0)
    return 1;
  return std::pow(2, std::ceil(std::log2(n)));
}

std::string stringToBinary(const std::string &str) {
  std::string binary;
  for (char c : str) {
    binary += std::bitset<8>(c).to_string();
  }
  return binary;
}

bool write_header_map(
    const std::unordered_map<std::string, std::string> &mappings,
    const std::string &output_path) {
  size_t num_buckets = next_power_of_two(mappings.size() * 3);
  std::vector<std::tuple<size_t, size_t, size_t>> table(num_buckets, {0, 0, 0});
  size_t max_value_len = 0;
  std::string strtable;
  strtable.push_back('\0');
  for (auto &[key, value] : mappings) {
    std::string key_str = key;
    std::string value_str = value;
    max_value_len = std::max(max_value_len, value_str.size());

    size_t key_idx = strtable.size();
    strtable += key_str + '\0';

    std::string prefix =
        std::filesystem::path(value_str).parent_path().string() + '/';
    std::string suffix = std::filesystem::path(value_str).filename().string();
    size_t prefix_idx = strtable.size();
    strtable += prefix + '\0';
    size_t suffix_idx = strtable.size();
    strtable += suffix + '\0';

    size_t hash = hmap_hash(key_str);
    for (size_t i = 0; i < num_buckets; ++i) {
      size_t idx = (hash + i) % num_buckets;
      if (std::get<0>(table[idx]) == 0) {
        table[idx] = {key_idx, prefix_idx, suffix_idx};
        break;
      }
    }
  }
  std::string magic = "pamh";
  size_t magic_size = 4;
  size_t header_size = sizeof(uint16_t) * 2 + sizeof(uint32_t) * 4; // "<HHIIII"
  size_t bucket_size = sizeof(uint32_t) * 3;                        // "<III"
  size_t strtable_offset = magic_size + header_size + num_buckets * bucket_size;
  std::vector<uint8_t> output_data;
  output_data.resize(strtable_offset + strtable.size());

  std::memcpy(output_data.data(), magic.data(), magic_size);

  uint16_t version = 1;
  uint16_t reserved = 0;
  uint32_t strtable_offset_u32 = static_cast<uint32_t>(strtable_offset);
  uint32_t num_entries = static_cast<uint32_t>(mappings.size());
  uint32_t num_buckets_u32 = static_cast<uint32_t>(num_buckets);
  uint32_t max_value_len_u32 = static_cast<uint32_t>(max_value_len);

  std::memcpy(output_data.data() + magic_size, &version, sizeof(version));
  std::memcpy(output_data.data() + magic_size + sizeof(version), &reserved,
              sizeof(reserved));

  std::memcpy(output_data.data() + magic_size + sizeof(version) +
                  sizeof(reserved),
              &strtable_offset_u32, sizeof(strtable_offset_u32));
  std::memcpy(output_data.data() + magic_size + sizeof(version) +
                  sizeof(reserved) + sizeof(strtable_offset_u32),
              &num_entries, sizeof(num_entries));
  std::memcpy(output_data.data() + magic_size + sizeof(version) +
                  sizeof(reserved) + sizeof(strtable_offset_u32) +
                  sizeof(num_entries),
              &num_buckets_u32, sizeof(num_buckets_u32));
  std::memcpy(output_data.data() + magic_size + sizeof(version) +
                  sizeof(reserved) + sizeof(strtable_offset_u32) +
                  sizeof(num_entries) + sizeof(num_buckets_u32),
              &max_value_len_u32, sizeof(max_value_len_u32));

  for (size_t i = 0; i < num_buckets; ++i) {
    uint32_t key_idx = static_cast<uint32_t>(std::get<0>(table[i]));
    uint32_t prefix_idx = static_cast<uint32_t>(std::get<1>(table[i]));
    uint32_t suffix_idx = static_cast<uint32_t>(std::get<2>(table[i]));

    std::memcpy(output_data.data() + magic_size + header_size + i * bucket_size,
                &key_idx, sizeof(key_idx));
    std::memcpy(output_data.data() + magic_size + header_size +
                    i * bucket_size + sizeof(key_idx),
                &prefix_idx, sizeof(prefix_idx));
    std::memcpy(output_data.data() + magic_size + header_size +
                    i * bucket_size + sizeof(key_idx) + sizeof(prefix_idx),
                &suffix_idx, sizeof(suffix_idx));
  }

  std::memcpy(output_data.data() + strtable_offset, strtable.data(),
              strtable.size());

  std::ofstream output_file(output_path, std::ios::binary);
  if (!output_file.is_open()) {
    return false;
  }

  output_file.write(reinterpret_cast<const char *>(output_data.data()),
                    output_data.size());
  output_file.close();

  return true;
}