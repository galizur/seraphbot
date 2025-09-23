#ifndef SBOT_VIEWMODELS_CHAT_VIEWMODEL_HPP
#define SBOT_VIEWMODELS_CHAT_VIEWMODEL_HPP

#include <string>

#include "seraphbot/core/twitch_service.hpp"

namespace sbot::viewmodels {

struct ChatVM {
  sbot::core::TwitchService &tw_service;

  ChatVM(sbot::core::TwitchService &tw) : tw_service(tw) {}

  auto canSendMessages() -> bool { return tw_service.canSendMessages(); }
  auto sendMessage(std::string msg) -> void { tw_service.sendMessage(msg); }
};

} // namespace sbot::viewmodels

#endif
