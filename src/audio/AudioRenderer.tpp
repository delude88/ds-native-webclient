//
// Created by Tobias Hegemann on 04.11.21.
//

template<typename T>
AudioRenderer<T>::AudioRenderer(DigitalStage::Api::Client &client) {
}
template<typename T>
std::optional<std::pair<T, T>> AudioRenderer<T>::getRenderValue(const std::string &audio_track_id) const {
  if (render_map_.count(audio_track_id)) {
    return render_map_[audio_track_id];
  }
  return std::nullopt;
}
