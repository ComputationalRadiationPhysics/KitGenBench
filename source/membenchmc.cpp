#include <membenchmc/membenchmc.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <string>

namespace membenchmc {
  /**
   * @brief Trims leading and trailing whitespace from a string.
   *
   * @param str The string to trim.
   * @return std::string The trimmed string.
   */
  std::string trim(const std::string& str) {
    auto start = std::find_if(str.cbegin(), str.cend(),
                              [](unsigned char ch) { return !std::isspace(ch); });
    auto end = std::find_if(str.crbegin(), str.crend(), [](unsigned char ch) {
                 return !std::isspace(ch);
               }).base();
    return std::string(start, end);
  }

  /**
   * @brief Calls `lscpu` and returns its stdout as string.
   *
   * @return std::string A string containing the output.
   */
  std::string callLscpu() {
    FILE* pipe = popen("lscpu", "r");
    if (!pipe) {
      return nlohmann::json::object({{"error", "CPU information not available"}});
    }
    char buffer[128];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      result += buffer;
    }
    pclose(pipe);
    return result;
  }

  /**
   * @brief Retrieves CPU information using the `lscpu` command.
   *
   * @return nlohmann::json A JSON object containing CPU information.
   */
  nlohmann::json getCPUInfo() {
    std::string result = callLscpu();
    std::cout << 1 << std::endl;
    nlohmann::json cpu_info;
    std::istringstream iss(result);
    std::string line;
    while (std::getline(iss, line)) {
      std::cout << 2 << ": " << line << std::endl;
      std::istringstream line_stream(line);
      std::string key, value;
      if (std::getline(line_stream, key, ':') && std::getline(line_stream, value)) {
        cpu_info[key] = trim(value);
      }
    }
    return cpu_info;
  }

  /**
   * @brief Retrieves the hostname of the system.
   *
   * @return std::string The hostname of the system.
   */
  std::string getHostName() {
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    return std::string(hostname);
  }

  /**
   * @brief Retrieves the username of the current user.
   *
   * @return std::string The username of the current user.
   */
  std::string getUserName() { return std::string(getlogin()); }

  nlohmann::json gatherMetadata() {
    auto start_time = time(nullptr);
    nlohmann::json metadata{};
    metadata["start_time"] = trim(ctime(&start_time));
    std::cout << 3 << std::endl;
    metadata["host_name"] = getHostName();
    std::cout << 4 << std::endl;
    metadata["user_name"] = getUserName();
    std::cout << 5 << std::endl;
    metadata["cpu_info"] = getCPUInfo();
    std::cout << 6 << std::endl;
    return metadata;
  }
}  // namespace membenchmc
