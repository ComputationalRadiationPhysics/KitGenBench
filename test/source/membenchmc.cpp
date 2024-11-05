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

  template <typename T> struct Aggregate {
    ALPAKA_FN_ACC T load(auto const) { return {}; }
    ALPAKA_FN_ACC void store(T&&, auto const) {}
    nlohmann::json generateReport() { return {}; }
  };

  template <typename TRecipe = membenchmc::setup::NoRecipe,
            typename TLogger = membenchmc::setup::NoLogger,
            typename TChecker = membenchmc::setup::NoChecker>
  struct InstructionDetails {
    Aggregate<membenchmc::setup::NoRecipe> recipes{};
    Aggregate<membenchmc::setup::NoLogger> loggers{};
    Aggregate<membenchmc::setup::NoChecker> checkers{};

    auto sendTo([[maybe_unused]] auto const& device, [[maybe_unused]] auto& queue) {
      // This is not exactly how it's supposed to be used (or at least it's not necessary). You
      // can return any object following the concept of InstructionDetails and typically this
      // would be something more specialised for the deivce.
      return InstructionDetails{};
    }
  };

  namespace nosetup {

    InstructionDetails<> makeInstructionDetails() { return {}; }

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

namespace setups::singleSizeMalloc {

  struct SingleSizeMallocRecipe {
    static constexpr std::uint32_t allocationSize{16U};
    static constexpr std::uint32_t numAllocations{100U};
    std::array<std::byte*, numAllocations> pointers{{}};
    std::uint32_t counter{0U};

    ALPAKA_FN_ACC auto next([[maybe_unused]] const auto& acc) {
      if (counter >= numAllocations)
        return std::make_tuple(
            membenchmc::Actions::STOP,
            std::span<std::byte>{static_cast<std::byte*>(nullptr), allocationSize});
      pointers[counter] = static_cast<std::byte*>(malloc(allocationSize));
      auto result = std::make_tuple(+membenchmc::Actions::MALLOC,
                                    std::span(pointers[counter], allocationSize));
      counter++;
      return result;
    }

    nlohmann::json generateReport() { return {}; }
  };

  auto makeInstructionDetails() { return InstructionDetails<SingleSizeMallocRecipe>{}; }

  auto composeSetup() {
    return composeSetup("singleSizeMalloc", makeExecutionDetails(), makeInstructionDetails(),
                        {{"allocation size [bytes]", SingleSizeMallocRecipe::allocationSize},
                         {"number of allocations", SingleSizeMallocRecipe::numAllocations}});
  }

}  // namespace setups::singleSizeMalloc

TEST_CASE("Single size malloc") {
  auto setup = setups::singleSizeMalloc::composeSetup();
  auto benchmarkReports = runBenchmarks(setup);
}
