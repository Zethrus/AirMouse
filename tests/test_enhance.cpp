#include <gtest/gtest.h>

#include "airmouse/tracking/enhance.hpp"

using namespace airmouse;

static RgbFrame solid(int w, int h, uint8_t r, uint8_t g, uint8_t b) {
  RgbFrame f;
  f.width = w;
  f.height = h;
  f.rgb.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 3, 0);
  for (size_t i = 0; i < f.rgb.size(); i += 3) {
    f.rgb[i] = r;
    f.rgb[i + 1] = g;
    f.rgb[i + 2] = b;
  }
  return f;
}

TEST(Enhance, BrightFrameUnchanged) {
  auto frame = solid(32, 32, 140, 140, 140);
  const auto before = frame.rgb;
  EnhanceState st;
  EnhanceConfig cfg;
  enhance_frame(frame, st, cfg, 0.016f);
  EXPECT_EQ(frame.rgb, before);
  EXPECT_FLOAT_EQ(st.gain, 1.f);
}

TEST(Enhance, DarkFrameLifts) {
  auto frame = solid(32, 32, 40, 40, 40);
  const float before = mean_luma(frame);
  EnhanceState st;
  EnhanceConfig cfg;
  enhance_frame(frame, st, cfg, 2.f);
  const float after = mean_luma(frame);
  EXPECT_GT(after, before);
  EXPECT_LE(st.gain, cfg.max_gain + 1e-4f);
  EXPECT_GT(st.gain, 1.f);
}

TEST(Enhance, GainDoesNotJump) {
  auto frame = solid(32, 32, 40, 40, 40);
  EnhanceState st;
  EnhanceConfig cfg;
  enhance_frame(frame, st, cfg, 0.016f);
  const float g1 = st.gain;
  auto frame2 = solid(32, 32, 40, 40, 40);
  enhance_frame(frame2, st, cfg, 0.016f);
  EXPECT_LT(g1, 1.2f);
  EXPECT_LT(st.gain - g1, 0.15f);
}

TEST(Enhance, DisabledIsNoop) {
  auto frame = solid(16, 16, 20, 20, 20);
  const auto before = frame.rgb;
  EnhanceState st;
  EnhanceConfig cfg;
  cfg.enabled = false;
  enhance_frame(frame, st, cfg, 2.f);
  EXPECT_EQ(frame.rgb, before);
}
