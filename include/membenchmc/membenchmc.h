#pragma once

#include <string>

namespace membenchmc {

  /**  Language codes to be used with the MemBenchMC class */
  enum class LanguageCode { EN, DE, ES, FR };

  /**
   * @brief A class for saying hello in multiple languages
   */
  class MemBenchMC {
    std::string name;

  public:
    /**
     * @brief Creates a new membenchmc
     * @param name the name to greet
     */
    MemBenchMC(std::string name);

    /**
     * @brief Creates a localized string containing the greeting
     * @param lang the language to greet in
     * @return a string containing the greeting
     */
    std::string greet(LanguageCode lang = LanguageCode::EN) const;
  };

}  // namespace membenchmc
