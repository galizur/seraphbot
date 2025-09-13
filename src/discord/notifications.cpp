#include "seraphbot/discord/notifications.hpp"

#include <boost/asio/awaitable.hpp>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <memory>
#include <nlohmann/json.hpp>
#include <sstream>
#include <utility>

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
    std::string webhook_url)
    : m_connection{std::move(connection)},
      m_webhook_url{std::move(webhook_url)} {
  LOG_CONTEXT("Discord Notifications");
}

auto sbot::discord::Notifications::sendMessage(std::string message)
    -> boost::asio::awaitable<void> {
  m_webhook_url = "";

  nlohmann::json payload = {
      {"content", "<@&1267602696706199703> " + message},
      {"embeds",
       {{{"title", "🔴 Stream is live!"},
         {"type", "rich"},
         {"url", "https://www.twitch.tv/lagizur"},
         {"description", "Playing some game (not implemented yet!)"},
         {"color", 9442302},
         {"timestamp", getIsoTimestamp()}}}           }
  };

  std::string resp = co_await tw::httpsPostAsync(
      m_connection, "discord.com", "443", m_webhook_url, payload.dump(), {});

  LOG_INFO("Discord response: {}", resp);
}
