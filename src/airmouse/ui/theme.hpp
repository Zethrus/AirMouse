#pragma once

#include <cstdint>

namespace airmouse::theme {

namespace color {
inline constexpr uint32_t void_ = 0x070A0E;
inline constexpr uint32_t panel = 0x0C1218;
inline constexpr uint32_t paper = 0xD7E0E6;
inline constexpr uint32_t dim = 0x6B7780;
inline constexpr uint32_t accent = 0x7FD4C8;
inline constexpr uint32_t warn = 0xD4A054;
inline constexpr uint32_t hair = 0xC5D0D6;
}  // namespace color

namespace alpha {
inline constexpr double glass = 0.82;
inline constexpr double lift = 0.88;
inline constexpr double hair = 0.16;
inline constexpr double hair_dim = 0.10;
inline constexpr double hair_hot = 0.40;
inline constexpr double lift_hair = 0.28;
inline constexpr double lift_rim = 0.35;
inline constexpr double grid = 0.06;
inline constexpr double bone = 0.20;
inline constexpr double ray = 0.70;
inline constexpr double chip = 0.92;
inline constexpr double chip_on = 0.22;
inline constexpr double chip_off = 0.08;
inline constexpr double text = 0.92;
inline constexpr double muted = 0.85;
inline constexpr double empty = 0.45;
inline constexpr double tip = 0.30;
inline constexpr double index_tip = 0.95;
inline constexpr double grip_dot = 0.30;
inline constexpr double grip_dot_hot = 0.90;
inline constexpr double seg_empty = 0.12;
inline constexpr double slider = 0.85;
}  // namespace alpha

namespace type {
inline constexpr float micro = 8.f;
inline constexpr float caption = 9.f;
inline constexpr float label = 11.f;
inline constexpr float body = 12.f;
inline constexpr float title = 18.f;
}  // namespace type

namespace font {
inline constexpr const char* mono = "IBM Plex Mono";
inline constexpr const char* sans = "IBM Plex Sans";
}  // namespace font

namespace space {
inline constexpr int xxs = 2;
inline constexpr int xs = 4;
inline constexpr int sm = 8;
inline constexpr int md = 12;
inline constexpr int lg = 16;
inline constexpr int xl = 24;
}  // namespace space

namespace radius {
inline constexpr double tight = 4;
inline constexpr double panel = 8;
inline constexpr double chip = 8;
}  // namespace radius

namespace motion {
inline constexpr float hover_tau = 0.12f;
inline constexpr float lift_tau = 0.08f;
inline constexpr float spring_tau = 0.055f;
inline constexpr float snap_px = 28.f;
inline constexpr float settle_pos = 0.5f;
inline constexpr float settle_vel = 1.f;
}  // namespace motion

namespace hud {
inline constexpr int w = 236;
inline constexpr int h = 164;
inline constexpr int grip_h = 22;
inline constexpr int pad = 24;
inline constexpr int well_inset = 10;
inline constexpr int bracket = 12;
inline constexpr int grid_x = 8;
inline constexpr int grid_y = 5;
inline constexpr int seg_w = 4;
inline constexpr int seg_h = 8;
inline constexpr int seg_gap = 2;
inline constexpr int segs = 5;
inline constexpr int footer_h = 22;
inline constexpr int chip_h = 16;
}  // namespace hud

namespace settings {
inline constexpr int w = 380;
inline constexpr int h = 424;
inline constexpr int row_h = 24;
inline constexpr int cam_h = 28;
inline constexpr int action_h = 32;
inline constexpr int slider_h = 18;
}  // namespace settings

}  // namespace airmouse::theme
