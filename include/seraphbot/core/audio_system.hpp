#ifndef SBOT_CORE_AUDIO_SYSTEM_HPP
#define SBOT_CORE_AUDIO_SYSTEM_HPP

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

namespace sbot::core {

class DirectoryWhitelist {
public:
  DirectoryWhitelist();
  ~DirectoryWhitelist() = default;

  auto addWhitelistedDirectory(const std::filesystem::path &directory) -> void;
  [[nodiscard]] auto isPathAllowed(const std::filesystem::path &path) const
      -> bool;
  [[nodiscard]] auto getWhitelistedDirectories() const
      -> std::vector<std::filesystem::path>;

private:
  std::unordered_set<std::string> m_whitelisted_paths;

  [[nodiscard]] auto normalizePath(const std::filesystem::path &path) const
      -> std::string;
};

class AudioSystem {
public:
  AudioSystem();
  ~AudioSystem();

  auto getWhitelist() -> DirectoryWhitelist & { return m_whitelist; }
  auto listAudioFiles(const std::filesystem::path &directory)
      -> std::vector<std::string>;
  auto findAudioFile(const std::string &name,
                     const std::filesystem::path &directory) -> std::string;
  auto playSound(const std::filesystem::path &filepath, int volume = 70)
      -> bool;
  static auto getSupportedExtensions() -> std::vector<std::string>;

private:
  DirectoryWhitelist m_whitelist;

  [[nodiscard]] auto
  isSupportedAudioFile(const std::filesystem::path &path) const -> bool;
  auto executeAudioCommand(const std::string &command) -> bool;
};

} // namespace sbot::core

#endif
