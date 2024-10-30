#include <doctest/doctest.h>
#include <membenchmc/membenchmc.h>
#include <membenchmc/version.h>

#include <string>

TEST_CASE("MemBenchMC") { using namespace membenchmc; }

TEST_CASE("MemBenchMC version") {
  static_assert(std::string_view(MEMBENCHMC_VERSION) == std::string_view("0.1"));
  CHECK(std::string(MEMBENCHMC_VERSION) == std::string("0.1"));
}
