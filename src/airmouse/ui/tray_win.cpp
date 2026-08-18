#include "airmouse/ui/tray.hpp"

#ifdef _WIN32

namespace airmouse {

class NullTray final : public Tray {
 public:
  bool create() override { return true; }
  void set_status(const std::string&) override {}
  void poll() override {}
  void set_handler(std::function<void(TrayAction)>) override {}
};

std::unique_ptr<Tray> create_tray() { return std::make_unique<NullTray>(); }

}  // namespace airmouse

#endif
