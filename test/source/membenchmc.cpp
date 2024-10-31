#include <doctest/doctest.h>
#include <membenchmc/membenchmc.h>
#include <membenchmc/version.h>
#include <sys/types.h>

#include <alpaka/alpaka.hpp>
#include <optional>
#include <string>

#include "alpaka/acc/AccCpuSerial.hpp"
#include "alpaka/acc/Traits.hpp"
#include "alpaka/workdiv/WorkDivMembers.hpp"
#include "nlohmann/json.hpp"

struct NoRecipe {};
struct NoChecker {};
struct NoLogger {};

template <typename TRecipe> struct Aggregate {
  TRecipe recipe;
  template <typename TAcc> auto get() { return std::span<TRecipe, 1>(&recipe, 1); }
};

using Dim = alpaka::DimInt<1>;
using Idx = std::uint32_t;
using Acc = alpaka::AccCpuSerial<Dim, Idx>;

TEST_CASE("runBenchmark trivially") {
  using namespace membenchmc;

  auto workdiv = alpaka::WorkDivMembers<Dim, Idx>{
      alpaka::Vec<Dim, Idx>{1}, alpaka::Vec<Dim, Idx>{1}, alpaka::Vec<Dim, Idx>{1}};
  auto recipes = Aggregate<NoRecipe>{};
  auto loggers = Aggregate<NoLogger>{};
  auto checkers = Aggregate<NoChecker>{};

  SUBCASE("can do nothing.") { runBenchmark<Acc>(workdiv, recipes, loggers, checkers); }

  SUBCASE("returns a report about that accelerator.") {
    auto report = runBenchmark<Acc>(workdiv, recipes, loggers, checkers);
    CHECK(report.contains(alpaka::getAccName<Acc>()));
  }
}

struct MallocFreeRecipe {
  std::vector<std::uint32_t> sizes{};
  std::uint32_t currentIndex{0U};
  void* currentPointer{nullptr};
};

template <>
std::optional<membenchmc::StepResult> membenchmc::next<MallocFreeRecipe>(MallocFreeRecipe& recipe) {
  if (recipe.currentIndex == recipe.sizes.size()) return std::nullopt;
  if (recipe.currentPointer == nullptr) {
    recipe.currentPointer = malloc(recipe.sizes[recipe.currentIndex]);
    return StepResult{"malloc", recipe.currentPointer};
  } else {
    free(recipe.currentPointer);
    recipe.currentPointer = nullptr;
    recipe.currentIndex++;
    return StepResult{"free", recipe.currentPointer};
  }
}

TEST_CASE("runBenchmark") {
  using namespace membenchmc;

  auto workdiv = alpaka::WorkDivMembers<Dim, Idx>{
      alpaka::Vec<Dim, Idx>{1}, alpaka::Vec<Dim, Idx>{1}, alpaka::Vec<Dim, Idx>{1}};
  auto recipes = Aggregate<MallocFreeRecipe>{{{16U}}};
  auto loggers = Aggregate<NoLogger>{};
  auto checkers = Aggregate<NoChecker>{};

  SUBCASE("can handle simple recipe.") { runBenchmark<Acc>(workdiv, recipes, loggers, checkers); }
}
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
