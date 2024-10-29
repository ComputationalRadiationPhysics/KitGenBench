#include <doctest/doctest.h>
#include <membenchmc/membenchmc.h>
#include <membenchmc/version.h>

#include <string>

TEST_CASE("MemBenchMC") {
  using namespace membenchmc;

  MemBenchMC membenchmc("Tests");

  CHECK(membenchmc.greet(LanguageCode::EN) == "Hello, Tests!");
  CHECK(membenchmc.greet(LanguageCode::DE) == "Hallo Tests!");
  CHECK(membenchmc.greet(LanguageCode::ES) == "¡Hola Tests!");
  CHECK(membenchmc.greet(LanguageCode::FR) == "Bonjour Tests!");
}

TEST_CASE("MemBenchMC version") {
  static_assert(std::string_view(MEMBENCHMC_VERSION) == std::string_view("1.0"));
  CHECK(std::string(MEMBENCHMC_VERSION) == std::string("1.0"));
}
