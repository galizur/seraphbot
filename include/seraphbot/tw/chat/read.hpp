#ifndef SBOT_TW_CHAT_READ_HPP
#define SBOT_TW_CHAT_READ_HPP

#include <boost/asio/awaitable.hpp>
#include <memory>

namespace sbot::tw {
class EventSub;
} // namespace sbot::tw

namespace sbot::tw::chat {

class Read {
public:
  Read(std::shared_ptr<EventSub> eventsub);
  ~Read();

  auto requestSubscription() -> boost::asio::awaitable<void>;

private:
  std::shared_ptr<EventSub> m_eventsub;
};

} // namespace sbot::tw::chat

#endif
