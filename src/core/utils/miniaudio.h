//
// Created by Tobias Hegemann on 20.10.21.
//

#ifndef LIBDS_EXAMPLE_SRC_UTILS_MINIAUDIO_H_
#define LIBDS_EXAMPLE_SRC_UTILS_MINIAUDIO_H_

#include "miniaudio.h"

/**
 * The direction of the strings has to be kept in sync with ma_backend
 */
static const char *backend_str[] =
    {"WASAPI", "DirectSound", "WinMM", "CoreAudio", "sndio", "audio", "OSS", "PulseAudio", "ALSA", "JACK", "AAudio",
     "OpenSL", "WebAudio", "Custom", "Null"};

/**
 * Convert the given ma_backend value into a string representation
 * @param backend
 * @return string representation
 */
static std::string convert_backend_to_string(ma_backend backend) {
  return backend_str[backend];
}

/**
 * Convert the string representation of an backend into a ma_backend value
 * @param str
 * @return ma_backend value
 */
static ma_backend convert_string_to_backend(const std::string &str) {
  for (int i = 0; i < sizeof(backend_str); i++) {
    if (backend_str[i] == str) {
      return static_cast<ma_backend>(i);
    }
  }
  return ma_backend_null;
}

/**
 * Determine possible sample rates
 * @return array of sample rates
 */
static std::vector<ma_uint32> get_sample_rates(ma_device_info device_info) {
  std::vector<ma_uint32> v = {22050, 44100, 48000, 88200, 96000, 176400, 192000};
  if (device_info.minSampleRate == 0 && device_info.maxSampleRate == 0) {
    // All are supported
    return v;
  }
  v.erase(
      std::remove_if(
          v.begin(), v.end(),
          [device_info](const int &x) {
            return x < device_info.minSampleRate || x > device_info.maxSampleRate;
          }),
      v.end()
  );
  return v;
}

static const std::string convert_device_id(ma_backend backend, ma_device_id id) {
  switch (backend) {
    case ma_backend_sndio:return id.sndio;
    case ma_backend_wasapi: {
      std::wstring ws(id.wasapi);
      std::string str(ws.begin(), ws.end());
      return str;
    }
    case ma_backend_dsound:return std::string(reinterpret_cast< char const * >(id.dsound));
    case ma_backend_winmm:return std::to_string(id.winmm);
    case ma_backend_coreaudio:return id.coreaudio;
    case ma_backend_audio4:return id.audio4;
    case ma_backend_oss:return id.oss;
    case ma_backend_pulseaudio:return id.pulse;
    case ma_backend_alsa:return id.alsa;
    case ma_backend_jack:return std::to_string(id.jack);
    case ma_backend_aaudio:return std::to_string(id.aaudio);
    case ma_backend_opensl:return std::to_string(id.opensl);
    case ma_backend_webaudio:return id.webaudio;
    case ma_backend_null:return std::to_string(id.nullbackend);
    default:return "0";
  }
}

static const ma_device_id convert_device_id(ma_backend backend, const std::string &id) {
  ma_device_id device_id;
  switch (backend) {
    case ma_backend_sndio: {
      strcpy(device_id.sndio, id.c_str());
      break;
    }
    case ma_backend_wasapi: {
      std::wstring ws(id.begin(), id.end());
      wcscpy(device_id.wasapi, ws.c_str());
      break;
    }
    case ma_backend_coreaudio: {
      strcpy(device_id.coreaudio, id.c_str());
      break;
    }
    default: {
      throw std::runtime_error("Implement me for " + convert_backend_to_string(backend));
    }
  }
  return device_id;
}

#endif //LIBDS_EXAMPLE_SRC_UTILS_MINIAUDIO_H_
