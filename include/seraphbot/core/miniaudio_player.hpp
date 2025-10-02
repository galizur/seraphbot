#ifndef SBOT_CORE_MINIAUDIO_PLAYER_HPP
#define SBOT_CORE_MINIAUDIO_PLAYER_HPP

#include <filesystem>
using ma_engine = struct ma_engine;

namespace sbot::core {

class MiniaudioPlayer {
public:
  MiniaudioPlayer();
  ~MiniaudioPlayer();

  MiniaudioPlayer(const MiniaudioPlayer &)                     = delete;
  auto operator=(const MiniaudioPlayer &) -> MiniaudioPlayer & = delete;

  auto initialize() -> bool;
  auto shutdown() -> void;
  [[nodiscard]] auto isInitialized() const -> bool { return m_initialized; }
  auto playSound(const std::filesystem::path &filepath, int volume = 70)
      -> bool;
  auto setMasterVolume(int volume) -> void;
  [[nodiscard]] auto getMasterVolume() const -> int;
  auto stopAll() -> void;

private:
  ma_engine *m_engine{nullptr};
  float m_master_volume{0.7F};
  bool m_initialized{false};

  [[nodiscard]] static auto volumeToFloat(int volume) -> float;
  [[nodiscard]] static auto floatToVolume(float volume) -> int;
};

} // namespace sbot::core

#endif
