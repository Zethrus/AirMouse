#include "airmouse/ui/settings.hpp"

#include <iostream>

namespace airmouse {

// Settings are file-backed. The first-cut window is a no-op; tray still opens
// the path by printing it so the user can edit config.json. A cairo window
// can replace this without touching the rest of the app.
class FileSettings final : public SettingsWindow {
 public:
  explicit FileSettings(Config* cfg) : cfg_(cfg) {}

  void show() override {
    visible_ = true;
    const auto path = default_config_path();
    try {
      save_config(path, *cfg_);
    } catch (...) {
    }
    std::cerr << "AirMouse settings: " << path.string() << '\n';
  }
  void hide() override { visible_ = false; }
  bool visible() const override { return visible_; }
  void poll() override {}
  void set_on_change(std::function<void(const Config&)> cb) override { cb_ = std::move(cb); }

 private:
  Config* cfg_ = nullptr;
  bool visible_ = false;
  std::function<void(const Config&)> cb_;
};

std::unique_ptr<SettingsWindow> create_settings(Config* cfg) {
  return std::make_unique<FileSettings>(cfg);
}

}  // namespace airmouse
