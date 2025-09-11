#ifndef SBOT_CORE_CHAT_MESSAGE_HPP
#define SBOT_CORE_CHAT_MESSAGE_HPP

#include <string>

namespace sbot::core {

struct ChatMessage {
  std::string user;
  std::string text;
  std::string color;
};

} // namespace sbot::core

#endif
