#include "seraphbot/tw/chat/send.hpp"

#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/core/logging.hpp"
#include "seraphbot/tw/config.hpp"

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/http/status.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

namespace {
namespace asio = boost::asio;
using tcp      = asio::ip::tcp;
using json     = nlohmann::json;
} // namespace

sbot::tw::chat::Send::Send(
    std::shared_ptr<core::ConnectionManager> conn_manager, ClientConfig cfg)
    : m_conn_manager{std::move(conn_manager)}, m_cfg{std::move(cfg)} {
  LOG_CONTEXT("Twitch Chat Send");
  LOG_TRACE("Initializing");
}

sbot::tw::chat::Send::~Send() { LOG_TRACE("Destructor called"); }

auto sbot::tw::chat::Send::message(std::string msg, std::string sender_id)
    -> asio::awaitable<void> {
  // Helix chat/messages body
  json body = {
      {"broadcaster_id", m_cfg.broadcaster_id},
      {"message",        msg                 },
      {"sender_id",      sender_id           }
  };

  std::string token = stripOauthPrefix(m_cfg.access_token);
  std::vector<std::pair<std::string, std::string>> headers = {
      {"Client-Id",     m_cfg.client_id  },
      {"Authorization", "Bearer " + token}
  };

  std::string resp = co_await httpsPostAsync(
      m_conn_manager, m_cfg.helix_host, m_cfg.helix_port,
      "/helix/chat/messages", body.dump(), headers);

  LOG_INFO("[Helix chat/send] resp: {}", resp);
}
