#include "airmouse/camera/capture.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "ole32.lib")

namespace airmouse {
namespace {

void yuyv_to_rgb(const uint8_t* src, int width, int height, int stride, uint8_t* dst) {
  auto clamp = [](int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); };
  for (int y = 0; y < height; ++y) {
    const uint8_t* row = src + static_cast<size_t>(y) * static_cast<size_t>(stride);
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

void bgr_to_rgb(const uint8_t* src, int width, int height, int stride, bool bottom_up,
                int bpp, uint8_t* dst) {
  for (int y = 0; y < height; ++y) {
    const int src_y = bottom_up ? (height - 1 - y) : y;
    const uint8_t* row = src + static_cast<size_t>(src_y) * static_cast<size_t>(std::abs(stride));
    for (int x = 0; x < width; ++x) {
      const uint8_t* px = row + x * bpp;
      *dst++ = px[2];
      *dst++ = px[1];
      *dst++ = px[0];
    }
  }
}

void nv12_to_rgb(const uint8_t* src, int width, int height, int y_stride, uint8_t* dst) {
  auto clamp = [](int v) { return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v)); };
  const uint8_t* uv_base = src + static_cast<size_t>(y_stride) * static_cast<size_t>(height);
  const int uv_stride = y_stride;
  for (int y = 0; y < height; ++y) {
    const uint8_t* yrow = src + static_cast<size_t>(y) * static_cast<size_t>(y_stride);
    const uint8_t* uvrow = uv_base + static_cast<size_t>(y / 2) * static_cast<size_t>(uv_stride);
    for (int x = 0; x < width; ++x) {
      const int Y = static_cast<int>(yrow[x]) - 16;
      const int U = static_cast<int>(uvrow[(x & ~1) + 0]) - 128;
      const int V = static_cast<int>(uvrow[(x & ~1) + 1]) - 128;
      *dst++ = clamp((298 * Y + 409 * V + 128) >> 8);
      *dst++ = clamp((298 * Y - 100 * U - 208 * V + 128) >> 8);
      *dst++ = clamp((298 * Y + 516 * U + 128) >> 8);
    }
  }
}

std::string hr_message(HRESULT hr) {
  char buf[64];
  std::snprintf(buf, sizeof(buf), "HRESULT 0x%08lX", static_cast<unsigned long>(hr));
  return buf;
}

enum class PixelKind { Rgb24, Rgb32, Yuy2, Nv12, Unknown };

class MfCamera final : public Camera {
 public:
  ~MfCamera() override { close(); }

  bool open(int index, int width, int height, int fps) override {
    close();
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    com_inited_ = SUCCEEDED(hr) || hr == S_FALSE || hr == RPC_E_CHANGED_MODE;
    hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(hr)) {
      err_ = "MFStartup failed: " + hr_message(hr);
      return false;
    }
    mf_started_ = true;

    IMFAttributes* attrs = nullptr;
    hr = MFCreateAttributes(&attrs, 2);
    if (FAILED(hr)) {
      err_ = "MFCreateAttributes failed";
      close();
      return false;
    }
    attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    IMFActivate** devices = nullptr;
    UINT32 count = 0;
    hr = MFEnumDeviceSources(attrs, &devices, &count);
    attrs->Release();
    if (FAILED(hr) || count == 0) {
      err_ = "no Windows camera devices found";
      close();
      return false;
    }
    if (index < 0 || static_cast<UINT32>(index) >= count) {
      err_ = "camera index out of range";
      for (UINT32 i = 0; i < count; ++i) devices[i]->Release();
      CoTaskMemFree(devices);
      close();
      return false;
    }

    IMFMediaSource* source = nullptr;
    hr = devices[static_cast<UINT32>(index)]->ActivateObject(IID_PPV_ARGS(&source));
    for (UINT32 i = 0; i < count; ++i) devices[i]->Release();
    CoTaskMemFree(devices);
    if (FAILED(hr) || !source) {
      err_ = "could not activate camera: " + hr_message(hr);
      close();
      return false;
    }

    IMFAttributes* reader_attrs = nullptr;
    MFCreateAttributes(&reader_attrs, 1);
    if (reader_attrs) {
      reader_attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
    }
    hr = MFCreateSourceReaderFromMediaSource(source, reader_attrs, &reader_);
    if (reader_attrs) reader_attrs->Release();
    source->Release();
    if (FAILED(hr) || !reader_) {
      err_ = "MFCreateSourceReaderFromMediaSource failed: " + hr_message(hr);
      close();
      return false;
    }

    reader_->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
    reader_->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);

    if (!configure_type(width, height, fps)) {
      err_ = "camera rejected RGB/YUY2/NV12 media type";
      close();
      return false;
    }
    err_.clear();
    return true;
  }

  void close() override {
    if (reader_) {
      reader_->Release();
      reader_ = nullptr;
    }
    if (mf_started_) {
      MFShutdown();
      mf_started_ = false;
    }
    width_ = 0;
    height_ = 0;
    kind_ = PixelKind::Unknown;
    stride_ = 0;
  }

  bool read(RgbFrame& out) override {
    if (!reader_) return false;
    IMFSample* sample = nullptr;
    DWORD flags = 0;
    const HRESULT hr =
        reader_->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags,
                            nullptr, &sample);
    if (FAILED(hr)) {
      err_ = "ReadSample failed: " + hr_message(hr);
      return false;
    }
    if (flags & MF_SOURCE_READERF_ERROR) {
      err_ = "camera stream error";
      if (sample) sample->Release();
      return false;
    }
    if (!sample) return false;

    IMFMediaBuffer* buffer = nullptr;
    HRESULT bhr = sample->ConvertToContiguousBuffer(&buffer);
    sample->Release();
    if (FAILED(bhr) || !buffer) return false;

    BYTE* data = nullptr;
    DWORD max_len = 0;
    DWORD cur_len = 0;
    if (FAILED(buffer->Lock(&data, &max_len, &cur_len)) || !data) {
      buffer->Release();
      return false;
    }

    out.width = width_;
    out.height = height_;
    out.rgb.resize(static_cast<size_t>(width_ * height_ * 3));
    convert(data, out.rgb.data());
    buffer->Unlock();
    buffer->Release();
    return true;
  }

  bool ok() const override { return reader_ != nullptr; }
  std::string last_error() const override { return err_; }

 private:
  bool configure_type(int width, int height, int fps) {
    const GUID wanted[] = {MFVideoFormat_RGB24, MFVideoFormat_RGB32, MFVideoFormat_YUY2,
                           MFVideoFormat_NV12};
    for (const GUID& sub : wanted) {
      IMFMediaType* type = nullptr;
      if (FAILED(MFCreateMediaType(&type))) continue;
      type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
      type->SetGUID(MF_MT_SUBTYPE, sub);
      MFSetAttributeSize(type, MF_MT_FRAME_SIZE, static_cast<UINT32>(width),
                         static_cast<UINT32>(height));
      if (fps > 0) {
        MFSetAttributeRatio(type, MF_MT_FRAME_RATE, static_cast<UINT32>(fps), 1);
      }
      const HRESULT hr =
          reader_->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, type);
      type->Release();
      if (SUCCEEDED(hr) && refresh_format()) return true;
    }
    return refresh_format();
  }

  bool refresh_format() {
    IMFMediaType* type = nullptr;
    if (FAILED(reader_->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &type)) ||
        !type) {
      return false;
    }
    GUID sub{};
    type->GetGUID(MF_MT_SUBTYPE, &sub);
    UINT32 w = 0;
    UINT32 h = 0;
    MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &w, &h);
    width_ = static_cast<int>(w);
    height_ = static_cast<int>(h);
    UINT32 stride = 0;
    if (SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride))) {
      stride_ = static_cast<int>(stride);
    } else {
      stride_ = 0;
    }
    if (sub == MFVideoFormat_RGB24) {
      kind_ = PixelKind::Rgb24;
      if (stride_ == 0) stride_ = (width_ * 3 + 3) & ~3;
    } else if (sub == MFVideoFormat_RGB32) {
      kind_ = PixelKind::Rgb32;
      if (stride_ == 0) stride_ = width_ * 4;
    } else if (sub == MFVideoFormat_YUY2) {
      kind_ = PixelKind::Yuy2;
      if (stride_ == 0) stride_ = width_ * 2;
    } else if (sub == MFVideoFormat_NV12) {
      kind_ = PixelKind::Nv12;
      if (stride_ == 0) stride_ = width_;
    } else {
      kind_ = PixelKind::Unknown;
    }
    type->Release();
    return kind_ != PixelKind::Unknown && width_ > 0 && height_ > 0;
  }

  void convert(const uint8_t* src, uint8_t* dst) const {
    switch (kind_) {
      case PixelKind::Rgb24:
        bgr_to_rgb(src, width_, height_, stride_, stride_ < 0, 3, dst);
        break;
      case PixelKind::Rgb32:
        bgr_to_rgb(src, width_, height_, stride_, stride_ < 0, 4, dst);
        break;
      case PixelKind::Yuy2:
        yuyv_to_rgb(src, width_, height_, std::abs(stride_), dst);
        break;
      case PixelKind::Nv12:
        nv12_to_rgb(src, width_, height_, std::abs(stride_), dst);
        break;
      case PixelKind::Unknown:
        break;
    }
  }

  IMFSourceReader* reader_ = nullptr;
  bool mf_started_ = false;
  bool com_inited_ = false;
  int width_ = 0;
  int height_ = 0;
  int stride_ = 0;
  PixelKind kind_ = PixelKind::Unknown;
  std::string err_;
};

}  // namespace

std::unique_ptr<Camera> create_camera() { return std::make_unique<MfCamera>(); }

std::vector<int> list_camera_indices() {
  std::vector<int> out;
  const HRESULT init = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(MFStartup(MF_VERSION, MFSTARTUP_LITE))) {
    if (SUCCEEDED(init)) CoUninitialize();
    return out;
  }
  IMFAttributes* attrs = nullptr;
  if (SUCCEEDED(MFCreateAttributes(&attrs, 1))) {
    attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                   MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    IMFActivate** devices = nullptr;
    UINT32 count = 0;
    if (SUCCEEDED(MFEnumDeviceSources(attrs, &devices, &count))) {
      for (UINT32 i = 0; i < count; ++i) {
        out.push_back(static_cast<int>(i));
        devices[i]->Release();
      }
      CoTaskMemFree(devices);
    }
    attrs->Release();
  }
  MFShutdown();
  if (SUCCEEDED(init)) CoUninitialize();
  return out;
}

}  // namespace airmouse

#endif
