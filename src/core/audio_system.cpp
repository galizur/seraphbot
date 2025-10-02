#include "seraphbot/core/audio_system.hpp"
#include "seraphbot/core/logging.hpp"
#include "seraphbot/core/miniaudio_player.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

sbot::core::DirectoryWhitelist::DirectoryWhitelist() {
  LOG_CONTEXT("DirectoryWhitelist");
  addWhitelistedDirectory("./sounds");
  addWhitelistedDirectory("./audio");
  addWhitelistedDirectory("./sfx");

  LOG_INFO("Initialized with default categories");
}

auto sbot::core::DirectoryWhitelist::addWhitelistedDirectory(
    const std::filesystem::path &directory) -> void {
  try {
    auto canonical         = std::filesystem::weakly_canonical(directory);
    std::string normalized = normalizePath(canonical);
    m_whitelisted_paths.insert(normalized);

    LOG_INFO("Added whitelisted directory: {} -> {}", directory.string(),
             normalized);
  } catch (const std::exception &err) {
    LOG_WARN("Failed to add whitelisted directory {}: {}", directory.string(),
             err.what());
  }
}

auto sbot::core::DirectoryWhitelist::isPathAllowed(
    const std::filesystem::path &path) const -> bool {
  try {
    auto canonical         = std::filesystem::weakly_canonical(path);
    std::string normalized = normalizePath(canonical);

    for (const auto &whitelisted : m_whitelisted_paths) {
      if (normalized.starts_with(whitelisted)) {
        LOG_DEBUG("Path {} allowed (matches whitelist: {})", normalized,
                  whitelisted);
        return true;
      }
    }
    LOG_WARN("Path {} not allowed (not in whitelist)", normalized);
    return false;
  } catch (const std::exception &err) {
    LOG_ERROR("Error checking path {}: {}", path.string(), err.what());
    return false;
  }
}

auto sbot::core::DirectoryWhitelist::getWhitelistedDirectories() const
    -> std::vector<std::filesystem::path> {
  std::vector<std::filesystem::path> result;
  result.reserve(m_whitelisted_paths.size());

  for (const auto &path : m_whitelisted_paths) {
    result.emplace_back(path);
  }

  return result;
}

auto sbot::core::DirectoryWhitelist::normalizePath(
    const std::filesystem::path &path) const -> std::string {
  std::string result = path.string();
  std::replace(result.begin(), result.end(), '\\', '/');

  if (!result.empty() && result.back() != '/') {
    result += '/';
  }

  return result;
}

sbot::core::AudioSystem::AudioSystem()
    : m_player{std::make_unique<MiniaudioPlayer>()} {
  LOG_CONTEXT("AudioSystem");
  if (!m_player->initialize()) {
    LOG_ERROR("Failed to initialized MiniaudioPlayer");
  } else {
    LOG_INFO("Initialized with MiniaudioPlayer");
  }
  LOG_INFO("Initialized");
}

sbot::core::AudioSystem::~AudioSystem() {
  LOG_CONTEXT("AudioSystem");
  LOG_INFO("Shutting Down");
}

auto sbot::core::AudioSystem::listAudioFiles(
    const std::filesystem::path &directory) -> std::vector<std::string> {
  if (!m_whitelist.isPathAllowed(directory)) {
    LOG_WARN("Directory {} not in whitelist", directory.string());
    return {};
  }

  if (!std::filesystem::exists(directory)) {
    LOG_WARN("Directory {} does not exist", directory.string());
    return {};
  }

  if (!std::filesystem::is_directory(directory)) {
    LOG_WARN("Path {} is not a directory", directory.string());
    return {};
  }

  std::vector<std::string> audio_files;

  try {
    for (const auto &entry : std::filesystem::directory_iterator(directory)) {
      if (entry.is_regular_file() && isSupportedAudioFile(entry.path())) {
        std::string filename = entry.path().stem().string();
        audio_files.push_back(filename);
      }
    }

    std::sort(audio_files.begin(), audio_files.end());

    LOG_INFO("Found {} audio files in {}", audio_files.size(),
             directory.string());
  } catch (const std::exception &err) {
    LOG_ERROR("Error listing audio files in {}: {}", directory.string(),
              err.what());
  }

  return audio_files;
}

auto sbot::core::AudioSystem::findAudioFile(
    const std::string &name, const std::filesystem::path &directory)
    -> std::string {
  if (!m_whitelist.isPathAllowed(directory)) {
    LOG_WARN("directory {} not in whitelist", directory.string());
    return "";
  }

  if (!std::filesystem::exists(directory)) {
    LOG_WARN("Directory {} does not exist", directory.string());
    return "";
  }

  std::vector<std::string> found_files;
  auto supported_extensions = getSupportedExtensions();

  try {
    for (const auto &ext : supported_extensions) {
      auto candidate = directory / (name + ext);
      if (std::filesystem::exists(candidate) &&
          std::filesystem::is_regular_file(candidate)) {
        found_files.push_back(candidate.string());
      }
    }
    if (found_files.empty()) {
      LOG_DEBUG("Audio file '{}' not found in {}", name, directory.string());
      return "";
    }

    if (found_files.size() > 1) {
      std::ostringstream oss;
      for (std::size_t i = 0; i < found_files.size(); ++i) {
        if (i > 0) {
          oss << ", ";
        }
        oss << std::filesystem::path(found_files[i]).filename().string();
      }
      LOG_WARN("Multiple audio files found for '{}': {}. Using first match.",
               name, oss.str());
    }
    LOG_DEBUG("Found audio file: {}", found_files[0]);
    return found_files[0];
  } catch (const std::exception &err) {
    LOG_ERROR("Error finding audio file '{}' in {}: {}", name,
              directory.string(), err.what());
    return "";
  }
}

auto sbot::core::AudioSystem::playSound(const std::filesystem::path &filepath,
                                        int volume) -> bool {
  if (!std::filesystem::exists(filepath)) {
    LOG_ERROR("Audio file does not exist: {}", filepath.string());
    return false;
  }
  if (!m_whitelist.isPathAllowed(filepath.parent_path())) {
    LOG_ERROR("Audio file path not whitelisted: {}", filepath.string());
    return false;
  }

  volume = std::clamp(volume, 0, 100);

  LOG_INFO("Playing sound: {} at volume {}", filepath.filename().string(),
           volume);

  if (!m_player || !m_player->isInitialized()) {
    LOG_ERROR("Audio player not initialized");
    return false;
  }
  return m_player->playSound(filepath, volume);
}

auto sbot::core::AudioSystem::setMasterVolume(int volume) -> void {
  if (m_player && m_player->isInitialized()) {
    m_player->setMasterVolume(volume);
  }
}

auto sbot::core::AudioSystem::getMasterVolume() const -> int {
  if (m_player && m_player->isInitialized()) {
    return m_player->getMasterVolume();
  }
  return 70;
}

auto sbot::core::AudioSystem::stopAllSounds() -> void {
  if (m_player && m_player->isInitialized()) {
    m_player->stopAll();
  }
}

auto sbot::core::AudioSystem::getSupportedExtensions()
    -> std::vector<std::string> {
  // TODO: Change from hardcoded extensions
  return {".mp3", ".wav", ".flac", ".ogg"};
}

auto sbot::core::AudioSystem::isSupportedAudioFile(
    const std::filesystem::path &path) const -> bool {
  auto extension = path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 ::tolower);

  auto supported = getSupportedExtensions();
  return std::find(supported.begin(), supported.end(), extension) !=
         supported.end();
}
