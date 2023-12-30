# notifier
Simple post notifier which uses curl library under MIT licence.

# Project description
This project contains header only implementation of HTTP notification client.  It is written in modern c++23  and is based on curl library. It works with g++-13 or later.  


# Install/run

1. Firstly, remember that this repository contains submodules and thus use `git clone --recursive ....`
when cloning it. Otherwise update recursively submodules with `git submodule update --recursive`

2. Secondly, before building anything you need to change `./external/curl/src/CMakeLists.txt` and change two occurences of `add_executable` to `add_library`.


3. As was mentioned above it is header only so all you need to do is include notifier.hpp into your project and that's all! However, note that some modern compiler is required (e.g. gcc13) and standard c++23 should be used.
In order to compile and run example from main.cpp just run `./scripts/make_debug.sh && ./scripts/run_debug.sh --url <your_url>` (or `./scripts/make_release.sh && ./scripts/run_release.sh --url <your_url>` for the release mode).
To see available flags for notifier run `./scripts/run_debug.sh --help`.

4. The example program is contained in `main.cpp` file.

# Remarks
Any improvements, suggestions or advice are always appreciated.