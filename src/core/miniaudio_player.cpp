#include "seraphbot/core/miniaudio_player.hpp"

#include <filesystem>

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include "seraphbot/core/logging.hpp"

sbot::core::MiniaudioPlayer::MiniaudioPlayer() {
  LOG_CONTEXT("MiniaudioPlayer");
  LOG_INFO("Initializing");
}

sbot::core::MiniaudioPlayer::~MiniaudioPlayer() {
  LOG_CONTEXT("MiniaudioPlayer");
  LOG_INFO("Shutting down");
  shutdown();
}

auto sbot::core::MiniaudioPlayer::initialize() -> bool {
  if (m_initialized) {
    LOG_WARN("Already initialized");
    return true;
  }

  m_engine = new ma_engine();

  ma_result result = ma_engine_init(nullptr, m_engine);
  if (result != MA_SUCCESS) {
    LOG_ERROR("Failed to initialized miniaudio engine: {}",
              static_cast<int>(result));
    delete m_engine;
    m_engine = nullptr;
    return false;
  }

  m_initialized = true;
  LOG_INFO("Initialized successfully");
  return true;
}

auto sbot::core::MiniaudioPlayer::shutdown() -> void {
  if (!m_initialized) {
    return;
  }

  if (m_engine) {
    ma_engine_uninit(m_engine);
    delete m_engine;
    m_engine = nullptr;
  }

  m_initialized = false;
}

auto sbot::core::MiniaudioPlayer::playSound(
    const std::filesystem::path &filepath, int volume) -> bool {
  if (!m_initialized || !m_engine) {
    LOG_ERROR("Audio player not initialized");
    return false;
  }

  if (!std::filesystem::exists(filepath)) {
    LOG_ERROR("Audio file does not exist: {}", filepath.string());
    return false;
  }

  volume = std::clamp(volume, 0, 100);
  float vol = volumeToFloat(volume) * m_master_volume;

  LOG_INFO("Playing sound: {} at volume {} (master: {})",
           filepath.filename().string(), volume,
           floatToVolume(m_master_volume));

  ma_result result =
      ma_engine_play_sound(m_engine, filepath.string().c_str(), nullptr);

  if(result != MA_SUCCESS) {
    LOG_ERROR("Failed to play sound {}: {}", filepath.string(),
              static_cast<int>(result));
    return false;
  }

  ma_engine_set_volume(m_engine, vol);

  LOG_DEBUG("Sound queued successfully");
  return true;
}

auto sbot::core::MiniaudioPlayer::setMasterVolume(int volume) -> void {
  volume = std::clamp(volume, 0, 100);
  m_master_volume = volumeToFloat(volume);

  if (m_engine) {
    ma_engine_set_volume(m_engine, m_master_volume);
  }

  LOG_INFO("Master volume set to: {}", volume);
}

auto sbot::core::MiniaudioPlayer::getMasterVolume() const -> int {
  return floatToVolume(m_master_volume);
}

auto sbot::core::MiniaudioPlayer::stopAll() -> void {
  if (!m_initialized || !m_engine) {
    return;
  }

  ma_engine_stop(m_engine);
  LOG_INFO("Stopped all sounds");
}

auto sbot::core::MiniaudioPlayer::volumeToFloat(int volume) -> float {
  volume = std::clamp(volume, 0, 100);
  return static_cast<float>(volume) / 100.0F;
}

auto sbot::core::MiniaudioPlayer::floatToVolume(float volume) -> int {
  volume = std::clamp(volume, 0.0F, 1.0F);
  return static_cast<int>(volume * 100.0F);
}
