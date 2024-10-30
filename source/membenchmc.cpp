#include <membenchmc/membenchmc.h>

namespace membenchmc {

  std::string trim(const std::string& str) {
    auto start = std::find_if(str.cbegin(), str.cend(),
                              [](unsigned char ch) { return !std::isspace(ch); });
    auto end = std::find_if(str.crbegin(), str.crend(), [](unsigned char ch) {
                 return !std::isspace(ch);
               }).base();
    return std::string(start, end);
  }

  nlohmann::json getCPUInfo() {
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
    nlohmann::json cpu_info;
    std::istringstream iss(result);
    std::string line;
    while (std::getline(iss, line)) {
      std::istringstream line_stream(line);
      std::string key, value;
      if (std::getline(line_stream, key, ':') && std::getline(line_stream, value)) {
        cpu_info[key] = trim(value);
      }
    }
    return cpu_info;
  }
  std::string getHostName() {
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    return std::string(hostname);
  }
  std::string getUserName() { return std::string(getlogin()); }

  nlohmann::json gatherMetadata() {
    auto start_time = time(nullptr);
    nlohmann::json metadata;
    metadata["start_time"] = trim(ctime(&start_time));
    metadata["host_name"] = getHostName();
    metadata["user_name"] = getUserName();
    metadata["cpu_info"] = getCPUInfo();
    return metadata;
  }
}  // namespace membenchmc
