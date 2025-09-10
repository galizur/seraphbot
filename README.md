# SeraphBot

A modern C++23 Twitch bot with real-time EventSub integration, chat moderation, stream management capabilities and OAuth authentication.

**Platform Support**: Linux (primary), Windows/macOS (experimental)

## Features - In Progress

- **Real-time Chat**: EventSub WebSocket integration for live Twitch chat
- **Stream Management**: Full bot capabilities for moderating and controlling chat
- **OAuth Authentication**: Secure login via hosted server (no client secrets in binary)
- **Modern C++**: Built with C++23, Boost.Asio coroutines, and async architecture
- **Advanced Logging**: Context-aware logging with spdlog integration
- **Thread-Safe**: Async networking with thread-safe message queues

## Architecture

```
┌─────────────────┐    ┌─────────────────┐    ┌────────────────┐
│    ImGui UI     │    │  ConnectionMgr  │    │  OAuth Server  │
│  (Main Thread)  │◄──►│  (Thread Pool)  │◄──►│    (Hosted)    │
└─────────────────┘    └─────────────────┘    └────────────────┘
         ▲                       │
         │              ┌────────▼────────┐
         │              │  Twitch APIs    │
         │              │  • EventSub WS  │
         │              │  • Helix API    │
         └──────────────┤  • Auth Flow    │
                        └─────────────────┘
```

## Prerequisites

### Dependencies
- **Clang 20** (tested with 20.1.8 for C++23 and libc++ support)
- **libc++** development headers
- **GCC 15** (tested with 15.2.1 for C++23 support)
- **Meson** 1.9 (tested with 1.9)
- System package manager for dependencies

**Note**: Dependency versions listed are what's currently tested. Lower versions may work but are untested. Contributions testing with older versions are welcome.

### Required Libraries
- **Boost** (ASIO, Beast, Process)
- **OpenGL** 4.6+
- **GLFW** 3.3+
- **ImGui** (docking branch - personal repo has a meson file for easy building)
- **spdlog** 1.12+
- **nlohmann/json** 3.11+
- **OpenSSL** 3.0+

### Platform Support

**Linux**: Primary development platform with full support (specifically Arch for now)

**Windows/macOS**: Experimental support. These platforms may work if you provide appropriate native files for your toolchain. Contributions with working native files for other platforms are welcome!

#### Windows Users

**Security Warning**: The Windows executable is not code-signed, so Windows Defender and browsers may show warnings like "Windows protected your PC" or "Unrecognized app". This is normal for unsigned software.

To run the application:
1. Click "More info" on the Windows Defender popup
2. Click "Run anyway"
3. Or add an exception in Windows Defender if needed.

**Why unsigned?** Code signing certificates cost hundreds of dollars annually. This is FOSS (free open-source software), which you can see the code and build it from source to verify it's safety.

## Building

```bash
# Install dependencies via your system package manager
# Ubuntu/Debian:
sudo apt install libboost-dev libglfw3-dev libspdlog-dev nlohmann-json3-dev libssl-dev

# Arch:
sudo pacman -S boost glfw spdlog nlohmann-json openssl

# Configure and build
meson setup --native-file=debug-linux-clang.ini build
meson compile -C build
```

## Setup

The application uses a hosted OAuth server to handle Twitch authentication securely. No additional server setup is required - just run the application and authenticate through your browser.

Update the OAuth server URL if needed:
```cpp
// In main.cpp
sbot::tw::Auth twitch_auth(connection, "seraphbot-oauth-server.onrender.com");
```

**Note**: The hosted OAuth server avoids using personal information whenever possible. Currently, it does not capture any data, but it may need to store a unique ID and state if switched to long-lasting connections. Any information that does not need to be retained will be used solely for authentication and then discarded. If any data must be stored, it will be stripped of identifiable details as much as possible and kept only for as long as necessary—no longer.

## Usage

1. **Launch** the application
2. **Click "Login to Twitch"** - opens browser for OAuth
3. **Authorize** the application in your browser
4. **Click "Connect to Chat"** once authenticated
5. **Chat away!** Real-time messages appear in the interface

## Project Structure

```
seraphbot/
├── include/seraphbot/
│   ├── core/           # Core systems (logging, connection management)
│   ├── tw/             # Twitch integration (auth, chat, EventSub)
│   └── ui/             # UI components and backends
├── src/                # Implementation files
└── meson.build         # Build configuration
```

## Key Components

### ConnectionManager
- Thread pool for async I/O operations
- Shared SSL context and resolvers
- Boost.Asio integration with proper RAII cleanup

### Auth System
- OAuth 2.0 flow with hosted server
- Automatic token refresh (TODO)
- User info fetching via Twitch Helix API

### EventSub Integration
- Real-time WebSocket connection to Twitch
- Automatic subscription management
- Thread-safe message delivery to UI

### Enhanced Logging
- Context-aware logging with stack tracking
- Configurable console and file outputs
- Thread-safe with performance optimizations

## Development

### Adding Features
The async architecture makes it easy to add new functionality:
- New Twitch API endpoints in `tw/` namespace
- UI components in `ui/` namespace
- Core systems in `core/` namespace

## TODO

- [ ] Clean up code and move to proper spaces
- [ ] Clean up UI design and layout
- [ ] Token refresh mechanism
- [x] Chat message sending
- [ ] Rate limits per Twitch's guidelines
- [ ] Moderation tools (timeouts, bans, message deletion)
- [ ] Custom command system
- [ ] Automated moderation rules
- [ ] Stream alerts and notifications
- [ ] Discord integration
- [ ] OBS/SLOBS communication
- [ ] Persistent configuration
- [ ] Plugin system
- [ ] Sound notifications
- [ ] Additional platforms (YouTube, etc)
- [ ] Windows/macOS platform support
- [ ] Vulkan backend for ImGui

## Twitch Scopes Used

<details>

<summary>List of scopes used by broadcaster</summary>

- `bits:read`
- `channel:manage:ads`
- `channel:read:ads`
- `channel:manage:broadcast`
- `channel:read:charity`
- `channel:edit:commercial`
- `channel:read:editors`
- `channel:manage:extensions`
- `channel:read:goals`
- `channel:read:guest_star`
- `channel:manage:guest_star`
- `channel:read:hype_train`
- `channel:manage:moderators`
- `channel:read:polls`
- `channel:manage:polls`
- `channel:read:predictions`
- `channel:manage:predictions`
- `channel:manage:raids`
- `channel:read:redemptions`
- `channel:manage:redemptions`
- `channel:manage:schedule`
- `channel:read:subscriptions`
- `channel:manage:videos`
- `channel:read:vips`
- `channel:manage:vips`
- `channel:moderate`
- `clips:edit`
- `moderation:read`
- `moderator:manage:announcements`
- `moderator:manage:automod`
- `moderator:read:automod_settings`
- `moderator:manage:automod_settings`
- `moderator:read:banned_users`
- `moderator:manage:banned_users`
- `moderator:read:blocked_terms`
- `moderator:read:chat_messages`
- `moderator:manage:blocked_terms`
- `moderator:manage:chat_messages`
- `moderator:read:chat_settings`
- `moderator:manage:chat_settings`
- `moderator:read:chatters`
- `moderator:read:followers`
- `moderator:read:guest_star`
- `moderator:manage:guest_star`
- `moderator:read:moderators`
- `moderator:read:shield_mode`
- `moderator:manage:shield_mode`
- `moderator:read:shoutouts`
- `moderator:manage:shoutouts`
- `moderator:read:suspicious_users`
- `moderator:read:unban_requests`
- `moderator:manage:unban_requests`
- `moderator:read:vips`
- `moderator:read:warnings`
- `moderator:manage:warnings`
- `user:edit`
- `user:edit:broadcast`
- `user:read:blocked_users`
- `user:manage:blocked_users`
- `user:read:broadcast`
- `user:read:chat`
- `user:write:chat`
- `user:manage:chat_color`
- `user:read:emotes`
- `user:read:follows`
- `user:read:moderated_channels`
- `user:read:subscriptions`
- `user:read:whispers`
- `user:manage:whispers`

</details>

<details>

<summary>List of scopes used by optional bot</summary>

- `channel:bot`
- `user:bot`

</details>

You can always verify the scopes at [Twitch Dev Docs](https://dev.twitch.tv/docs/authentication/scopes/)

## Contributing

This is a personal learning project exploring modern C++ patterns and async programming. Feel free to fork and experiment!

## License

MIT License - see LICENSE file for details.

---

*Built with modern C++23 • Powered by Boost.Asio • UI with Dear ImGui*
