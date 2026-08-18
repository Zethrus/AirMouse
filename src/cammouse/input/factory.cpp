#include "cammouse/input/factory.hpp"

#include <cstdlib>
#include <stdexcept>

#ifdef _WIN32
namespace cammouse {
std::unique_ptr<InputBackend> create_sendinput_backend();
}
#else
namespace cammouse {
std::unique_ptr<InputBackend> create_xtest_backend();
std::unique_ptr<InputBackend> create_uinput_backend();
}
#endif

namespace cammouse {

SessionKind detect_session() {
#ifdef _WIN32
  return SessionKind::Windows;
#else
  if (const char* type = std::getenv("XDG_SESSION_TYPE")) {
    if (std::string(type) == "wayland") return SessionKind::Wayland;
    if (std::string(type) == "x11") return SessionKind::X11;
  }
  if (std::getenv("WAYLAND_DISPLAY") && *std::getenv("WAYLAND_DISPLAY")) {
    return SessionKind::Wayland;
  }
  if (std::getenv("DISPLAY") && *std::getenv("DISPLAY")) {
    return SessionKind::X11;
  }
  return SessionKind::Unknown;
#endif
}

std::string session_label(SessionKind kind) {
  switch (kind) {
    case SessionKind::X11:
      return "x11";
    case SessionKind::Wayland:
      return "wayland";
    case SessionKind::Windows:
      return "windows";
    case SessionKind::Unknown:
      break;
  }
  return "unknown";
}

std::unique_ptr<InputBackend> create_input_backend() {
#ifdef _WIN32
  return create_sendinput_backend();
#else
  const SessionKind session = detect_session();
  if (session == SessionKind::Wayland) {
    return create_uinput_backend();
  }
  if (session == SessionKind::X11 || session == SessionKind::Unknown) {
    return create_xtest_backend();
  }
  throw std::runtime_error("no input backend for this session");
#endif
}

}  // namespace cammouse
