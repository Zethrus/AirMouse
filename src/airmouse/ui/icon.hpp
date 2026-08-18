#pragma once

#include <filesystem>

namespace airmouse {

// Best matching shipped icon for the requested pixel size.
std::filesystem::path app_icon_path(int size_hint = 256);

}  // namespace airmouse
