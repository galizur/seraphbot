#ifndef SBOT_VIEWMODELS_AUTH_VIEWMODEL_HPP
#define SBOT_VIEWMODELS_AUTH_VIEWMODEL_HPP

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <string>

#include "seraphbot/core/connection_manager.hpp"
#include "seraphbot/core/twitch_service.hpp"

namespace sbot::viewmodels {

// TODO: Consider caching expensive operations instead of calling them each
// time.
struct AuthVM {
  sbot::core::TwitchService &tw_service;
  sbot::core::ConnectionManager &connection;
  sbot::core::TwitchService::State state{};

  AuthVM(sbot::core::TwitchService &tw_s, sbot::core::ConnectionManager &c)
      : tw_service(tw_s), connection(c) {}

  auto syncFrom() -> void { state = tw_service.getState(); }
  auto getState() const -> sbot::core::TwitchService::State { return state; }

  auto login() -> void { tw_service.startLogin(); }
  [[nodiscard]] auto currentUser() const -> std::string {
    return tw_service.getCurrentUser();
  }
  auto connect() -> void {
    boost::asio::co_spawn(*connection.getIoContext(),
                          tw_service.connectToChat(), boost::asio::detached);
  }
  auto disconnect() -> void { tw_service.disconnect(); }
};

} // namespace sbot::viewmodels

#endif
