#pragma once
#include <alpaka/alpaka.hpp>
#include <nlohmann/json.hpp>

namespace membenchmc {

  inline auto initialiseBenchmark([[maybe_unused]] nlohmann::json const& config) {
    return std::make_tuple(1, 2, 3);
  }

  template <typename TAcc> nlohmann::json runBenchmark([[maybe_unused]] auto& recipes,
                                                       [[maybe_unused]] auto& loggers,
                                                       [[maybe_unused]] auto& checkers) {
    return {{alpaka::getAccName<TAcc>(), ""}};
  };
  /**
   * @brief Gathers metadata about the system, including start time, hostname, username, and CPU
   * information.
   *
   * @return nlohmann::json A JSON object containing the gathered metadata.
   */
  nlohmann::json gatherMetadata();
}  // namespace membenchmc
