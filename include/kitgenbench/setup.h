
#pragma once
#include <alpaka/core/Common.hpp>
#include <string>
#include <tuple>

#include "nlohmann/json.hpp"

namespace kitgenbench::Actions {
  // This namespace mimics an enum but is supposed to be extended by the user to allow for more
  // setups. Library-defined actions have negative values, user-defined positive ones.
  static constexpr int STOP = -1;
  static constexpr int CHECK = -2;
}  // namespace kitgenbench::Actions

namespace kitgenbench::setup {
  template <typename TExecutionDetails, typename TInstructionDetails> struct Setup {
    std::string name{};
    TExecutionDetails execution{};
    TInstructionDetails instructions{};
    nlohmann::json description{};
  };

  template <typename TExecutionDetails, typename TInstructionDetails>
  Setup<TExecutionDetails, TInstructionDetails> composeSetup(std::string name,
                                                             TExecutionDetails execution,
                                                             TInstructionDetails instructions,
                                                             nlohmann::json description) {
    // Instructions might be heavy weight because the recipes, loggers and checkers might have
    // allocated some memory to manage their state.
    return {name, execution, std::move(instructions), description};
  }

  struct NoChecker {
    ALPAKA_FN_ACC auto check([[maybe_unused]] const auto& acc,
                             [[maybe_unused]] const auto& result) {
      return std::make_tuple(Actions::CHECK, true);
    }

    nlohmann::json generateReport() { return {}; }
  };

  struct NoLogger {
    ALPAKA_FN_INLINE ALPAKA_FN_ACC auto call(auto const& acc, auto func) { return func(acc); }

    nlohmann::json generateReport() { return {}; }
  };

  struct NoRecipe {
    ALPAKA_FN_ACC auto next([[maybe_unused]] const auto& acc) {
      return std::make_tuple(Actions::STOP);
    }

    nlohmann::json generateReport() { return {}; }
  };
}  // namespace kitgenbench::setup
