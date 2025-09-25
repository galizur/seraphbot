# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]
### Added
- Test subscriptions to verify WebSocket connection
- Text in chat wraps at the end of the window
- Initial Discord going-live integration
- Add Discord frontend
- Initial implementation of OBS WebSocket connection
- Basic command parser to recognize incoming chat commands
- Lua scripting engine via Sol2 for adding user scripting capabilities
  - Commands have a name and function.
  - Optionally, you can give aliases, description, usage, global/user cooldowns, max uses per stream and subscriber, moderator or VIP levels of use
- Some basic examples to test capabilities
- Add basic audio interaction system (can play sounds from folders)
- Add basic sandboxing features


### Changed
- Added a default fallback font (FiraSans - random right now!)
- Moved functionality from main to `ImGuiManager`.

## [0.1.0-alpha.2] - 2025-09-12
### Changed
- Defaulted builds to preferred library type (on Linux usually shared libraries)
- Modified meson.build to prefer system libraries first, fall back to subprojects if they can't be found
- `Boost` version is pinned to a specific tested version.
- `spdlog` will compile accordingly depending of if `libc++` was found or not
- `ImGui` now points to my personal fork that just has a `meson.build` file for easy building
- Listed all scopes used (or will be used)
- Moved to a centralized `EventSub` class and all other subscriptions will use this to request to subscribe to Twitch features
- Also moved most Twitch functionality under a `TwitchService` class to better control the flow of operations
- Add some helper classes
- Simplified `ImGui` slightly and thus the main function (still needs a lot of work)
- Enabled docking feature of `ImGui`

### Added
- `Boost` is now pinned to version 1.88 and falls back to that tag for building when needed
- More scopes in the authentication server
- Bot authentication endpoint in authentication server
- Clarify authentication server privacy

### Fixed
- Windows builds fall back to `c++latest` and if all fail they fall back to `c++20`
- Switched to wrap files and lowered OpenSSL version requirement, so they are self contained.

## [0.1.0-alpha.1] - 2025-09-08
### Added
- Initial Twitch chat bot implementation
- OAuth authentication with Twitch
- Real-time chat message reading via EventSub WebSocket
- Chat message sending capability
- ImGui-based user interface
- Connection management with SSL/TLS support
- Logging system with file and console output

### Technical
- Boost.Beast for HTTP/WebSocket networking
- Boost.Asio for async I/O
- OpenGL + GLFW for rendering
- nlohmann/json for JSON parsing
- Experimental Windows build support
