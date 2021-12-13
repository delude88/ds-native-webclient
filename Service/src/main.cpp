

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
#include "auth_cli.h"
//#include "RemoteAuthService.h"
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

bool is_running = false;

void sig_handler(int s) {
  printf("Caught signal %d\n", s);
  is_running = false;
}

int main(int, char *[]) {
  static plog::ConsoleAppender<plog::TxtFormatter> console_appender;
  plog::init(plog::debug, &console_appender);

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
  auto api_client = std::make_shared<DigitalStage::Api::Client>(API_URL);
  auto client = std::make_unique<Client>(api_client);

  // Describe this device
  nlohmann::json initial_device_information;
  // - always use an UUID when you want Digital Stage to remember this device and its settings
  initial_device_information["uuid"] = device_id;
  initial_device_information["type"] = "native";
  initial_device_information["canAudio"] = true;
  initial_device_information["canVideo"] = false;
  initial_device_information["sendAudio"] = false;
  initial_device_information["receiveAudio"] = true;
#ifdef USE_RT_AUDIO
  initial_device_information["audioEngine"] = "rtaudio";
#else
  initialDeviceInformation["audioEngine"] = "miniaudio";
#endif

  // And finally connect with the token and device description
  api_client->connect(token, initial_device_information);

  // For debug only, show discovered peers
  for (const auto &item: discovery->scan()) {
    auto ip_and_port = item.ip_port();
    std::cout << "Found other client in local network at " << ip_and_port.ip() << std::endl;
  }

  is_running = true;
  while (is_running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return 0;
}