#include <membenchmc/membenchmc.h>
#include <membenchmc/version.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cxxopts.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

auto trim(const std::string& str) {
  auto start
      = std::find_if(str.cbegin(), str.cend(), [](unsigned char ch) { return !std::isspace(ch); });
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

auto main(/*int argc, char** argv*/) -> int {
  std::cout << getCPUInfo() << "\n";
  //  const std::unordered_map<std::string, membenchmc::LanguageCode> languages{
  //      {"en", membenchmc::LanguageCode::EN},
  //      {"de", membenchmc::LanguageCode::DE},
  //      {"es", membenchmc::LanguageCode::ES},
  //      {"fr", membenchmc::LanguageCode::FR},
  //  };
  //
  //  cxxopts::Options options(*argv, "A program to welcome the world!");
  //
  //  std::string language;
  //  std::string name;
  //
  //  // clang-format off
  //  options.add_options()
  //    ("h,help", "Show help")
  //    ("v,version", "Print the current version number")
  //    ("n,name", "Name to greet", cxxopts::value(name)->default_value("World"))
  //    ("l,lang", "Language code to use", cxxopts::value(language)->default_value("en"))
  //  ;
  //  // clang-format on
  //
  //  auto result = options.parse(argc, argv);
  //
  //  if (result["help"].as<bool>()) {
  //    std::cout << options.help() << std::endl;
  //    return 0;
  //  }
  //
  //  if (result["version"].as<bool>()) {
  //    std::cout << "MemBenchMC, version " << MEMBENCHMC_VERSION << std::endl;
  //    return 0;
  //  }
  //
  //  auto langIt = languages.find(language);
  //  if (langIt == languages.end()) {
  //    std::cerr << "unknown language code: " << language << std::endl;
  //    return 1;
  //  }
  //
  //  membenchmc::MemBenchMC membenchmc(name);
  //  std::cout << membenchmc.greet(langIt->second) << std::endl;

  return 0;
}
