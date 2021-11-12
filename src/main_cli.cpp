// AudioIO engine
#ifdef USE_RT_AUDIO
#include <audio/RtAudioIO.h>
#else
// Miniaudio
#ifdef __APPLE__
#define MA_NO_RUNTIME_LINKING
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include <audio/MiniAudioIO.h>
#endif

#if defined (__unix__) || (defined (__APPLE__) && defined (__MACH__))
#define POSIX
#endif

// Std lib
#include <memory>
#include <string>

// Libds
#include <DigitalStage/Api/Client.h>

// Device identification
#include <deviceid/id.h>

// Local
#include "cli/auth_cli.h"
#include "cli/RemoteAuthService.h"
#include <Client.h>
#include <utils/ServiceDiscovery.h>

// Logger
#include <plog/Init.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ConsoleAppender.h>

// Signal handling (posix-only)
#ifdef POSIX
#include <csignal>
#endif

// Resource management
#include <cmrc/cmrc.hpp>
CMRC_DECLARE(clientres);

bool isRunning = false;

void sig_handler(int s) {
  printf("Caught signal %d\n", s);
  isRunning = false;
}

int main(int, char *[]) {
  static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
  plog::init(plog::warning, &consoleAppender);

#ifdef POSIX
  // What to do for WIN32 here?
  signal(SIGINT, &sig_handler);
#endif

  // Fetch unique deviceid
  const auto device_id = std::to_string(deviceid::get());

  // Use service discovery
  auto discovery = std::make_unique<ServiceDiscovery>(device_id, true);

  //auto auth_service = std::make_unique<RemoteAuthService>();
  //auth_service->start();

  // Usually authenticate the user first
  auto token = authenticate_user(); //TODO: Remove

  // Create an API service
  auto apiClient = std::make_unique<DigitalStage::Api::Client>(API_URL);
#if USE_RT_AUDIO
  auto audioIO = std::make_unique<RtAudioIO>(*apiClient);
#else
  auto audioIO = std::make_unique<MiniAudioIO>(*apiClient);
#endif
  auto client = std::make_unique<Client>(*apiClient, *audioIO);

  // Describe this device
  nlohmann::json initialDeviceInformation;
  // - always use an UUID when you want Digital Stage to remember this device and its settings
  initialDeviceInformation["uuid"] = device_id;
  initialDeviceInformation["type"] = "native";
  initialDeviceInformation["canAudio"] = true;
  initialDeviceInformation["canVideo"] = false;

  // And finally connect with the token and device description
  apiClient->connect(token, initialDeviceInformation);

  // For debug only, show discovered peers
  auto peers = discovery->scan();
  for (const auto &item: peers) {
    auto ip_and_port = item.ip_port();
    std::cout << "Found other client in local network at " << ip_and_port.ip() << std::endl;
  }

  isRunning = true;
  while (isRunning) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return 0;
}