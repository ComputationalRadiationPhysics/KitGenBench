#pragma once
#include <alpaka/alpaka.hpp>
#include <nlohmann/json.hpp>

#include "alpaka/queue/Properties.hpp"

namespace membenchmc {

  namespace Actions {
    // This namespace mimics an enum but is supposed to be extended by the user to allow for more
    // setups. Library-defined actions have negative values, user-defined positive ones.
    constexpr int STOP = -1;
    constexpr int CHECK = -2;
  }  // namespace Actions

  template <typename TAcc, typename TDev> struct ExecutionDetails {
    alpaka::WorkDivMembers<alpaka::Dim<TAcc>, alpaka::Idx<TAcc>> workdiv{};
    TDev device{};
  };

  template <typename TRecipe>
  concept Recipe = requires(TRecipe recipe) {
    { std::get<0>(recipe.next()) } -> std::same_as<decltype(Actions::STOP)>;
  };

  template <typename TLogger, typename TFunctor>
  concept Logger = requires(TLogger logger, TFunctor func) {
    { (logger.call(func)) } -> std::same_as<decltype(func())>;
  };

  template <typename TChecker, typename TResult>
  concept Checker = requires(TChecker checker, TResult result) {
    std::get<0>(checker.check(result)) == Actions::CHECK;
    { std::get<1>(checker.check(result)) } -> std::same_as<bool>;
  };

  struct BenchmarkKernel {
    template <typename TAcc> ALPAKA_FN_ACC auto operator()(TAcc const& acc, auto instructions) const
        -> void {
      auto const globalThreadIdx = alpaka::getIdx<alpaka::Grid, alpaka::Threads>(acc);
      auto const globalThreadExtent = alpaka::getWorkDiv<alpaka::Grid, alpaka::Threads>(acc);
      auto const linearizedGlobalThreadIdx
          = alpaka::mapIdx<1u>(globalThreadIdx, globalThreadExtent).x();

      // Get a local copy, so we work on registers and don't strain global memory too much.
      auto myRecipe = instructions.recipes.load(linearizedGlobalThreadIdx);
      auto myLogger = instructions.loggers.load(linearizedGlobalThreadIdx);
      auto myChecker = instructions.checkers.load(linearizedGlobalThreadIdx);

      bool recipeExhausted = false;
      while (not recipeExhausted) {
        auto result = myLogger.call([&myRecipe] { return myRecipe.next(); });
        myLogger.call([&myChecker, &result] { return myChecker.check(result); });
        recipeExhausted = (std::get<0>(result) == Actions::STOP);
      }

      // Put our local copy back from where we got it.
      instructions.recipes.store(std::move(myRecipe), linearizedGlobalThreadIdx);
      instructions.loggers.store(std::move(myLogger), linearizedGlobalThreadIdx);
      instructions.checkers.store(std::move(myChecker), linearizedGlobalThreadIdx);
    }
  };

  template <typename TExecutionDetails, typename TInstructionDetails> struct Setup {
    std::string name{};
    TExecutionDetails execution{};
    TInstructionDetails instructions{};
    nlohmann::json description{};
  };

  namespace detail {
    template <template <typename, typename> typename TExecutionDetails, typename TAcc,
              typename TDev>
    struct AccOf {
      TExecutionDetails<TAcc, TDev> execution;
      using type = TAcc;
    };
  }  // namespace detail

  nlohmann::json runBenchmark(auto& setup) {
    auto start = std::chrono::high_resolution_clock::now();
    using Acc = decltype(detail::AccOf{setup.execution})::type;
    auto queue = alpaka::Queue<Acc, alpaka::Blocking>(setup.execution.device);

    alpaka::exec<Acc>(queue, setup.execution.workdiv, BenchmarkKernel{},
                      setup.instructions.sendTo(setup.execution.device, queue));
    alpaka::wait(queue);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    return {{"total runtime [ns]", duration},
            {"logs", setup.instructions.loggers.generateReport()},
            {"checks", setup.instructions.checkers.generateReport()},
            {"description", setup.description}};
  };

  template <typename... TSetup> nlohmann::json runBenchmarks(TSetup&... setup) {
    auto start = std::chrono::high_resolution_clock::now();
    auto finalReport = nlohmann::json::object();
    (finalReport.push_back({setup.name, runBenchmark(setup)}), ...);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    finalReport["total runtime [ns]"] = duration;
    return finalReport;
  };

  /**
   * @brief Gathers metadata about the system, including start time, hostname, username, and CPU
   * information.
   *
   * @return nlohmann::json A JSON object containing the gathered metadata.
   */
  nlohmann::json gatherMetadata();
}  // namespace membenchmc
