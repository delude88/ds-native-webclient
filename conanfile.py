from conans import ConanFile, CMake


class DigitalStageConnectorConan(ConanFile):
    settings = ["os", "compiler", "build_type", "arch"]
    requires = ["nlohmann_json/3.10.4", "sigslot/1.2.1", "libdeflate/1.8", "zlib/1.2.11", "eigen/3.4.0", "cereal/1.3.0"]
    generators = "cmake", "cmake_find_package", "json"

    def configure(self):
        if self.settings.os == "Windows":
            self.requires.add("mbedtls/3.0.0")
        if self.settings.os == "Linux":
            self.requires.add("openssl/1.1.1l")
