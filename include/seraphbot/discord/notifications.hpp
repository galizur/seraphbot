#ifndef SBOT_DISCORD_NOTIFICATIONS_HPP
#define SBOT_DISCORD_NOTIFICATIONS_HPP

#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/tw/config.hpp"
#include <boost/asio/awaitable.hpp>
#include <memory>
#include <string>
#include <utility>

namespace sbot::discord {

class Notifications {
public:
  Notifications(std::shared_ptr<core::ConnectionManager> connection,
                tw::ClientConfig &cfg);

  auto sendMessage(std::string message) -> boost::asio::awaitable<void>;

  [[nodiscard]] auto getWebhookUrl() const -> std::string {
    return m_webhook_url;
  }
  auto setWebhookUrl(std::string webhook_url) -> void {
    m_webhook_url = std::move(webhook_url);
  }

private:
  std::shared_ptr<core::ConnectionManager> m_connection;
  tw::ClientConfig &m_cfg;
  std::string m_webhook_url;
};

} // namespace sbot::discord

#endif
