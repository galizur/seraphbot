#ifndef SBOT_CORE_CHAT_MESSAGE_HPP
#define SBOT_CORE_CHAT_MESSAGE_HPP

#include <string>
#include <vector>

namespace sbot::core {

struct ChatMessage {
  std::string user;
  std::string text;
  std::string color;
  std::vector<std::string> badges;
};

} // namespace sbot::core

#endif
