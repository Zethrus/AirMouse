#include "airmouse/camera/capture.hpp"

#ifndef _WIN32

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <sstream>

namespace airmouse {
namespace {

void yuyv_to_rgb(const uint8_t* src, int width, int height, uint8_t* dst) {
  auto clamp = [](int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); };
  for (int i = 0; i < width * height; i += 2) {
    const int y0 = src[0];
    const int u = src[1] - 128;
    const int y1 = src[2];
    const int v = src[3] - 128;
    src += 4;
    const int c0 = y0 - 16;
    const int c1 = y1 - 16;
    *dst++ = clamp((298 * c0 + 409 * v + 128) >> 8);
    *dst++ = clamp((298 * c0 - 100 * u - 208 * v + 128) >> 8);
    *dst++ = clamp((298 * c0 + 516 * u + 128) >> 8);
    *dst++ = clamp((298 * c1 + 409 * v + 128) >> 8);
    *dst++ = clamp((298 * c1 - 100 * u - 208 * v + 128) >> 8);
    *dst++ = clamp((298 * c1 + 516 * u + 128) >> 8);
  }
}

struct MapBuf {
  void* start = nullptr;
  size_t length = 0;
};

class V4l2Camera final : public Camera {
 public:
  ~V4l2Camera() override { close(); }

  bool open(int index, int width, int height, int fps) override {
    close();
    char path[64];
    std::snprintf(path, sizeof(path), "/dev/video%d", index);
    fd_ = ::open(path, O_RDWR | O_NONBLOCK);
    if (fd_ < 0) {
      err_ = std::string("cannot open ") + path;
      return false;
    }

    v4l2_capability cap{};
    if (ioctl(fd_, VIDIOC_QUERYCAP, &cap) < 0 ||
        (cap.device_caps & V4L2_CAP_VIDEO_CAPTURE) == 0) {
      err_ = "device is not a video capture source";
      close();
      return false;
    }

    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = static_cast<uint32_t>(width);
    fmt.fmt.pix.height = static_cast<uint32_t>(height);
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
      err_ = "camera rejected YUYV 640x480-style format";
      close();
      return false;
    }
    width_ = static_cast<int>(fmt.fmt.pix.width);
    height_ = static_cast<int>(fmt.fmt.pix.height);

    if (fps > 0) {
      v4l2_streamparm parm{};
      parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      parm.parm.capture.timeperframe.numerator = 1;
      parm.parm.capture.timeperframe.denominator = static_cast<uint32_t>(fps);
      ioctl(fd_, VIDIOC_S_PARM, &parm);
    }

    v4l2_requestbuffers req{};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd_, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
      err_ = "VIDIOC_REQBUFS failed";
      close();
      return false;
    }
    bufs_.resize(req.count);
    for (unsigned i = 0; i < req.count; ++i) {
      v4l2_buffer buf{};
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      buf.index = i;
      if (ioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0) {
        err_ = "VIDIOC_QUERYBUF failed";
        close();
        return false;
      }
      bufs_[i].length = buf.length;
      bufs_[i].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_,
                            buf.m.offset);
      if (bufs_[i].start == MAP_FAILED) {
        bufs_[i].start = nullptr;
        err_ = "mmap failed";
        close();
        return false;
      }
      if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
        err_ = "VIDIOC_QBUF failed";
        close();
        return false;
      }
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
      err_ = "VIDIOC_STREAMON failed";
      close();
      return false;
    }
    err_.clear();
    return true;
  }

  void close() override {
    if (fd_ >= 0) {
      v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      ioctl(fd_, VIDIOC_STREAMOFF, &type);
    }
    for (auto& b : bufs_) {
      if (b.start && b.start != MAP_FAILED) {
        munmap(b.start, b.length);
      }
    }
    bufs_.clear();
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
  }

  bool read(RgbFrame& out) override {
    if (fd_ < 0) return false;
    v4l2_buffer buf{};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
      if (errno == EAGAIN) return false;
      err_ = "VIDIOC_DQBUF failed";
      return false;
    }
    out.width = width_;
    out.height = height_;
    out.rgb.resize(static_cast<size_t>(width_ * height_ * 3));
    const auto* src = static_cast<const uint8_t*>(bufs_[buf.index].start);
    yuyv_to_rgb(src, width_, height_, out.rgb.data());
    ioctl(fd_, VIDIOC_QBUF, &buf);
    return true;
  }

  bool ok() const override { return fd_ >= 0; }
  std::string last_error() const override { return err_; }

 private:
  int fd_ = -1;
  int width_ = 0;
  int height_ = 0;
  std::string err_;
  std::vector<MapBuf> bufs_;
};

}  // namespace

std::unique_ptr<Camera> create_camera() { return std::make_unique<V4l2Camera>(); }

std::vector<int> list_camera_indices() {
  std::vector<int> out;
  for (int i = 0; i < 10; ++i) {
    char path[64];
    std::snprintf(path, sizeof(path), "/dev/video%d", i);
    const int fd = ::open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) continue;
    v4l2_capability cap{};
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) == 0 &&
        (cap.device_caps & V4L2_CAP_VIDEO_CAPTURE) != 0) {
      out.push_back(i);
    }
    ::close(fd);
  }
  return out;
}

}  // namespace airmouse

#endif
