#include "airmouse/camera/capture.hpp"

#ifndef _WIN32

#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <setjmp.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#ifdef AIRMOUSE_HAVE_JPEG
#include <jpeglib.h>
#endif

namespace airmouse {
namespace {

void yuyv_to_rgb(const uint8_t* src, int width, int height, int stride, uint8_t* dst) {
  auto clamp = [](int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); };
  const int row_bytes = stride > 0 ? stride : width * 2;
  for (int y = 0; y < height; ++y) {
    const uint8_t* row = src + static_cast<size_t>(y) * static_cast<size_t>(row_bytes);
    for (int x = 0; x < width; x += 2) {
      const int y0 = row[0];
      const int u = row[1] - 128;
      const int y1 = row[2];
      const int v = row[3] - 128;
      row += 4;
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
}

void uyvy_to_rgb(const uint8_t* src, int width, int height, int stride, uint8_t* dst) {
  auto clamp = [](int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); };
  const int row_bytes = stride > 0 ? stride : width * 2;
  for (int y = 0; y < height; ++y) {
    const uint8_t* row = src + static_cast<size_t>(y) * static_cast<size_t>(row_bytes);
    for (int x = 0; x < width; x += 2) {
      const int u = row[0] - 128;
      const int y0 = row[1];
      const int v = row[2] - 128;
      const int y1 = row[3];
      row += 4;
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
}

#ifdef AIRMOUSE_HAVE_JPEG
struct JpegErr {
  jpeg_error_mgr pub;
  jmp_buf jmp;
};

void jpeg_on_error(j_common_ptr cinfo) {
  auto* err = reinterpret_cast<JpegErr*>(cinfo->err);
  longjmp(err->jmp, 1);
}

bool jpeg_to_rgb(const uint8_t* src, unsigned long len, int* width, int* height,
                 std::vector<uint8_t>* dst) {
  jpeg_decompress_struct cinfo{};
  JpegErr jerr{};
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpeg_on_error;
  if (setjmp(jerr.jmp)) {
    jpeg_destroy_decompress(&cinfo);
    return false;
  }
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, const_cast<unsigned char*>(src), len);
  if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
    jpeg_destroy_decompress(&cinfo);
    return false;
  }
  cinfo.out_color_space = JCS_RGB;
  jpeg_start_decompress(&cinfo);
  *width = static_cast<int>(cinfo.output_width);
  *height = static_cast<int>(cinfo.output_height);
  dst->assign(static_cast<size_t>(*width) * static_cast<size_t>(*height) * 3, 0);
  while (cinfo.output_scanline < cinfo.output_height) {
    JSAMPROW row = dst->data() +
                   static_cast<size_t>(cinfo.output_scanline) * static_cast<size_t>(*width) * 3;
    jpeg_read_scanlines(&cinfo, &row, 1);
  }
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);
  return *width > 0 && *height > 0;
}
#endif

bool is_capture_fd(int fd) {
  v4l2_capability cap{};
  if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) return false;
  const uint32_t caps = cap.device_caps ? cap.device_caps : cap.capabilities;
  return (caps & (V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE)) != 0;
}

std::string errno_text(const char* path) {
  const int e = errno;
  std::string msg = std::string(path) + ": " + std::strerror(e);
  if (e == EACCES) {
    msg += " (add your user to the 'video' group, then log out)";
  }
  return msg;
}

enum class PixelKind { Yuyv, Uyvy, Mjpeg, Unknown };

struct MapBuf {
  void* start = nullptr;
  size_t length = 0;
};

class V4l2Camera final : public Camera {
 public:
  ~V4l2Camera() override { close(); }

  bool open(int index, int width, int height, int fps) override {
    close();
    std::vector<int> candidates;
    if (index >= 0) candidates.push_back(index);
    for (int found : list_camera_indices()) {
      if (found != index) candidates.push_back(found);
    }
    if (candidates.empty()) {
      err_ = camera_absence_message(diagnose_camera());
      return false;
    }

    std::string errors;
    for (int i : candidates) {
      if (open_device(i, width, height, fps)) {
        err_.clear();
        return true;
      }
      if (!errors.empty()) errors += " | ";
      errors += err_;
    }
    err_ = errors.empty() ? "camera open failed" : errors;
    return false;
  }

  void close() override {
    release_stream();
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
    kind_ = PixelKind::Unknown;
  }

  bool read(RgbFrame& out) override {
    if (fd_ < 0) return false;
    v4l2_buffer latest{};
    bool have = false;
    for (;;) {
      v4l2_buffer buf{};
      buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      buf.memory = V4L2_MEMORY_MMAP;
      if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
        if (errno == EAGAIN) break;
        err_ = "VIDIOC_DQBUF failed";
        if (have) ioctl(fd_, VIDIOC_QBUF, &latest);
        return false;
      }
      if (have) ioctl(fd_, VIDIOC_QBUF, &latest);
      latest = buf;
      have = true;
    }
    if (!have) return false;
    const auto* src = static_cast<const uint8_t*>(bufs_[latest.index].start);
    bool ok = false;
    if (kind_ == PixelKind::Yuyv) {
      out.width = width_;
      out.height = height_;
      out.rgb.resize(static_cast<size_t>(width_) * static_cast<size_t>(height_) * 3);
      yuyv_to_rgb(src, width_, height_, stride_, out.rgb.data());
      ok = true;
    } else if (kind_ == PixelKind::Uyvy) {
      out.width = width_;
      out.height = height_;
      out.rgb.resize(static_cast<size_t>(width_) * static_cast<size_t>(height_) * 3);
      uyvy_to_rgb(src, width_, height_, stride_, out.rgb.data());
      ok = true;
#ifdef AIRMOUSE_HAVE_JPEG
    } else if (kind_ == PixelKind::Mjpeg) {
      int w = 0;
      int h = 0;
      ok = jpeg_to_rgb(src, latest.bytesused, &w, &h, &out.rgb);
      if (ok) {
        out.width = w;
        out.height = h;
      } else {
        err_ = "MJPEG decode failed";
      }
#endif
    }
    ioctl(fd_, VIDIOC_QBUF, &latest);
    return ok;
  }

  bool ok() const override { return fd_ >= 0; }
  std::string last_error() const override { return err_; }

 private:
  void release_stream() {
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
      v4l2_requestbuffers req{};
      req.count = 0;
      req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      req.memory = V4L2_MEMORY_MMAP;
      ioctl(fd_, VIDIOC_REQBUFS, &req);
    }
  }

  bool open_device(int index, int width, int height, int fps) {
    char path[64];
    std::snprintf(path, sizeof(path), "/dev/video%d", index);
    fd_ = ::open(path, O_RDWR | O_NONBLOCK);
    if (fd_ < 0) {
      if (errno == EACCES) {
        err_ = camera_absence_message(CameraAbsence::Permission);
      } else if (errno == EBUSY) {
        err_ = camera_absence_message(CameraAbsence::Busy);
      } else {
        err_ = errno_text(path);
      }
      return false;
    }
    if (!is_capture_fd(fd_)) {
      err_ = std::string(path) + " is not a capture node";
      close();
      return false;
    }

    try_auto_exposure();

    if (fps > 0) {
      v4l2_streamparm parm{};
      parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
      parm.parm.capture.timeperframe.numerator = 1;
      parm.parm.capture.timeperframe.denominator = static_cast<uint32_t>(fps);
      ioctl(fd_, VIDIOC_S_PARM, &parm);
    }

    const uint32_t wanted[] = {
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_UYVY,
#ifdef AIRMOUSE_HAVE_JPEG
        V4L2_PIX_FMT_MJPEG,
        V4L2_PIX_FMT_JPEG,
#endif
    };
    std::string format_errors;
    for (uint32_t fourcc : wanted) {
      if (!apply_format(fourcc, width, height)) {
        if (!format_errors.empty()) format_errors += " | ";
        format_errors += err_;
        continue;
      }
      if (start_stream(path)) {
        err_.clear();
        return true;
      }
      if (!format_errors.empty()) format_errors += " | ";
      format_errors += err_;
      release_stream();
    }
    err_ = format_errors.empty() ? "camera has no YUYV/UYVY/MJPEG format" : format_errors;
    close();
    return false;
  }

  void try_auto_exposure() {
    if (fd_ < 0) return;
    v4l2_control c{};
    c.id = V4L2_CID_EXPOSURE_AUTO;
    c.value = V4L2_EXPOSURE_APERTURE_PRIORITY;
    if (ioctl(fd_, VIDIOC_S_CTRL, &c) < 0) {
      c.value = V4L2_EXPOSURE_AUTO;
      ioctl(fd_, VIDIOC_S_CTRL, &c);
    }
    c = {};
    c.id = V4L2_CID_AUTOGAIN;
    c.value = 1;
    ioctl(fd_, VIDIOC_S_CTRL, &c);
  }

  bool apply_format(uint32_t fourcc, int width, int height) {
    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = static_cast<uint32_t>(width);
    fmt.fmt.pix.height = static_cast<uint32_t>(height);
    fmt.fmt.pix.pixelformat = fourcc;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    if (ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0 || fmt.fmt.pix.pixelformat != fourcc) {
      err_ = "format rejected";
      return false;
    }
    width_ = static_cast<int>(fmt.fmt.pix.width);
    height_ = static_cast<int>(fmt.fmt.pix.height);
    stride_ = static_cast<int>(fmt.fmt.pix.bytesperline);
    if (fourcc == V4L2_PIX_FMT_YUYV) kind_ = PixelKind::Yuyv;
    else if (fourcc == V4L2_PIX_FMT_UYVY) kind_ = PixelKind::Uyvy;
    else kind_ = PixelKind::Mjpeg;
    if (width_ <= 0 || height_ <= 0) {
      err_ = "invalid capture size";
      return false;
    }
    return true;
  }

  bool start_stream(const char* path) {
    v4l2_requestbuffers req{};
    req.count = 4;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (ioctl(fd_, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
      err_ = std::string(path) + " VIDIOC_REQBUFS failed";
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
        return false;
      }
      bufs_[i].length = buf.length;
      bufs_[i].start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_,
                            buf.m.offset);
      if (bufs_[i].start == MAP_FAILED) {
        bufs_[i].start = nullptr;
        err_ = "mmap failed";
        return false;
      }
      if (ioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
        err_ = "VIDIOC_QBUF failed";
        return false;
      }
    }

    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
      err_ = std::string(path) + " VIDIOC_STREAMON failed";
      return false;
    }
    return true;
  }

  int fd_ = -1;
  int width_ = 0;
  int height_ = 0;
  int stride_ = 0;
  PixelKind kind_ = PixelKind::Unknown;
  std::string err_;
  std::vector<MapBuf> bufs_;
};

}  // namespace

std::unique_ptr<Camera> create_camera() { return std::make_unique<V4l2Camera>(); }

}  // namespace airmouse

#endif
