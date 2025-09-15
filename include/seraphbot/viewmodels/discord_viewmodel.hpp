#ifndef SBOT_VIEWMODELS_DISCORD_VIEWMODEL_HPP
#define SBOT_VIEWMODELS_DISCORD_VIEWMODEL_HPP

#include <string>

#include "seraphbot/discord/notifications.hpp"

namespace sbot::viewmodels {

struct DiscordVM {
  sbot::discord::Notifications &notifications;
  char webhook_url[512] = "";

  DiscordVM(sbot::discord::Notifications &d) : notifications(d) {}

  auto syncTo() -> void { notifications.setWebhookUrl(webhook_url); }

  auto readFrom() -> std::string { return notifications.getWebhookUrl(); }
};

} // namespace sbot::viewmodels

#endif
