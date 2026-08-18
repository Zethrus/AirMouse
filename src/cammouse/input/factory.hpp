#pragma once

#include <memory>
#include <string>

#include "cammouse/input/backend.hpp"

namespace cammouse {

enum class SessionKind { X11, Wayland, Windows, Unknown };

SessionKind detect_session();
std::string session_label(SessionKind kind);
std::unique_ptr<InputBackend> create_input_backend();

}  // namespace cammouse
