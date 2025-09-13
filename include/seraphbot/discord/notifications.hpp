#ifndef SBOT_DISCORD_NOTIFICATIONS_HPP
#define SBOT_DISCORD_NOTIFICATIONS_HPP

#include "seraphbot/core/connection_manager.hpp"
#include <boost/asio/awaitable.hpp>
#include <memory>
#include <string>

namespace sbot::discord {

class Notifications {
public:
  Notifications(std::shared_ptr<core::ConnectionManager> connection,
                std::string webhook_url);

  auto sendMessage(std::string message) -> boost::asio::awaitable<void>;

private:
  std::shared_ptr<core::ConnectionManager> m_connection;
  std::string m_webhook_url;
};

} // namespace sbot::discord

#endif
