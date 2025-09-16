#ifndef SBOT_VIEWMODELS_DISCORD_VIEWMODEL_HPP
#define SBOT_VIEWMODELS_DISCORD_VIEWMODEL_HPP

#include <boost/asio/awaitable.hpp>
#include <string>

#include "seraphbot/discord/notifications.hpp"

namespace sbot::viewmodels {

struct DiscordVM {
  sbot::discord::Notifications &notifications;
  char webhook_url[512] = "";

  DiscordVM(sbot::discord::Notifications &d) : notifications(d) {}

  auto syncTo() -> void { notifications.setWebhookUrl(webhook_url); }
  [[nodiscard]] auto syncFrom() const -> std::string {
    return notifications.getWebhookUrl();
  }
  auto testMessage() -> boost::asio::awaitable<void> {
    co_await notifications.sendMessage("We are live!");
  }
};

} // namespace sbot::viewmodels

#endif
