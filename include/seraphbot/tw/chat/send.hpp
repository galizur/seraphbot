#ifndef SBOT_TW_CHAT_SEND_HPP
#define SBOT_TW_CHAT_SEND_HPP

#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/tw/config.hpp"

#include <boost/asio/awaitable.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sbot::tw::chat {

class Send {
public:
  Send(std::shared_ptr<core::ConnectionManager> conn_manager, ClientConfig cfg);
  ~Send();

  auto message(const std::string &msg, const std::string &sender_id)
      -> boost::asio::awaitable<void>;

private:
  std::shared_ptr<core::ConnectionManager> m_conn_manager;
  ClientConfig m_cfg;
};

} // namespace sbot::tw::chat

#endif
