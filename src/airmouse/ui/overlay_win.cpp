#include "airmouse/ui/overlay.hpp"

#ifdef _WIN32

namespace airmouse {

class NullOverlay final : public Overlay {
 public:
  bool create() override { return true; }
  void destroy() override {}
  void set_visible(bool on) override { visible_ = on; }
  bool visible() const override { return visible_; }
  void set_snapshot(const TrackingSnapshot&) override {}
  void set_chip(std::string) override {}
  void poll() override {}
  bool wants_quit() const override { return false; }

 private:
  bool visible_ = true;
};

std::unique_ptr<Overlay> create_overlay() { return std::make_unique<NullOverlay>(); }

}  // namespace airmouse

#endif
