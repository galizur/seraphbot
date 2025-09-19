#ifndef SBOT_VIEWMODELS_DISCORD_VIEWMODEL_HPP
#define SBOT_VIEWMODELS_DISCORD_VIEWMODEL_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_future.hpp>
#include <string>

#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/core/logging.hpp"
#include "seraphbot/discord/notifications.hpp"

namespace sbot::viewmodels {

struct DiscordVM {
  sbot::core::ConnectionManager &connection;
  sbot::discord::Notifications &notifications;
  char webhook_url[512] = "";

  DiscordVM(sbot::core::ConnectionManager &c, sbot::discord::Notifications &d)
  : connection(c), notifications(d)  {}

  auto syncTo(const std::string &url) -> void {
    notifications.setWebhookUrl(url);
  }
  [[nodiscard]] auto syncFrom() const -> std::string {
    return notifications.getWebhookUrl();
  }
  auto testMessage(const std::string &message) -> void {
    auto future = boost::asio::co_spawn(*connection.getIoContext(),
                                        notifications.sendMessage(message),
                                        boost::asio::use_future);
    try {
      future.get();
      LOG_INFO("Discord message sent successfully");
    } catch (const std::exception &err) {
      LOG_ERROR("Discord send failed: {}", err.what());
    }
  }
};

} // namespace sbot::viewmodels

#endif
