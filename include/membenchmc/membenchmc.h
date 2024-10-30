#pragma once
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

namespace membenchmc {
  nlohmann::json getCPUInfo();
  std::string getHostName();
  std::string getUserName();
  nlohmann::json gatherMetadata();
}  // namespace membenchmc
