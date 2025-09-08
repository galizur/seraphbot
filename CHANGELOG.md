# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]
### Changed
- Defaulted builds to preferred library type (on Linux usually shared libraries)
- Modified meson.build to prefer system libraries first, fall back to subprojects if they can't be found
- Boost version is pinned to a specific tested version.
- spdlog will compile accordingly depending of if `libc++` was found or not
- ImGui now points to my personal fork that just has a `meson.build` file for easy building

### Added
- boost is now pinned to version 1.88 and falls back to that tag for building when needed

### Fixed
- Windows builds fall back to `c++latest` and if all fail they fall back to `c++20`

## [0.1.0] - 2025-09-08
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
