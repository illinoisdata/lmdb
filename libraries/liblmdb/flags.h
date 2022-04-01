// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <iostream>
#include <map>
#include <vector>
#include <sstream>
#include <string>

std::map<std::string, std::string> parse_flags(int argc, char** argv) {
  std::map<std::string, std::string> flags;
  for (int i = 1; i < argc; i++) {
    std::string arg(argv[i]);
    size_t equals = arg.find("=");
    size_t dash = arg.find("--");
    if (dash != 0) {
      std::cout << "Bad flag '" << argv[i] << "'. Expected --key=value"
                << std::endl;
      continue;
    }
    std::string key = arg.substr(2, equals - 2);
    std::string val;
    if (equals == std::string::npos) {
      val = "";
      std::cout << "found flag " << key << std::endl;
    } else {
      val = arg.substr(equals + 1);
      std::cout << "found flag " << key << " = " << val << std::endl;
    }
    flags[key] = val;
  }
  return flags;
}

std::string get_with_default(const std::map<std::string, std::string>& m,
                             const std::string& key,
                             const std::string& defval) {
  auto it = m.find(key);
  if (it == m.end()) {
    return defval;
  }
  return it->second;
}

std::string get_required(const std::map<std::string, std::string>& m,
                         const std::string& key) {
  auto it = m.find(key);
  if (it == m.end()) {
    std::cout << "Required flag --" << key << " was not found" << std::endl;
  }
  return it->second;
}

bool get_boolean_flag(const std::map<std::string, std::string>& m,
                      const std::string& key) {
  return m.find(key) != m.end();
}

std::vector<std::string> get_comma_separated(
    std::map<std::string, std::string>& m, const std::string& key) {
  std::vector<std::string> vals;
  auto it = m.find(key);
  if (it == m.end()) {
    return vals;
  }
  std::istringstream s(m[key]);
  std::string val;
  while (std::getline(s, val, ',')) {
    vals.push_back(val);
    std::cout << "parsed csv val " << val << std::endl;
  }
  return vals;
}



void encode64(uint64_t n, char* bytes) {
  bytes[7] = (n >> 56) & 0xFF;
  bytes[6] = (n >> 48) & 0xFF;
  bytes[5] = (n >> 40) & 0xFF;
  bytes[4] = (n >> 32) & 0xFF;;
  bytes[3] = (n >> 24) & 0xFF;
  bytes[2] = (n >> 16) & 0xFF;
  bytes[1] = (n >> 8) & 0xFF;
  bytes[0] = n & 0xFF;
}

void encode32(uint32_t n, char* bytes) {
  bytes[3] = (n >> 24) & 0xFF;
  bytes[2] = (n >> 16) & 0xFF;
  bytes[1] = (n >> 8) & 0xFF;
  bytes[0] = n & 0xFF;
}

uint64_t decode64(unsigned char* bytes) {
  return (((uint64_t) bytes[7]) << 56) +
    (((uint64_t) bytes[6]) << 48) +
    (((uint64_t) bytes[5]) << 40) +
    (((uint64_t) bytes[4]) << 32) +
    (((uint64_t) bytes[3]) << 24) +
    (((uint64_t) bytes[2]) << 16) +
    (((uint64_t) bytes[1]) << 8)  +
    bytes[0];
}

uint32_t decode32(unsigned char* bytes) {
  return (((uint32_t) bytes[3]) << 24) +
    (((uint32_t) bytes[2]) << 16) +
    (((uint32_t) bytes[1]) << 8)  +
    bytes[0];
}