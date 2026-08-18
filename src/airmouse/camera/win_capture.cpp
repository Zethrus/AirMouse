#include "airmouse/camera/capture.hpp"

#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mfreadwrite.h>
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
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
  const int row_bytes = std::abs(stride);
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

void bgr_to_rgb(const uint8_t* src, int width, int height, int stride, int bpp, uint8_t* dst) {
  const bool bottom_up = stride < 0;
  const int row_bytes = std::abs(stride);
  for (int y = 0; y < height; ++y) {
    const int src_y = bottom_up ? (height - 1 - y) : y;
    const uint8_t* row = src + static_cast<size_t>(src_y) * static_cast<size_t>(row_bytes);
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
  const int stride = std::abs(y_stride);
  const uint8_t* uv_base = src + static_cast<size_t>(stride) * static_cast<size_t>(height);
  for (int y = 0; y < height; ++y) {
    const uint8_t* yrow = src + static_cast<size_t>(y) * static_cast<size_t>(stride);
    const uint8_t* uvrow = uv_base + static_cast<size_t>(y / 2) * static_cast<size_t>(stride);
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
  class Callback final : public IMFSourceReaderCallback {
   public:
    explicit Callback(MfCamera* owner) : owner_(owner) {}
    void detach() { owner_ = nullptr; }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
      if (!ppv) return E_POINTER;
      if (riid == __uuidof(IUnknown) || riid == __uuidof(IMFSourceReaderCallback)) {
        *ppv = static_cast<IMFSourceReaderCallback*>(this);
        AddRef();
        return S_OK;
      }
      *ppv = nullptr;
      return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&refs_); }
    STDMETHODIMP_(ULONG) Release() override { return InterlockedDecrement(&refs_); }
    STDMETHODIMP OnReadSample(HRESULT hr, DWORD, DWORD flags, LONGLONG,
                              IMFSample* sample) override {
      if (owner_) owner_->on_sample(hr, flags, sample);
      return S_OK;
    }
    STDMETHODIMP OnFlush(DWORD) override {
      if (owner_) owner_->on_flush();
      return S_OK;
    }
    STDMETHODIMP OnEvent(DWORD, IMFMediaEvent*) override { return S_OK; }

   private:
    MfCamera* owner_ = nullptr;
    LONG refs_ = 1;
  };

 public:
  MfCamera() : cb_(this) {}
  ~MfCamera() override { close(); }

  bool open(int index, int width, int height, int fps) override {
    close();
    closing_ = false;
    flushed_ = false;

    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    com_owned_ = (com == S_OK);
    if (FAILED(com) && com != RPC_E_CHANGED_MODE) {
      err_ = "CoInitializeEx failed: " + hr_message(com);
      return false;
    }

    const HRESULT mf = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(mf)) {
      err_ = "MFStartup failed: " + hr_message(mf);
      release_com();
      return false;
    }
    mf_owned_ = (mf == S_OK);

    IMFAttributes* attrs = nullptr;
    HRESULT hr = MFCreateAttributes(&attrs, 1);
    if (FAILED(hr) || !attrs) {
      err_ = "MFCreateAttributes failed";
      close();
      return false;
    }
    attrs->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
                   MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
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
    MFCreateAttributes(&reader_attrs, 4);
    if (reader_attrs) {
      reader_attrs->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, &cb_);
      reader_attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
      reader_attrs->SetUINT32(MF_LOW_LATENCY, TRUE);
      reader_attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE);
    }
    hr = MFCreateSourceReaderFromMediaSource(source, reader_attrs, &reader_);
    if (reader_attrs) reader_attrs->Release();
    source->Release();
    if (FAILED(hr) || !reader_) {
      err_ = "MFCreateSourceReaderFromMediaSource failed: " + hr_message(hr);
      close();
      return false;
    }

    reader_->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS), FALSE);
    reader_->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), TRUE);

    if (!configure_type(width, height, fps)) {
      err_ = "camera rejected RGB/YUY2/NV12 media type";
      close();
      return false;
    }
    err_.clear();
    return request_sample();
  }

  void close() override {
    closing_ = true;
    cv_.notify_all();
    IMFSourceReader* reader = nullptr;
    {
      std::lock_guard<std::mutex> lock(mu_);
      reader = reader_;
      reader_ = nullptr;
    }
    if (reader) {
      reader->Flush(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS));
      {
        std::unique_lock<std::mutex> lock(mu_);
        cv_.wait_for(lock, std::chrono::milliseconds(400), [this] { return flushed_.load(); });
      }
      reader->Release();
    }
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (sample_) {
        sample_->Release();
        sample_ = nullptr;
      }
      pending_ = false;
    }
    if (mf_owned_) {
      MFShutdown();
      mf_owned_ = false;
    }
    release_com();
    width_ = 0;
    height_ = 0;
    kind_ = PixelKind::Unknown;
    stride_ = 0;
  }

  bool read(RgbFrame& out) override {
    if (closing_) return false;
    IMFSourceReader* reader = nullptr;
    {
      std::lock_guard<std::mutex> lock(mu_);
      reader = reader_;
    }
    if (!reader) return false;

    if (type_changed_.exchange(false)) {
      refresh_format();
    }

    IMFSample* sample = nullptr;
    {
      std::unique_lock<std::mutex> lock(mu_);
      if (!sample_ && !pending_) {
        lock.unlock();
        if (!request_sample()) return false;
        lock.lock();
      }
      if (!sample_) {
        cv_.wait_for(lock, std::chrono::milliseconds(80),
                     [this] { return sample_ != nullptr || closing_.load(); });
      }
      if (closing_ || !sample_) return false;
      sample = sample_;
      sample_ = nullptr;
    }

    IMFMediaBuffer* buffer = nullptr;
    const HRESULT bhr = sample->ConvertToContiguousBuffer(&buffer);
    sample->Release();
    request_sample();
    if (FAILED(bhr) || !buffer) return false;

    out.width = width_;
    out.height = height_;
    out.rgb.resize(static_cast<size_t>(width_) * static_cast<size_t>(height_) * 3);
    const bool ok = convert_buffer(buffer, out.rgb.data());
    buffer->Release();
    return ok;
  }

  bool ok() const override {
    std::lock_guard<std::mutex> lock(mu_);
    return reader_ != nullptr && !closing_;
  }
  std::string last_error() const override { return err_; }

 private:
  void release_com() {
    if (com_owned_) {
      CoUninitialize();
      com_owned_ = false;
    }
  }

  void on_sample(HRESULT hr, DWORD flags, IMFSample* sample) {
    if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
      type_changed_ = true;
    }
    {
      std::lock_guard<std::mutex> lock(mu_);
      pending_ = false;
      if (sample_) {
        sample_->Release();
        sample_ = nullptr;
      }
      status_ = hr;
      if (SUCCEEDED(hr) && sample && !(flags & MF_SOURCE_READERF_ERROR)) {
        sample->AddRef();
        sample_ = sample;
      } else if (FAILED(hr)) {
        err_ = "camera sample failed: " + hr_message(hr);
      }
    }
    cv_.notify_one();
  }

  void on_flush() {
    flushed_ = true;
    cv_.notify_all();
  }

  bool request_sample() {
    IMFSourceReader* reader = nullptr;
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (!reader_ || closing_ || pending_) return reader_ && !closing_;
      pending_ = true;
      reader = reader_;
    }
    const HRESULT hr = reader->ReadSample(
        static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM), 0, nullptr, nullptr, nullptr,
        nullptr);
    if (FAILED(hr)) {
      std::lock_guard<std::mutex> lock(mu_);
      pending_ = false;
      err_ = "ReadSample request failed: " + hr_message(hr);
      return false;
    }
    return true;
  }

  bool try_set_type(const GUID& sub, int width, int height, int fps) {
    IMFMediaType* type = nullptr;
    if (FAILED(MFCreateMediaType(&type)) || !type) return false;
    type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    type->SetGUID(MF_MT_SUBTYPE, sub);
    if (width > 0 && height > 0) {
      MFSetAttributeSize(type, MF_MT_FRAME_SIZE, static_cast<UINT32>(width),
                         static_cast<UINT32>(height));
    }
    if (fps > 0) {
      MFSetAttributeRatio(type, MF_MT_FRAME_RATE, static_cast<UINT32>(fps), 1);
    }
    const HRESULT hr =
        reader_->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
                                     nullptr, type);
    type->Release();
    return SUCCEEDED(hr) && refresh_format();
  }

  bool configure_type(int width, int height, int fps) {
    const GUID wanted[] = {MFVideoFormat_RGB32, MFVideoFormat_RGB24, MFVideoFormat_YUY2,
                           MFVideoFormat_NV12};
    for (const GUID& sub : wanted) {
      if (try_set_type(sub, width, height, fps)) return true;
    }
    for (const GUID& sub : wanted) {
      if (try_set_type(sub, 0, 0, 0)) return true;
    }
    for (DWORD i = 0;; ++i) {
      IMFMediaType* native = nullptr;
      if (FAILED(reader_->GetNativeMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
                                             i, &native)) ||
          !native) {
        break;
      }
      GUID major{};
      GUID sub{};
      native->GetGUID(MF_MT_MAJOR_TYPE, &major);
      native->GetGUID(MF_MT_SUBTYPE, &sub);
      HRESULT hr = E_FAIL;
      if (major == MFMediaType_Video &&
          (sub == MFVideoFormat_RGB32 || sub == MFVideoFormat_RGB24 || sub == MFVideoFormat_YUY2 ||
           sub == MFVideoFormat_NV12)) {
        hr = reader_->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
                                          nullptr, native);
      }
      native->Release();
      if (SUCCEEDED(hr) && refresh_format()) return true;
    }
    return refresh_format();
  }

  bool refresh_format() {
    if (!reader_) return false;
    IMFMediaType* type = nullptr;
    if (FAILED(reader_->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_VIDEO_STREAM),
                                            &type)) ||
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
      LONG default_stride = 0;
      if (SUCCEEDED(MFGetStrideForBitmapInfoHeader(sub.Data1, w, &default_stride))) {
        stride_ = static_cast<int>(default_stride);
      } else {
        stride_ = 0;
      }
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

  bool convert_buffer(IMFMediaBuffer* buffer, uint8_t* dst) const {
    IMF2DBuffer* buf2d = nullptr;
    if (SUCCEEDED(buffer->QueryInterface(IID_PPV_ARGS(&buf2d))) && buf2d) {
      BYTE* scan = nullptr;
      LONG pitch = 0;
      const HRESULT hr = buf2d->Lock2D(&scan, &pitch);
      bool ok = false;
      if (SUCCEEDED(hr) && scan) {
        convert(scan, static_cast<int>(pitch), dst);
        buf2d->Unlock2D();
        ok = true;
      }
      buf2d->Release();
      if (ok) return true;
    }
    BYTE* data = nullptr;
    DWORD max_len = 0;
    DWORD cur_len = 0;
    if (FAILED(buffer->Lock(&data, &max_len, &cur_len)) || !data) return false;
    convert(data, stride_, dst);
    buffer->Unlock();
    return true;
  }

  void convert(const uint8_t* src, int stride, uint8_t* dst) const {
    switch (kind_) {
      case PixelKind::Rgb24:
        bgr_to_rgb(src, width_, height_, stride, 3, dst);
        break;
      case PixelKind::Rgb32:
        bgr_to_rgb(src, width_, height_, stride, 4, dst);
        break;
      case PixelKind::Yuy2:
        yuyv_to_rgb(src, width_, height_, stride, dst);
        break;
      case PixelKind::Nv12:
        nv12_to_rgb(src, width_, height_, stride, dst);
        break;
      case PixelKind::Unknown:
        break;
    }
  }

  Callback cb_;
  IMFSourceReader* reader_ = nullptr;
  IMFSample* sample_ = nullptr;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::atomic<bool> closing_{false};
  std::atomic<bool> flushed_{false};
  std::atomic<bool> type_changed_{false};
  bool pending_ = false;
  bool mf_owned_ = false;
  bool com_owned_ = false;
  HRESULT status_ = S_OK;
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
  if (FAILED(init) && init != RPC_E_CHANGED_MODE) {
    return out;
  }
  const bool com_balanced = SUCCEEDED(init);
  const HRESULT mf = MFStartup(MF_VERSION, MFSTARTUP_LITE);
  if (FAILED(mf)) {
    if (com_balanced) CoUninitialize();
    return out;
  }
  const bool mf_owned = (mf == S_OK);
  IMFAttributes* attrs = nullptr;
  if (SUCCEEDED(MFCreateAttributes(&attrs, 1)) && attrs) {
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
  if (mf_owned) MFShutdown();
  if (com_balanced) CoUninitialize();
  return out;
}

}  // namespace airmouse

#endif
