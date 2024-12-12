[![Actions Status](https://github.com/chillenzer/KitGenBench/workflows/Ubuntu/badge.svg)](https://github.com/chillenzer/KitGenBench/actions)
[![Actions Status](https://github.com/chillenzer/KitGenBench/workflows/Install/badge.svg)](https://github.com/chillenzer/KitGenBench/actions)
[![codecov](https://codecov.io/gh/chillenzer/KitGenBench/branch/master/graph/badge.svg)](https://codecov.io/gh/chillenzer/KitGenBench)

*Have you ever sat in a restaurant and wondered: Why the heck do they already have their Cock Au Vin on the next table and my simple pasta is taking forever? If you're anything like us, you immediately apply for funding to set up a large scale experiment measuring preparation times for all combinations of recipes. It's, of course, the only reasonable thing to do.*

*Well, we can't help you with the funding. What we can offer you is a comfy kitchen bench to take a seat, observe and come up with a unified theory of bolognesio synthesis revolutionising the field. So, compose your recipe, lean back and observe...*

*Wait, did we say "kitchen bench"? We meant...*

# KitGenBench

KitGenBench is a Kit for Generating (Micro-)Benchmarks based on [alpaka](https://github.com/alpaka-group/alpaka).
As such, it is suitable to generate benchmarks on a wide variety of hardware architectures
including all major CPU and GPU vendors as well as some FPGAs.

## Usage

The main idea is very simple:
You provide three pieces of information that will be used by the infrastructure to run benchmarks.
These are:

- a recipe saying what the benchmark does
- a checker saying how to verify correctness of the benchmarks
- a logger saying how to record what happens during the benchmark

The main loop of the program is as simple as (up to technical details)

```C++
while (not recipeExhausted) {
  result = logger.call([]{recipe.next();});
  logger.call([]{checker.check(result);});
}
```

See [examples](./examples) for recipes inspirations and technical details.

## Installation

### Build and run the standalone target

Use the following command to build and run the executable target.

```bash
cmake -S standalone -B build/standalone
cmake --build build/standalone
./build/standalone/KitGenBench --help
```

### Build and run test suite

Use the following commands from the project's root directory to run the test suite.

```bash
cmake -S test -B build/test
cmake --build build/test
CTEST_OUTPUT_ON_FAILURE=1 cmake --build build/test --target test

# or simply call the executable:
./build/test/KitGenBenchTests
```

To collect code coverage information, run CMake with the `-DENABLE_TEST_COVERAGE=1` option.

### Build the documentation

The documentation is automatically built and [published](https://chillenzer.github.io/KitGenBench) whenever a [GitHub Release](https://help.github.com/en/github/administering-a-repository/managing-releases-in-a-repository) is created.
To manually build documentation, call the following command.

```bash
cmake -S documentation -B build/doc
cmake --build build/doc --target GenerateDocs
# view the docs
open build/doc/doxygen/html/index.html
```

To build the documentation locally, you will need Doxygen, jinja2 and Pygments installed on your system.

### Build everything at once

The project also includes an `all` directory that allows building all targets at the same time.
This is useful during development, as it exposes all subprojects to your IDE and avoids redundant builds of the library.

```bash
cmake -S all -B build
cmake --build build

# run tests
./build/test/KitGenBenchTests
# format code
cmake --build build --target fix-format
# run standalone
./build/standalone/KitGenBench --help
# build docs
cmake --build build --target GenerateDocs
```

### Additional tools

The test and standalone subprojects include the [tools.cmake](cmake/tools.cmake) file which is used to import additional tools on-demand through CMake configuration arguments.
The following are currently supported.

#### Sanitizers

Sanitizers can be enabled by configuring CMake with `-DUSE_SANITIZER=<Address | Memory | MemoryWithOrigins | Undefined | Thread | Leak | 'Address;Undefined'>`.

#### Static Analyzers

Static Analyzers can be enabled by setting `-DUSE_STATIC_ANALYZER=<clang-tidy | iwyu | cppcheck>`, or a combination of those in quotation marks, separated by semicolons.
By default, analyzers will automatically find configuration files such as `.clang-format`.
Additional arguments can be passed to the analyzers by setting the `CLANG_TIDY_ARGS`, `IWYU_ARGS` or `CPPCHECK_ARGS` variables.

#### Ccache

Ccache can be enabled by configuring with `-DUSE_CCACHE=<ON | OFF>`.
