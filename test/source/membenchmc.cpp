#include <doctest/doctest.h>
#include <membenchmc/membenchmc.h>
#include <membenchmc/version.h>

#include <alpaka/alpaka.hpp>
#include <string>

#include "alpaka/acc/AccCpuSerial.hpp"
#include "alpaka/acc/Traits.hpp"
#include "nlohmann/json.hpp"

struct NoRecipe {};
struct NoChecker {};
struct NoLogger {};

using Dim = alpaka::DimInt<1>;
using Idx = std::uint32_t;
using TAcc = alpaka::AccCpuSerial<Dim, Idx>;

TEST_CASE("runBenchmark trivially") {
  using namespace membenchmc;

  auto recipes = NoRecipe{};
  auto loggers = NoLogger{};
  auto checkers = NoChecker{};

  SUBCASE("can do nothing.") { runBenchmark<TAcc>(recipes, loggers, checkers); }

  SUBCASE("returns a report about that accelerator.") {
    auto report = runBenchmark<TAcc>(recipes, loggers, checkers);
    CHECK(report.contains(alpaka::getAccName<TAcc>()));
  }
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
