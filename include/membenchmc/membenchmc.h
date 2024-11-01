#pragma once
#include <alpaka/alpaka.hpp>
#include <nlohmann/json.hpp>
#include <optional>

#include "alpaka/queue/Properties.hpp"

namespace membenchmc {

  inline auto initialiseBenchmark([[maybe_unused]] nlohmann::json const& config) {
    return std::make_tuple(1, 2, 3);
  }

  struct StepResult {
    std::string action{};
    void* pointer{};
  };

  template <typename TRecipe>
  concept Recipe = requires(TRecipe recipe) {
    { recipe.next() } -> std::same_as<std::optional<StepResult>>;
  };

  template <typename TChecker>
  std::optional<StepResult> check([[maybe_unused]] TChecker& checker,
                                  [[maybe_unused]] std::optional<StepResult> result) {
    return {};
  }

  template <typename TLogger>
  auto callLogged([[maybe_unused]] TLogger& logger, [[maybe_unused]] auto const& func) {
    return func();
  }

  struct BenchmarkKernel {
    template <typename TAcc>
    ALPAKA_FN_ACC auto operator()(TAcc const& acc, auto recipes, auto loggers, auto checkers) const
        -> void {
      auto const globalThreadIdx = alpaka::getIdx<alpaka::Grid, alpaka::Threads>(acc);
      auto const globalThreadExtent = alpaka::getWorkDiv<alpaka::Grid, alpaka::Threads>(acc);
      auto const linearizedGlobalThreadIdx
          = alpaka::mapIdx<1u>(globalThreadIdx, globalThreadExtent).x();

      auto& myRecipe = recipes[linearizedGlobalThreadIdx];
      auto& myLogger = loggers[linearizedGlobalThreadIdx];
      auto& myChecker = checkers[linearizedGlobalThreadIdx];

      std::optional<StepResult> result{};
      do {
        result = myLogger.call([&myRecipe] { return myRecipe.next(); });
        myLogger.call([&myChecker, &result] { return check(myChecker, result); });
      } while (result);
    }
  };

  template <typename TAcc>
  nlohmann::json runBenchmark(auto const& workdiv, [[maybe_unused]] auto& recipes,
                              [[maybe_unused]] auto& loggers, [[maybe_unused]] auto& checkers) {
    auto const platformAcc = alpaka::Platform<TAcc>{};
    auto const dev = alpaka::getDevByIdx(platformAcc, 0);
    auto queue = alpaka::Queue<TAcc, alpaka::Blocking>(dev);

    alpaka::exec<TAcc>(queue, workdiv, BenchmarkKernel{}, recipes.template get<TAcc>(),
                       loggers.template get<TAcc>(), checkers.template get<TAcc>());
    alpaka::wait(queue);

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
