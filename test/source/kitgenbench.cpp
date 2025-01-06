#include <doctest/doctest.h>
#include <kitgenbench/kitgenbench.h>
#include <kitgenbench/setup.h>
#include <kitgenbench/version.h>
#include <sys/types.h>

#include <alpaka/core/Common.hpp>
#include <cstdint>
#include <string>

#include "nlohmann/json.hpp"

TEST_CASE("KitGenBench version") {
  static_assert(std::string_view(KITGENBENCH_VERSION) == std::string_view("0.1"));
  CHECK(std::string(KITGENBENCH_VERSION) == std::string("0.1"));
}

namespace kitgenbench::Actions {
  [[maybe_unused]] static constexpr int MALLOC = 1;
  [[maybe_unused]] static constexpr int FREE = 2;
}  // namespace kitgenbench::Actions

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
    return kitgenbench::ExecutionDetails<Acc, decltype(dev)>{workdiv, dev};
  }

  template <typename T> struct Aggregate {
    ALPAKA_FN_ACC T load(auto const) { return {}; }
    ALPAKA_FN_ACC void store(const auto&, T&&, auto const) {}
    nlohmann::json generateReport() { return {}; }
  };

  template <typename TRecipe = kitgenbench::setup::NoRecipe,
            typename TLogger = kitgenbench::setup::NoLogger,
            typename TChecker = kitgenbench::setup::NoChecker>
  struct InstructionDetails {
    Aggregate<TRecipe> recipes{};
    Aggregate<TLogger> loggers{};
    Aggregate<TChecker> checkers{};

    auto sendTo([[maybe_unused]] auto const& device, [[maybe_unused]] auto& queue) {
      // This is not exactly how it's supposed to be used (or at least it's not necessary). You
      // can return any object following the concept of InstructionDetails and typically this
      // would be something more specialised for the deivce.
      return this;
    }

    auto retrieveFrom([[maybe_unused]] auto const& device, [[maybe_unused]] auto& queue) {}

    nlohmann::json generateReport() {
      return {{"recipes", recipes.generateReport()},
              {"logs", loggers.generateReport()},
              {"checks", checkers.generateReport()}};
    }
  };

  namespace nosetup {

    InstructionDetails<> makeInstructionDetails() { return {}; }

    auto composeSetup() {
      return kitgenbench::setup::composeSetup("No Setup", makeExecutionDetails(),
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
            kitgenbench::Actions::STOP,
            std::span<std::byte>{static_cast<std::byte*>(nullptr), allocationSize});
      pointers[counter] = static_cast<std::byte*>(malloc(allocationSize));
      auto result = std::make_tuple(+kitgenbench::Actions::MALLOC,
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

namespace setups::mallocFreeManySize {

  struct MallocFreeRecipe {
    const std::array<std::uint32_t, 9> sizes{16U, 256U, 1024U, 16U, 16U, 256U, 16U, 1024U, 1024U};
    std::uint32_t currentIndex{0U};
    void* currentPointer{nullptr};

    ALPAKA_FN_ACC auto next([[maybe_unused]] const auto& acc) {
      if (currentIndex >= sizes.size()) {
        return std::make_tuple(kitgenbench::Actions::STOP,
                               std::span<std::byte>{static_cast<std::byte*>(nullptr), 0U});
      }

      if (currentPointer == nullptr) {
        currentPointer = malloc(sizes[currentIndex]);
        return std::make_tuple(
            +kitgenbench::Actions::MALLOC,
            std::span<std::byte>{static_cast<std::byte*>(currentPointer), sizes[currentIndex]});
      } else {
        free(currentPointer);
        auto result = std::make_tuple(
            +kitgenbench::Actions::FREE,
            std::span<std::byte>{static_cast<std::byte*>(currentPointer), sizes[currentIndex]});
        currentPointer = nullptr;
        currentIndex++;
        return result;
      }
    }

    nlohmann::json generateReport() { return {}; }
  };

  auto makeInstructionDetails() { return InstructionDetails<MallocFreeRecipe>{}; }

  auto composeSetup() {
    return composeSetup("mallocFreeManySize", makeExecutionDetails(), makeInstructionDetails(),
                        {{"what it does",
                          "This setup runs through a given vector of allocation sizes, allocating "
                          "and deallocating each size one after another."},
                         {"number of allocations", MallocFreeRecipe{}.sizes.size()}});
  }

}  // namespace setups::mallocFreeManySize

TEST_CASE("Malloc free many size") {
  auto setup = setups::mallocFreeManySize::composeSetup();
  auto benchmarkReports = kitgenbench::runBenchmarks(setup);
}
