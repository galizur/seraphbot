#include "seraphbot/tw/chat/read.hpp"

#include <boost/asio/awaitable.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <utility>

#include "seraphbot/core/logging.hpp"
#include "seraphbot/tw/eventsub.hpp"

namespace {
namespace asio  = boost::asio;
namespace beast = boost::beast;
namespace http  = beast::http;
namespace ws    = beast::websocket;
namespace ssl   = asio::ssl;
using tcp       = asio::ip::tcp;
using json      = nlohmann::json;
} // namespace

sbot::tw::chat::Read::Read(std::shared_ptr<sbot::tw::EventSub> eventsub)
    : m_eventsub{std::move(eventsub)} {
  LOG_CONTEXT("Twitch Chat Read");
  LOG_TRACE("Initializing");
}

sbot::tw::chat::Read::~Read() { LOG_TRACE("Shutting down"); }

auto sbot::tw::chat::Read::request() -> asio::awaitable<void> {
  json condition = {
      {"broadcaster_user_id", m_eventsub->getTwitchConfig().broadcaster_id},
      {"user_id",             m_eventsub->getTwitchConfig().broadcaster_id}
  };

  json transport = {
      {"method",     "websocket"               },
      {"session_id", m_eventsub->getSessionId()}
  };

  // Subscript to channel.chat.notification
  json payload = {
      {"type",      "channel.chat.message"},
      {"version",   "1"                   },
      {"condition", condition             },
      {"transport", transport             }
  };
  co_await m_eventsub->subscribe(payload);
  LOG_INFO("Chat Read subscription completed");
}
