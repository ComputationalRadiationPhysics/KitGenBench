#include <doctest/doctest.h>
#include <membenchmc/membenchmc.h>
#include <membenchmc/setup.h>
#include <membenchmc/version.h>
#include <sys/types.h>

#include <alpaka/core/Common.hpp>
#include <string>

#include "nlohmann/json.hpp"

TEST_CASE("MemBenchMC version") {
  static_assert(std::string_view(MEMBENCHMC_VERSION) == std::string_view("0.1"));
  CHECK(std::string(MEMBENCHMC_VERSION) == std::string("0.1"));
}

namespace membenchmc::Actions {
  [[maybe_unused]] static constexpr int MALLOC = 1;
  [[maybe_unused]] static constexpr int FREE = 2;
}  // namespace membenchmc::Actions

using Dim = alpaka::DimInt<1>;
using Idx = std::uint32_t;
using Acc = alpaka::TagToAcc<std::remove_cvref_t<decltype(std::get<0>(alpaka::EnabledAccTags{}))>,
                             Dim, Idx>;

namespace setups {

  auto makeExecutionDetails() {
    auto const platformAcc = alpaka::Platform<Acc>{};
    auto const dev = alpaka::getDevByIdx(platformAcc, 0);
    auto workdiv = alpaka::WorkDivMembers<Dim, Idx>{
        alpaka::Vec<Dim, Idx>{1}, alpaka::Vec<Dim, Idx>{1}, alpaka::Vec<Dim, Idx>{1}};
    return membenchmc::ExecutionDetails<Acc, decltype(dev)>{workdiv, dev};
  }

  namespace nosetup {

    template <typename T> struct Aggregate {
      ALPAKA_FN_ACC T load(auto const) { return {}; }
      ALPAKA_FN_ACC void store(T&&, auto const) {}
      nlohmann::json generateReport() { return {}; }
    };

    struct InstructionDetails {
      Aggregate<membenchmc::setup::NoRecipe> recipes{};
      Aggregate<membenchmc::setup::NoLogger> loggers{};
      Aggregate<membenchmc::setup::NoChecker> checkers{};

      auto sendTo([[maybe_unused]] auto const& device, [[maybe_unused]] auto& queue) {
        // This is not exactly how it's supposed to be used (or at least it's not necessary). You
        // can return any object following the concept of InstructionDetails.
        return InstructionDetails{};
      }
    };

    InstructionDetails makeInstructionDetails() { return {}; }

    auto composeSetup() {
      return membenchmc::setup::composeSetup("No Setup", makeExecutionDetails(),
                                             makeInstructionDetails(),
                                             {"what it does", "This does exactly nothing."});
    }
  }  // namespace nosetup
}  // namespace setups

TEST_CASE("No Setup") {
  auto setup = setups::nosetup::composeSetup();
  auto benchmarkReports = runBenchmarks(setup);
}
