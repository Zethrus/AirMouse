#include "airmouse/camera/capture.hpp"

#ifdef _WIN32

#include <stdexcept>

namespace airmouse {

class StubWinCamera final : public Camera {
 public:
  bool open(int, int, int, int) override {
    err_ = "Windows camera backend is not wired yet";
    return false;
  }
  void close() override {}
  bool read(RgbFrame&) override { return false; }
  bool ok() const override { return false; }
  std::string last_error() const override { return err_; }

 private:
  std::string err_ = "not opened";
};

std::unique_ptr<Camera> create_camera() { return std::make_unique<StubWinCamera>(); }

std::vector<int> list_camera_indices() { return {}; }

}  // namespace airmouse

#endif
