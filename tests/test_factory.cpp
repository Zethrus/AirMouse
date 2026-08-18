#include <gtest/gtest.h>

#include "airmouse/input/factory.hpp"

using namespace airmouse;

TEST(Factory, DetectsSomething) {
  const auto kind = detect_session();
#ifdef _WIN32
  EXPECT_EQ(kind, SessionKind::Windows);
#else
  EXPECT_NE(kind, SessionKind::Windows);
  EXPECT_FALSE(session_label(kind).empty());
#endif
}
