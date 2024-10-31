#include <doctest/doctest.h>
#include <membenchmc/membenchmc.h>
#include <membenchmc/version.h>

#include <alpaka/alpaka.hpp>
#include <string>

TEST_CASE("MemBenchMC") {
  using Dim = alpaka::DimInt<1>;
  using namespace membenchmc;
  std::cout << Dim::value << std::endl;
}

TEST_CASE("MemBenchMC version") {
  static_assert(std::string_view(MEMBENCHMC_VERSION) == std::string_view("0.1"));
  CHECK(std::string(MEMBENCHMC_VERSION) == std::string("0.1"));
}
