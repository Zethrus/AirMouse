#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "airmouse/camera/capture.hpp"
#include "airmouse/config.hpp"

namespace airmouse {

struct EnhanceState {
  float gain = 1.f;
};

inline float mean_luma(const RgbFrame& frame) {
  if (frame.width <= 0 || frame.height <= 0 || frame.rgb.size() < 3) return 0.f;
  const int stride = frame.width * 3;
  const int max_bytes = static_cast<int>(frame.rgb.size());
  double sum = 0;
  int n = 0;
  constexpr int kStep = 4;
  for (int y = 0; y < frame.height; y += kStep) {
    const int row = y * stride;
    if (row >= max_bytes) break;
    for (int x = 0; x < frame.width; x += kStep) {
      const int i = row + x * 3;
      if (i + 2 >= max_bytes) break;
      const float r = static_cast<float>(frame.rgb[static_cast<size_t>(i)]);
      const float g = static_cast<float>(frame.rgb[static_cast<size_t>(i + 1)]);
      const float b = static_cast<float>(frame.rgb[static_cast<size_t>(i + 2)]);
      sum += 0.299 * r + 0.587 * g + 0.114 * b;
      ++n;
    }
  }
  return n > 0 ? static_cast<float>(sum / n) : 0.f;
}

inline void blur_3tap(RgbFrame& frame) {
  if (frame.width < 2 || frame.height < 2) return;
  const int w = frame.width;
  const int h = frame.height;
  const int stride = w * 3;
  std::vector<uint8_t> tmp(frame.rgb.size());
  auto at = [&](const std::vector<uint8_t>& src, int x, int y, int c) -> int {
    x = std::clamp(x, 0, w - 1);
    y = std::clamp(y, 0, h - 1);
    return src[static_cast<size_t>(y * stride + x * 3 + c)];
  };
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      for (int c = 0; c < 3; ++c) {
        const int v = at(frame.rgb, x - 1, y, c) + 2 * at(frame.rgb, x, y, c) +
                      at(frame.rgb, x + 1, y, c);
        tmp[static_cast<size_t>(y * stride + x * 3 + c)] = static_cast<uint8_t>(v >> 2);
      }
    }
  }
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      for (int c = 0; c < 3; ++c) {
        const int v = at(tmp, x, y - 1, c) + 2 * at(tmp, x, y, c) + at(tmp, x, y + 1, c);
        frame.rgb[static_cast<size_t>(y * stride + x * 3 + c)] = static_cast<uint8_t>(v >> 2);
      }
    }
  }
}

inline void enhance_frame(RgbFrame& frame, EnhanceState& state, const EnhanceConfig& cfg,
                          float dt) {
  if (!cfg.enabled || frame.rgb.empty()) return;
  const float mean = mean_luma(frame);
  float target = 1.f;
  if (mean < 96.f && mean > 1e-3f) {
    const float max_gain = std::max(1.f, cfg.max_gain);
    target = std::clamp(cfg.target_luma / mean, 1.f, max_gain);
  }
  const float tau = std::max(cfg.adapt_tau, 1e-3f);
  const float a = 1.f - std::exp(-std::max(dt, 0.f) / tau);
  state.gain += (target - state.gain) * a;
  if (state.gain < 1.f) state.gain = 1.f;
  if (state.gain <= 1.001f && target <= 1.f) {
    state.gain = 1.f;
    return;
  }
  const float gain = state.gain;
  for (uint8_t& px : frame.rgb) {
    const float v = static_cast<float>(px) * gain;
    px = static_cast<uint8_t>(v > 255.f ? 255.f : v);
  }
  if (gain > cfg.denoise_above) {
    blur_3tap(frame);
  }
}

}  // namespace airmouse
