#include <membenchmc/membenchmc.h>
#include <membenchmc/version.h>

#include <cstdlib>
#include <cxxopts.hpp>

auto defineCmdlineInterface() {
  cxxopts::Options options("my_program", "A program to process a configuration file");
  // clang-format off
  options.add_options()
    ("c,config", "Path to the configuration file", cxxopts::value<std::string>())
    ("h,help", "Print help")
    ( "v,version", "Print the current version number");
  // clang-format on
  return options;
}

auto extractConfigFilePath(int argc, char* argv[], auto& options) {
  auto result = options.parse(argc, argv);
  // Check if help was requested
  if (result.count("help")) {
    std::cout << options.help() << std::endl;
    exit(0);
  }
  if (result["version"].template as<bool>()) {
    std::cout << "MemBenchMC, version " << MEMBENCHMC_VERSION << std::endl;
    exit(0);
  }
  // Check if the config option was provided
  if (!result.count("config")) {
    std::cerr << "Error: No configuration file specified. Use --config <path_to_config_file>."
              << std::endl;
    exit(1);
  }
  // Get the path to the configuration file
  return result["config"].template as<std::string>();
}

auto main(int argc, char* argv[]) -> int {
  auto metadata = membenchmc::gatherMetadata();
  auto options = defineCmdlineInterface();
  try {
    auto configFilePath = extractConfigFilePath(argc, argv, options);
  } catch (const cxxopts::OptionException& e) {
    std::cerr << "Error parsing options: " << e.what() << std::endl;
    exit(1);
  }
  return EXIT_SUCCESS;
}
