cmake_minimum_required(VERSION 3.24)
project(cammouse VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT MSVC)
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CAMMOUSE_SOURCES
  src/cammouse/config.cpp
  src/cammouse/gestures/poses.cpp
  src/cammouse/gestures/engine.cpp
  src/cammouse/pointer/filter.cpp
  src/cammouse/pointer/mapper.cpp
  src/cammouse/pointer/screens.cpp
  src/cammouse/input/factory.cpp
  src/cammouse/tracking/mediapipe_landmarker.cpp
  src/cammouse/app/tracker.cpp
  src/cammouse/ui/settings.cpp
  src/cammouse/platform/dbus_toggle.cpp
)

if(WIN32)
  list(APPEND CAMMOUSE_SOURCES
    src/cammouse/input/windows_sendinput.cpp
    src/cammouse/camera/win_capture.cpp
    src/cammouse/ui/overlay_win.cpp
    src/cammouse/ui/tray_win.cpp
    src/cammouse/platform/hotkey_win.cpp
  )
else()
  list(APPEND CAMMOUSE_SOURCES
    src/cammouse/input/linux_xtest.cpp
    src/cammouse/input/linux_uinput.cpp
    src/cammouse/camera/v4l2_capture.cpp
    src/cammouse/ui/overlay_x11.cpp
    src/cammouse/ui/tray_sni.cpp
    src/cammouse/platform/hotkey_x11.cpp
  )
endif()

add_library(cammouse_core STATIC ${CAMMOUSE_SOURCES})
target_include_directories(cammouse_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mediapipe/include
)

set(MP_LIB "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mediapipe/lib/libmediapipe.so")
if(WIN32)
  set(MP_LIB "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mediapipe/lib/mediapipe.dll")
endif()

if(UNIX AND NOT APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(X11 REQUIRED x11 xtst xext xrender xfixes)
  pkg_check_modules(CAIRO REQUIRED cairo cairo-xlib)
  pkg_check_modules(PANGO REQUIRED pangocairo)
  pkg_check_modules(DBUS REQUIRED dbus-1)
  target_include_directories(cammouse_core PUBLIC
    ${X11_INCLUDE_DIRS} ${CAIRO_INCLUDE_DIRS} ${PANGO_INCLUDE_DIRS} ${DBUS_INCLUDE_DIRS})
  target_link_libraries(cammouse_core PUBLIC
    ${X11_LIBRARIES} ${CAIRO_LIBRARIES} ${PANGO_LIBRARIES} ${DBUS_LIBRARIES})
endif()

if(EXISTS "${MP_LIB}")
  target_link_libraries(cammouse_core PUBLIC "${MP_LIB}")
  set_target_properties(cammouse_core PROPERTIES
    BUILD_RPATH "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mediapipe/lib"
    INSTALL_RPATH "$ORIGIN/../lib"
  )
endif()

add_executable(cammouse src/cammouse/main.cpp)
target_link_libraries(cammouse PRIVATE cammouse_core)
if(UNIX AND NOT APPLE)
  target_link_libraries(cammouse PRIVATE pthread)
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(cammouse_tests
  tests/test_poses.cpp
  tests/test_engine.cpp
  tests/test_mapper.cpp
  tests/test_filter.cpp
  tests/test_factory.cpp
)
target_link_libraries(cammouse_tests PRIVATE cammouse_core GTest::gtest_main)
add_test(NAME cammouse_tests COMMAND cammouse_tests)

install(TARGETS cammouse RUNTIME DESTINATION bin)
install(FILES "${MP_LIB}" DESTINATION lib)
install(DIRECTORY assets/ DESTINATION share/cammouse)
install(FILES packaging/linux/cammouse.desktop DESTINATION share/applications)
