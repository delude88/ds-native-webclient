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

## macOS

Clone this repository and enter it:

```shell
git clone https://github.com/delude88/ds-native-webclient.git
cd ds-native-webclient
```

Then proceed either using conan or not:

### Using conan

Install conan if you dont' own it yet:

```shell
pip3 install wheel setuptools
pip3 install conan
  ```

Install the missing dependencies:

```shell
git submodule update --init --recursive
conan install -if build .
```

And build:

```shell
cmake -S . -B build
cmake --build build --parallel
```

### Without conan

Install the missing dependencies using brew:

```shell
brew install nlohmann-json cpprestsdk cmake
```

And build:

```shell
cmake -S . -B build
cmake --build build --parallel
```

## Linux

Clone this repository and enter it:

```shell
git clone https://github.com/delude88/ds-native-webclient.git
cd ds-native-webclient
```

Then proceed either using conan or not:

### Using conan

Install conan if you don't own it yet:

```shell
sudo apt-get install python3-pip
pip3 install wheel setuptools
pip3 install conan
  ```

Install the missing dependencies:

```shell
sudo apt-get install libssl-dev libsrtp2-dev
git submodule update --init --recursive
conan install -if build .
```

And build:

```shell
cmake -S . -B build
cmake --build build --parallel
```

### Without conan

Install the missing dependencies via apt:

```shell
sudo apt-get install libssl-dev libsrtp2-dev libcpprest-dev nlohmann-json-dev cmake
```

Install local dependencies using submodules:

```shell
git submodule update --init --recursive
```

And build:

```shell
cmake -S . -B build
cmake --build build --parallel
```
