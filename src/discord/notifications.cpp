#include "seraphbot/discord/notifications.hpp"

#include <boost/asio/awaitable.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <memory>
#include <nlohmann/json.hpp>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/core/logging.hpp"
#include "seraphbot/tw/config.hpp"

namespace {
auto getIsoTimestamp() -> std::string {
  auto now    = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);

  std::ostringstream oss;
  oss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}
} // namespace

sbot::discord::Notifications::Notifications(
    std::shared_ptr<sbot::core::ConnectionManager> connection,
    sbot::tw::ClientConfig &cfg)
    : m_connection{std::move(connection)}, m_cfg{cfg} {
  LOG_CONTEXT("Discord Notifications");
}

auto sbot::discord::Notifications::sendMessage(std::string message)
    -> boost::asio::awaitable<void> {
  std::string url =
      "/helix/streams?user_id=" + m_cfg.broadcaster_id + "&type=all";

  std::string token = tw::stripOauthPrefix(m_cfg.access_token);
  std::vector<std::pair<std::string, std::string>> headers{
      {"Client-Id",     m_cfg.client_id  },
      {"Authorization", "Bearer " + token}
  };
  for (const auto &h : headers) {
    LOG_DEBUG("Header: {}: {}", h.first, h.second);
  }

  std::string response = co_await tw::httpsGetAsync(
      m_connection, m_cfg.helix_host, m_cfg.helix_port, url, headers);
  nlohmann::json info = nlohmann::json::parse(response);

  LOG_WARN("=====Stream Info=====\n{}", info.dump());
  if (!info.contains("data") || info["data"].empty()) {
    co_return;
  }
  auto data = info["data"][0];

  std::string thumbnail_url = data.value("thumbnail_url", "");
  thumbnail_url =
      std::regex_replace(thumbnail_url, std::regex(R"(\{width\})"), "1920");
  thumbnail_url =
      std::regex_replace(thumbnail_url, std::regex(R"(\{height\})"), "1080");

  std::string url_games = "/helix/games?id=" + std::string(data.at("game_id"));

  response.clear();
  response = co_await tw::httpsGetAsync(m_connection, m_cfg.helix_host,
                                        m_cfg.helix_port, url_games, headers);
  nlohmann::json games = nlohmann::json::parse(response);

  LOG_WARN("=======Game Info=====\n{}", games.dump());
  if (!games.contains("data") || games["data"].empty()) {
    co_return;
  }
  auto game_data = games["data"][0];

  std::string thumbnail_games = game_data.value("box_art_url", "");
  thumbnail_games =
      std::regex_replace(thumbnail_url, std::regex(R"(\{width\})"), "285");
  thumbnail_games =
      std::regex_replace(thumbnail_url, std::regex(R"(\{height\})"), "380");

  nlohmann::json payload = {
      {"content", "<@&1267602696706199703> " + message                 },
      {"embeds",
       {{{"title", "🔴 Stream is live!"},
         {"type", "rich"},
         {"url", "https://www.twitch.tv/lagizur"},
         {"description", data.at("title")},
         {"image", thumbnail_url},
         {"thumbnail", game_data.at("box_art_url")},
         {"color", 9442302},
         {"timestamp", getIsoTimestamp()},
         {"footer", "Sent by SeraphBot"},
         {"fields",
          {{{"name", "Game Name"}, {"value", data.at("game_name")}}}}}}}
  };

  std::string resp = co_await tw::httpsPostAsync(
      m_connection, "discord.com", "443", m_webhook_url, payload.dump(), {});

  LOG_INFO("Discord response: {}", resp);
}
