#include <gtest/gtest.h>

#include "airmouse/camera/capture.hpp"

using namespace airmouse;

TEST(CameraStatus, AbsenceCopy) {
  EXPECT_STREQ(camera_absence_status(CameraAbsence::PoweredOff), "CAM OFF");
  EXPECT_STREQ(camera_absence_status(CameraAbsence::NoDevice), "NO CAM");
  EXPECT_STREQ(camera_absence_status(CameraAbsence::Permission), "NO CAM");
  EXPECT_STREQ(camera_absence_status(CameraAbsence::Busy), "NO CAM");
  EXPECT_STREQ(camera_absence_status(CameraAbsence::Unsupported), "NO CAM");
  EXPECT_STREQ(camera_absence_status(CameraAbsence::Present), "NO CAM");

  EXPECT_EQ(camera_absence_message(CameraAbsence::PoweredOff),
            "Press the camera key, then wait");
  EXPECT_EQ(camera_absence_message(CameraAbsence::Permission),
            "Add user to video group, log out");
  EXPECT_EQ(camera_absence_message(CameraAbsence::Busy), "Camera in use by another app");
  EXPECT_EQ(camera_absence_message(CameraAbsence::NoDevice), "No webcam found");
  EXPECT_EQ(camera_absence_message(CameraAbsence::Unsupported),
            "Camera found, cannot capture");
  EXPECT_TRUE(camera_absence_message(CameraAbsence::Present).empty());
}

TEST(CameraStatus, MessagesFitHud) {
  const CameraAbsence cases[] = {
      CameraAbsence::PoweredOff, CameraAbsence::Permission, CameraAbsence::Busy,
      CameraAbsence::NoDevice,   CameraAbsence::Unsupported,
  };
  for (auto absence : cases) {
    EXPECT_LE(camera_absence_message(absence).size(), 34u);
  }
}

#ifndef _WIN32
TEST(CameraStatus, DiagnoseDoesNotCrash) {
  const auto absence = diagnose_camera();
  EXPECT_FALSE(camera_absence_status(absence) == nullptr);
  const auto devices = list_camera_devices();
  for (const auto& dev : devices) {
    EXPECT_TRUE(dev.capture);
    EXPECT_GE(dev.index, 0);
    EXPECT_FALSE(dev.path.empty());
  }
}
#endif
