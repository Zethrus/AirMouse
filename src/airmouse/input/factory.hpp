#pragma once

#include <memory>
#include <string>

#include "airmouse/input/backend.hpp"

namespace airmouse {

enum class SessionKind { X11, Wayland, Windows, Unknown };

SessionKind detect_session();
std::string session_label(SessionKind kind);
std::unique_ptr<InputBackend> create_input_backend();

}  // namespace airmouse
