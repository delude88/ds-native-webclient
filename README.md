# ds-native-webclient

Native webclient for the digital stage project, which streams audio via WebRTC

This is just a private project and not maintained or supported by digital stage, but still interacting with the
concurrent digital stage infrastructure.

Mainly this project acts as feasibility and archive for example code provided to the developer of digital stage.

## Dependencies

- cmake/3.8+
- nlohmann_json/3.9.1
- cpprestsdk/2.10.18
- openssl/1.1.1l

### macOS

To install the missing dependencies using brew:

```shell
brew install nlohmann-json cpprestsdk cmake
```

### Linux

To install the missing dependencies on Ubuntu via apt:
```shell
sudo apt-get install libcpprest-dev nlohmann-json-dev cmake
```

## Checkout

Check out this repository and fetch all submodules via:

```shell
git submodule update --init --recursive
```

### Build

Once checked out with all submodules, you can build:

```shell
cmake -B build .
cmake --build build --parallel
```

You'll find the executables then inside the build folder.