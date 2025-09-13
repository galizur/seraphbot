#ifndef SBOT_TW_CONFIG_HPP
#define SBOT_TW_CONFIG_HPP

#include <boost/asio/awaitable.hpp>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "seraphbot/core/connection_manager.hpp"

namespace sbot::tw {

struct ClientConfig {
  std::string host       = "eventsub.wss.twitch.tv";
  std::string port       = "443";
  std::string helix_host = "api.twitch.tv";
  std::string helix_port = "443";

  std::string client_id;
  std::string access_token;   // raw token (no "oauth:" prefix)
  std::string broadcaster_id; // user id of the channel you want events for
};

auto httpsPostAsync(std::shared_ptr<core::ConnectionManager> conn_manager,
                    std::string host, std::string port, std::string target,
                    std::string body,
                    std::vector<std::pair<std::string, std::string>> headers = {})
    -> boost::asio::awaitable<std::string>;

auto stripOauthPrefix(std::string token) -> std::string;

} // namespace sbot::tw

#endif
