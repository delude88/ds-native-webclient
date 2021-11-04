#ifdef __APPLE__
#include "utils/macos.h"
#endif

#ifdef __APPLE__
#define MA_NO_RUNTIME_LINKING
#endif
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#ifdef MA_POSIX
#include <csignal>
#endif

#include <memory>
#include <string>
#include <deviceid/id.h>
#include <DigitalStage/Api/Client.h>
#include <plog/Init.h>
#include <plog/Formatters/TxtFormatter.h>
#include <plog/Appenders/ConsoleAppender.h>
#include "auth.cpp"
#include "Client.h"

bool isRunning = false;

void sig_handler(int s) {
  printf("Caught signal %d\n", s);
  isRunning = false;
}

int main(int, char *[]) {
  static plog::ConsoleAppender<plog::TxtFormatter> consoleAppender;
  plog::init(plog::debug, &consoleAppender);

#ifdef MA_POSIX
  // What to do for WIN32 here?
  signal(SIGINT, &sig_handler);
#endif

#ifdef __APPLE__
  if (!check_access()) {
    std::cerr << "No access" << std::endl;
    return -1;
  }
#endif

  // Usually authenticate the user first
  auto token = authenticate_user();

  // Create an API service
  auto apiClient = new DigitalStage::Api::Client(API_URL);

  auto client = std::make_unique<Client>(*apiClient);

  // Describe this device
  nlohmann::json initialDeviceInformation;
  // - always use an UUID when you want Digital Stage to remember this device and its settings
  initialDeviceInformation["uuid"] = std::to_string(deviceid::get());
  initialDeviceInformation["type"] = "native";
  initialDeviceInformation["canAudio"] = true;
  initialDeviceInformation["canVideo"] = false;

  // And finally connect with the token and device description
  apiClient->connect(token, initialDeviceInformation);

  isRunning = true;
  while (isRunning) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return 0;
}