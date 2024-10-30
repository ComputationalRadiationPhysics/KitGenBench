#pragma once
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

namespace membenchmc {
  /**
   * @brief Retrieves CPU information using the `lscpu` command.
   *
   * @return nlohmann::json A JSON object containing CPU information.
   */
  nlohmann::json getCPUInfo();

  /**
   * @brief Retrieves the hostname of the system.
   *
   * @return std::string The hostname of the system.
   */
  std::string getHostName();

  /**
   * @brief Retrieves the username of the current user.
   *
   * @return std::string The username of the current user.
   */
  std::string getUserName();

  /**
   * @brief Gathers metadata about the system, including start time, hostname, username, and CPU
   * information.
   *
   * @return nlohmann::json A JSON object containing the gathered metadata.
   */
  nlohmann::json gatherMetadata();
}  // namespace membenchmc
