#include <membenchmc/membenchmc.h>
#include <membenchmc/version.h>

#include <cstdlib>
#include <cxxopts.hpp>
#include <fstream>
#include <optional>

using nlohmann::json;

/**
 * @brief Define the command-line interface for the program.
 *
 * This function sets up the command-line options for the program using the
 * cxxopts library. The options include a path to the configuration file, a
 * help option, and a version option.
 *
 * @return cxxopts::Options The options object containing the command-line options.
 */
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

/**
 * @brief Extract the path to the configuration file from the command-line arguments.
 *
 * This function parses the command-line arguments using the cxxopts library and
 * extracts the path to the configuration file if it was provided. If the help
 * option was requested, the function prints the help message and exits. If the
 * version option was requested, the function prints the version number and exits.
 * If the config option was not provided, the function prints a warning message
 * and returns an empty optional value.
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments.
 * @param options The cxxopts::Options object containing the command-line options.
 * @return std::optional<std::string> The path to the configuration file if it was
 * provided, or an empty optional value if it was not.
 */
std::optional<std::string> extractConfigFilePath(int argc, char* argv[], auto& options) {
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
    std::cerr << "Warning: No configuration file specified. Use --config <path_to_config_file> to "
                 "do so. Using the default now."
              << std::endl;
  } else {
    // Get the path to the configuration file
    return result["config"].template as<std::string>();
  }
  return {};
}

/**
 * @brief Parse a JSON file into a json object.
 *
 * This function opens the specified file and parses its contents into a json
 * object using the nlohmann::json library. If the file cannot be opened or if
 * there is an error parsing the JSON, the function throws a std::runtime_error
 * exception.
 *
 * @param filename The name of the JSON file to parse.
 * @return json The json object containing the parsed JSON data.
 */
json parseJsonFile(const std::string& filename) {
  // Open the file
  std::ifstream file(filename);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open file: " + filename);
  }
  // Parse the JSON file into an json object
  json json;
  try {
    file >> json;
  } catch (const json::parse_error& e) {
    throw std::runtime_error("Error parsing JSON file: " + filename + " - " + e.what());
  }
  return json;
}

/**
 * @brief Read the path to the configuration file from the command-line arguments.
 *
 * In doing so, it also handles other command-line arguments like `help` or `version`.
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments.
 * @return std::optional<std::string> The path to the configuration file if it was
 * provided, or an empty optional value if it was not.
 */

auto readConfigFilePath(int argc, char* argv[]) {
  auto options = defineCmdlineInterface();
  try {
    return extractConfigFilePath(argc, argv, options);
  } catch (const cxxopts::OptionException& e) {
    std::cerr << "Error parsing options: " << e.what() << std::endl;
    exit(1);
  }
}

/// @brief The default configuration supplementing whatever is given by the user.
json defaultConfig{};

/**
 * @brief Supplement the provided configuration with default values.
 *
 * This function takes a json object representing the provided configuration
 * and merges it with a default configuration using the merge_patch method.
 * The resulting json object is returned.
 *
 * @param providedConfig The json object representing the provided configuration.
 * @return json The json object representing the supplemented configuration.
 */
json supplementWithDefaults(const json& providedConfig) {
  auto config = defaultConfig;
  config.merge_patch(providedConfig);
  return config;
}

/**
 * @brief Compose the final configuration for the program.
 *
 * This function is responsible for composing the final configuration for the
 * program. It first reads the path to the configuration file's name from the
 * command-line arguments. If a configuration file was provided, it parses the JSON file and
 * supplements it with default values. If no configuration file was provided, the function returns
 * the default configuration.
 *
 * @param argc The number of command-line arguments.
 * @param argv The array of command-line arguments.
 * @return json The json object representing the final configuration for the program.
 */
json composeConfig(int argc, char* argv[]) {
  auto configFilePath = readConfigFilePath(argc, argv);
  if (configFilePath) {
    auto providedConfig = parseJsonFile(configFilePath.value());
    return supplementWithDefaults(providedConfig);
  }
  return defaultConfig;
}

struct IndividualReports {};

/**
 * @brief Compose a report from the provided metadata, configuration, and individual reports.
 *
 * This function takes a json object representing the metadata, a json object
 * representing the configuration, and a json object representing the individual
 * reports, and composes a report by merging them into a single json object.
 * The resulting json object is returned.
 *
 * @param metadata The json object representing the metadata.
 * @param config The json object representing the configuration.
 * @param individualReports The json object representing the individual reports.
 * @return json The json object representing the composed report.
 */
json composeReport(json const& metadata, json const& config,
                   [[maybe_unused]] IndividualReports const& individualReports) {
  json report{};
  report["metadata"] = metadata;
  report["config"] = config;
  return report;
}

void output(json const& report) { std::cout << report << std::endl; }

auto main(int argc, char* argv[]) -> int {
  auto metadata = membenchmc::gatherMetadata();
  auto config = composeConfig(argc, argv);
  auto reports = IndividualReports{};
  auto report = composeReport(metadata, config, reports);
  output(report);
  return EXIT_SUCCESS;
}
