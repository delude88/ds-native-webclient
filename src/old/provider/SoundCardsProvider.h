//
// Created by Tobias Hegemann on 20.10.21.
//

#ifndef LIBDS_EXAMPLE_SRC_PROVIDER_SOUNDCARDSPROVIDER_H_
#define LIBDS_EXAMPLE_SRC_PROVIDER_SOUNDCARDSPROVIDER_H_

#include "miniaudio.h"
#include <vector>
#include <nlohmann/json.hpp>
#include <DigitalStage/Api/Store.h>

class SoundCardsProvider {
 public:
  static std::vector<nlohmann::json> getSoundCards(const DigitalStage::Api::Store *);
};

nlohmann::json convert_device_to_sound_card(ma_device_info,
                                                           ma_context *,
                                                           const DigitalStage::Api::Store *,
                                                           bool is_input
);
#endif //LIBDS_EXAMPLE_SRC_PROVIDER_SOUNDCARDSPROVIDER_H_
