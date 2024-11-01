#include <doctest/doctest.h>
#include <membenchmc/membenchmc.h>
#include <membenchmc/version.h>
#include <sys/types.h>

#include <alpaka/alpaka.hpp>
#include <chrono>
#include <optional>
#include <string>

#include "alpaka/acc/AccCpuSerial.hpp"
#include "alpaka/dim/Traits.hpp"
#include "alpaka/workdiv/WorkDivMembers.hpp"
#include "nlohmann/json.hpp"

struct NoRecipe {
  std::optional<membenchmc::StepResult> next() { return std::nullopt; }
};
struct NoChecker {
  nlohmann::json generateReport() { return {}; }
};
struct NoLogger {
  auto call(auto const& func) { return func(); }
};

template <typename TRecipe> struct Aggregate {
  using type = TRecipe;
  TRecipe recipe;
  static constexpr size_t size = 1U;
  auto get([[maybe_unused]] auto& queue, [[maybe_unused]] auto const& device) {
    return std::span<TRecipe, 1>(&recipe, 1);
  }
  auto generateReport() { return recipe.generateReport(); }
};

using Dim = alpaka::DimInt<1>;
using Idx = std::uint32_t;
using Acc = alpaka::AccCpuSerial<Dim, Idx>;

struct MallocFreeRecipe {
  std::vector<std::uint32_t> sizes{};
  std::uint32_t currentIndex{0U};
  void* currentPointer{nullptr};

  std::optional<membenchmc::StepResult> next() {
    if (currentIndex >= sizes.size()) return std::nullopt;

    if (currentPointer == nullptr) {
      currentPointer = malloc(sizes[currentIndex]);
      return membenchmc::StepResult{"malloc", currentPointer};
    } else {
      free(currentPointer);
      currentPointer = nullptr;
      currentIndex++;
      return membenchmc::StepResult{"free", currentPointer};
    }
  }
};

struct SumUpLogger {
  std::map<std::string, std::chrono::nanoseconds> durations;

  auto call(const auto& func) {
    auto start = std::chrono::high_resolution_clock::now();
    std::optional<membenchmc::StepResult> result = func();
    auto end = std::chrono::high_resolution_clock::now();

    if (not result.has_value()) {
      return result;
    }

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    if (durations.contains(result.value().action)) {
      durations[result.value().action] += duration;
    } else {
      durations[result.value().action] = duration;
    }

    return result;
  }

  nlohmann::json generateReport() {
    auto report = nlohmann::json::object();
    for (auto const& [key, value] : durations) {
      report[key + " [ns]"] = value.count();
    }
    return report;
  }
};

TEST_CASE("initialiseBenchmark") {
  using namespace membenchmc;
  nlohmann::json config{};

  SUBCASE("takes a json config and returns recipes, loggers and checkers.") {
    [[maybe_unused]] auto [recipes, loggers, checkers] = initialiseBenchmark(config);
  }
}

TEST_CASE("MemBenchMC version") {
  static_assert(std::string_view(MEMBENCHMC_VERSION) == std::string_view("0.1"));
  CHECK(std::string(MEMBENCHMC_VERSION) == std::string("0.1"));
}

template <typename TAcc, typename TDev> struct ExecutionDetails {
  alpaka::WorkDivMembers<alpaka::Dim<TAcc>, alpaka::Idx<TAcc>> workdiv{};
  TDev device{};
};

template <typename TRecipes, typename TLoggers, typename TCheckers> struct InstructionDetails {
  TRecipes recipes{};
  TLoggers loggers{};
  TCheckers checkers{};

  struct Package {
    std::span<typename TRecipes::type, TRecipes::size> recipes{};
    std::span<typename TLoggers::type, TLoggers::size> loggers{};
    std::span<typename TCheckers::type, TCheckers::size> checkers{};
  };

  auto sendTo([[maybe_unused]] auto const& device, auto& queue) {
    auto r = recipes.get(queue, device);
    auto l = loggers.get(queue, device);
    auto c = checkers.get(queue, device);
    return Package{r, l, c};
  }
};

template <typename TExecutionDetails, typename TInstructionDetails> struct Setup {
  std::string name{};
  TExecutionDetails execution{};
  TInstructionDetails instructions{};
  nlohmann::json description{};
};

template <typename TExecutionDetails, typename TInstructionDetails>
Setup<TExecutionDetails, TInstructionDetails> bundleUp(std::string name,
                                                       TExecutionDetails execution,
                                                       TInstructionDetails instructions,
                                                       nlohmann::json description) {
  return {name, execution, instructions, description};
}

auto makeExecutionDetails() {
  using Dim = alpaka::DimInt<1>;
  using Idx = std::uint32_t;
  using Acc = alpaka::AccCpuSerial<Dim, Idx>;
  auto const platformAcc = alpaka::Platform<Acc>{};
  auto const dev = alpaka::getDevByIdx(platformAcc, 0);
  auto workdiv = alpaka::WorkDivMembers<Dim, Idx>{
      alpaka::Vec<Dim, Idx>{1}, alpaka::Vec<Dim, Idx>{1}, alpaka::Vec<Dim, Idx>{1}};
  return ExecutionDetails<Acc, decltype(dev)>{workdiv, dev};
}

auto makeInstructionDetails() {
  auto recipes = Aggregate<MallocFreeRecipe>{{{16U}}};
  auto loggers = Aggregate<SumUpLogger>{};
  auto checkers = Aggregate<NoChecker>{};
  return InstructionDetails<decltype(recipes), decltype(loggers), decltype(checkers)>{
      recipes, loggers, checkers};
}

TEST_CASE("runBenchmarks") {
  auto setup = bundleUp("Test", makeExecutionDetails(), makeInstructionDetails(), {});
  auto reports = membenchmc::runBenchmarks(setup);
  std::cout << reports << std::endl;
}
