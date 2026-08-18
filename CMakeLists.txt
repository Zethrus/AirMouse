cmake_minimum_required(VERSION 3.24)
project(airmouse VERSION 0.1.0 LANGUAGES CXX)

if(NOT AIRMOUSE_VERSION)
  set(AIRMOUSE_VERSION "${PROJECT_VERSION}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
  add_compile_options(/W4 /utf-8)
  add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN -DUNICODE -D_UNICODE)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(AIRMOUSE_SOURCES
  src/airmouse/config.cpp
  src/airmouse/gestures/poses.cpp
  src/airmouse/gestures/engine.cpp
  src/airmouse/pointer/filter.cpp
  src/airmouse/pointer/mapper.cpp
  src/airmouse/pointer/screens.cpp
  src/airmouse/input/factory.cpp
  src/airmouse/tracking/mediapipe_landmarker.cpp
  src/airmouse/app/tracker.cpp
  src/airmouse/ui/settings.cpp
  src/airmouse/platform/dbus_toggle.cpp
)

if(WIN32)
  list(APPEND AIRMOUSE_SOURCES
    src/airmouse/input/windows_sendinput.cpp
    src/airmouse/camera/win_capture.cpp
    src/airmouse/ui/overlay_win.cpp
    src/airmouse/ui/tray_win.cpp
    src/airmouse/platform/hotkey_win.cpp
  )
else()
  list(APPEND AIRMOUSE_SOURCES
    src/airmouse/input/linux_xtest.cpp
    src/airmouse/input/linux_uinput.cpp
    src/airmouse/camera/v4l2_capture.cpp
    src/airmouse/ui/overlay_x11.cpp
    src/airmouse/ui/tray_sni.cpp
    src/airmouse/platform/hotkey_x11.cpp
  )
endif()

add_library(airmouse_core STATIC ${AIRMOUSE_SOURCES})
target_include_directories(airmouse_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mediapipe/include
)

set(MP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/mediapipe/lib")
if(WIN32)
  set(MP_SHARED "${MP_DIR}/libmediapipe.dll")
  set(MP_IMPLIB "${MP_DIR}/libmediapipe.lib")
else()
  set(MP_SHARED "${MP_DIR}/libmediapipe.so")
  set(MP_IMPLIB "${MP_SHARED}")
endif()

option(AIRMOUSE_FETCH_MEDIAPIPE "Download MediaPipe C library if missing" ON)
if(AIRMOUSE_FETCH_MEDIAPIPE AND NOT EXISTS "${MP_SHARED}")
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/fetch_mediapipe.py"
            --dest "${MP_DIR}"
    RESULT_VARIABLE _mp_fetch_rc)
  if(NOT _mp_fetch_rc EQUAL 0)
    message(WARNING "MediaPipe fetch failed (rc=${_mp_fetch_rc})")
  endif()
endif()

if(UNIX AND NOT APPLE)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(X11 REQUIRED x11 xtst xext xrender xfixes)
  pkg_check_modules(CAIRO REQUIRED cairo cairo-xlib)
  pkg_check_modules(PANGO REQUIRED pangocairo)
  pkg_check_modules(DBUS REQUIRED dbus-1)
  pkg_check_modules(EGL REQUIRED egl)
  pkg_check_modules(GLESV2 REQUIRED glesv2)
  target_include_directories(airmouse_core PUBLIC
    ${X11_INCLUDE_DIRS} ${CAIRO_INCLUDE_DIRS} ${PANGO_INCLUDE_DIRS} ${DBUS_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS} ${GLESV2_INCLUDE_DIRS})
  target_link_libraries(airmouse_core PUBLIC
    ${X11_LIBRARIES} ${CAIRO_LIBRARIES} ${PANGO_LIBRARIES} ${DBUS_LIBRARIES}
    ${EGL_LIBRARIES} ${GLESV2_LIBRARIES})
endif()

if(WIN32)
  target_link_libraries(airmouse_core PUBLIC user32)
endif()

if(WIN32 AND EXISTS "${MP_SHARED}" AND NOT EXISTS "${MP_IMPLIB}")
  find_package(Python3 COMPONENTS Interpreter)
  if(Python3_Interpreter_FOUND)
    execute_process(
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/gen_msvc_implib.py"
              "${MP_SHARED}"
      RESULT_VARIABLE _mp_implib_rc)
    if(NOT _mp_implib_rc EQUAL 0)
      message(WARNING "Could not generate MediaPipe import library")
    endif()
  endif()
endif()

if(EXISTS "${MP_IMPLIB}")
  target_link_libraries(airmouse_core PUBLIC "${MP_IMPLIB}")
  set_target_properties(airmouse_core PROPERTIES
    BUILD_RPATH "${MP_DIR}"
    INSTALL_RPATH "$ORIGIN/../lib"
  )
endif()

add_executable(airmouse src/airmouse/main.cpp)
target_compile_definitions(airmouse PRIVATE AIRMOUSE_VERSION="${AIRMOUSE_VERSION}")
target_link_libraries(airmouse PRIVATE airmouse_core)
if(UNIX AND NOT APPLE)
  target_link_libraries(airmouse PRIVATE pthread)
endif()
set_target_properties(airmouse PROPERTIES
  BUILD_RPATH "${MP_DIR}"
  INSTALL_RPATH "$ORIGIN/../lib"
)

if(WIN32 AND EXISTS "${MP_SHARED}")
  add_custom_command(TARGET airmouse POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MP_SHARED}" "$<TARGET_FILE_DIR:airmouse>/libmediapipe.dll")
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_executable(airmouse_tests
  tests/test_poses.cpp
  tests/test_engine.cpp
  tests/test_mapper.cpp
  tests/test_filter.cpp
  tests/test_factory.cpp
)
target_link_libraries(airmouse_tests PRIVATE airmouse_core GTest::gtest_main)
add_test(NAME airmouse_tests COMMAND airmouse_tests)

include(GNUInstallDirs)

if(WIN32)
  install(TARGETS airmouse RUNTIME DESTINATION .)
  if(EXISTS "${MP_SHARED}")
    install(FILES "${MP_SHARED}" DESTINATION .)
  endif()
  install(DIRECTORY assets/ DESTINATION assets)
  install(FILES README.md LICENSE DESTINATION .)
else()
  install(TARGETS airmouse RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  if(EXISTS "${MP_SHARED}")
    install(FILES "${MP_SHARED}" DESTINATION ${CMAKE_INSTALL_LIBDIR})
  endif()
  install(DIRECTORY assets/ DESTINATION ${CMAKE_INSTALL_DATADIR}/airmouse)
  install(FILES packaging/linux/airmouse.desktop
          DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
  install(FILES packaging/linux/udev/99-airmouse-uinput.rules
          DESTINATION ${CMAKE_INSTALL_DATADIR}/airmouse)
  install(FILES README.md LICENSE DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()
