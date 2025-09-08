# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

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
