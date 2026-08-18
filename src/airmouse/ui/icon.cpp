#include "airmouse/ui/icon.hpp"

#include "airmouse/config.hpp"

namespace airmouse {

std::filesystem::path app_icon_path(int size_hint) {
  const auto icons = asset_root() / "icons";
#ifdef _WIN32
  const auto ico = icons / "airmouse.ico";
  if (std::filesystem::exists(ico)) return ico;
#endif
  const int ladder[] = {size_hint, 256, 128, 64, 48, 32, 24, 22, 16};
  for (int size : ladder) {
    if (size <= 0) continue;
    const auto png = icons / ("airmouse-" + std::to_string(size) + ".png");
    if (std::filesystem::exists(png)) return png;
  }
  const auto fallback = icons / "airmouse.png";
  if (std::filesystem::exists(fallback)) return fallback;
  const auto ico = icons / "airmouse.ico";
  if (std::filesystem::exists(ico)) return ico;
  return {};
}

}  // namespace airmouse
