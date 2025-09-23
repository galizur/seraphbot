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

  std::string stream_title{"Stream Notification"};
  std::string game_name{"Just Chatting"};
  std::string thumbnail_url{""};
  std::string game_thumbnail{""};
  std::string twitch_url{"https://www.twitch.tv/"};

  if (m_cfg.broadcaster_id.empty()) {
    twitch_url += "channel_name";
  } else {
    twitch_url += m_cfg.broadcaster_id;
  }

  try {
    std::string response = co_await tw::httpsGetAsync(
        m_connection, m_cfg.helix_host, m_cfg.helix_port, url, headers);
    nlohmann::json info = nlohmann::json::parse(response);

    LOG_DEBUG("=====Stream Info=====\n{}", info.dump());
    if (info.contains("data") || !info["data"].empty()) {
      auto data     = info["data"][0];
      stream_title  = data.value("title", "Live Stream");
      game_name     = data.value("game_name", "Just Chatting");
      thumbnail_url = data.value("thumbnail_url", "");
      if (!thumbnail_url.empty()) {
        thumbnail_url = std::regex_replace(thumbnail_url,
                                           std::regex(R"(\{width\})"), "1920");
        thumbnail_url = std::regex_replace(thumbnail_url,
                                           std::regex(R"(\{height\})"), "1080");
      }

      if (data.contains("game_id") && !std::string(data["game_id"]).empty()) {
        std::string url_games =
            "/helix/games?id=" + std::string(data.at("game_id"));

        try {
          std::string game_response =
              co_await tw::httpsGetAsync(m_connection, m_cfg.helix_host,
                                         m_cfg.helix_port, url_games, headers);
          nlohmann::json games = nlohmann::json::parse(game_response);

          LOG_DEBUG("=======Game Info=====\n{}", games.dump());
          if (games.contains("data") || !games["data"].empty()) {
            auto game_data = games["data"][0];

            game_thumbnail = game_data.value("box_art_url", "");
            if (!game_thumbnail.empty()) {
              game_thumbnail = std::regex_replace(
                  game_thumbnail, std::regex(R"(\{width\})"), "285");
              game_thumbnail = std::regex_replace(
                  game_thumbnail, std::regex(R"(\{height\})"), "380");
            }
          }
        } catch (const std::exception &err) {
          LOG_WARN("Failed to fetch game info: {}", err.what());
        }
      }
    } else {
      LOG_INFO("Stream is offline, sending notification with default values");
    }
  } catch (const std::exception &err) {
    LOG_ERROR("Failed to fetch stream info: {}", err.what());
    LOG_INFO("Using placeholder values for notification");
  }

  nlohmann::json embed = {
      {"title",       "🔴 Stream is live!"                              },
      {"type",        "rich"                                            },
      {"url",         twitch_url                                        },
      {"description", stream_title                                      },
      {"color",       9442302                                           },
      {"timestamp",   getIsoTimestamp()                                 },
      {"footer",      {{"text", "Sent by SeraphBot"}}                   },
      {"fields",
       nlohmann::json::array(
           {{{"name", "Game"}, {"value", game_name}, {"inline", true}}})}
  };

  if (!thumbnail_url.empty()) {
    embed["image"] = {
        {"url", thumbnail_url}
    };
  }
  if (!game_thumbnail.empty()) {
    embed["thumbnail"] = {
        {"url", game_thumbnail}
    };
  }

  nlohmann::json payload = {
      {"content", "<@&1267602696706199703> " + message},
      {"embeds",  nlohmann::json::array({embed})      }
      // ,
      // {"components", nlohmann::json::array(
      //                    {{{"type", 1},
      //                      {"components", nlohmann::json::array(
      //                                         {{{"type", 2},
      //                                           {"label", "Watch Stream"},
      //                                           {"style", 5},
      //                                           {"url", twitch_url}}})}}})}
  };

  LOG_INFO("Payload dump: {}", payload.dump());
  try {
    std::string resp = co_await tw::httpsPostAsync(
        m_connection, "discord.com", "443", m_webhook_url, payload.dump(), {});

    LOG_INFO("Discord notification sent successfully: : {}", resp);
  } catch (const std::exception &err) {
    LOG_ERROR("Failed to send Discord notification: {}", err.what());
  }
}
