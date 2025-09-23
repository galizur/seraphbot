#include "seraphbot/tw/chat/read.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <utility>

#include "seraphbot/core/logging.hpp"
#include "seraphbot/tw/eventsub.hpp"

namespace {
namespace asio = boost::asio;
using tcp      = asio::ip::tcp;
using json     = nlohmann::json;
} // namespace

sbot::tw::chat::Read::Read(std::shared_ptr<sbot::tw::EventSub> eventsub)
    : m_eventsub{std::move(eventsub)} {
  LOG_CONTEXT("Twitch Chat Read");
  LOG_INFO("Initializing");
}

sbot::tw::chat::Read::~Read() {
  LOG_CONTEXT("Twitch Chat Read");
  LOG_INFO("Shutting down"); }


auto sbot::tw::chat::Read::requestSubscription() -> asio::awaitable<void> {
  co_await m_eventsub->subscribe("channel.chat.message", "1");
  LOG_INFO("Chat Read subscription completed");
}
