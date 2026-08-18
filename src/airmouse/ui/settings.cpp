#include "airmouse/ui/settings.hpp"

namespace airmouse {

#ifdef _WIN32
std::unique_ptr<SettingsWindow> create_win_settings(Config* cfg);
#else
std::unique_ptr<SettingsWindow> create_x11_settings(Config* cfg);
#endif

std::unique_ptr<SettingsWindow> create_settings(Config* cfg) {
#ifdef _WIN32
  return create_win_settings(cfg);
#else
  return create_x11_settings(cfg);
#endif
}

}  // namespace airmouse
