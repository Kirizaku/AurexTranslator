/******************************************************************************
    Copyright (C) 2026 by Daniil Nabiulin

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "screencast-overlay.h"
#include "src/utils/overlay_protocol.h"

#include <cstddef>
#include <cstring>

#ifdef Q_OS_LINUX
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#elif defined(Q_OS_WIN)
#include <dxgi1_2.h>
#endif

ScreenCastOverlay::ScreenCastOverlay(QObject *parent)
    : QThread(parent) {}

ScreenCastOverlay::~ScreenCastOverlay()
{
    cleanup();
}

void ScreenCastOverlay::init()
{
    m_loop = true;
    m_stopped = false;
    m_isProcessed = false;
}

void ScreenCastOverlay::stop()
{
    m_loop = false;
    m_stopped = true;

    m_isProcessed = true;
    m_waitCondition.wakeOne();

    wait();
}

void ScreenCastOverlay::run()
{
    while (m_loop) {
        int frameTime = 1000 / m_framerate;
        msleep(frameTime);

        if (!m_stopped) {
            m_isProcessed = false;
        }

        uint32_t h = 0, w = 0;
        void* data = nullptr;
        const bool got = requestAndReadFrame(h, w, data);

        if (got) {
            emit currentFrameBuffer(h, w, data);

            QMutexLocker locker(&m_mutex);
            while (!m_isProcessed) {
                m_waitCondition.wait(&m_mutex);
            }
        }
    }
}

bool ScreenCastOverlay::ensureHandles()
{
#ifdef Q_OS_WIN
    if (!m_evRequest)
        m_evRequest = CreateEventA(nullptr, FALSE, FALSE, AT_OVERLAY_CAPTURE_REQUEST_EVENT);
    if (!m_evReady)
        m_evReady = CreateEventA(nullptr, FALSE, FALSE, AT_OVERLAY_CAPTURE_READY_EVENT);
    return m_evRequest && m_evReady;
#elif defined(Q_OS_LINUX)
    if (!m_semRequest) {
        sem_t *s = sem_open(AT_OVERLAY_CAPTURE_REQUEST_SEM, O_CREAT, 0600, 0);
        m_semRequest = (s == SEM_FAILED) ? nullptr : s;
    }
    if (!m_semReady) {
        sem_t *s = sem_open(AT_OVERLAY_CAPTURE_READY_SEM, O_CREAT, 0600, 0);
        m_semReady = (s == SEM_FAILED) ? nullptr : s;
        if (m_semReady) {
            while (sem_trywait(m_semReady) == 0) { /* drain */ }
        }
    }
    return m_semRequest && m_semReady;
#else
    return false;
#endif
}

bool ScreenCastOverlay::ensureMapping()
{
    if (m_view) return true;

#ifdef Q_OS_WIN
    if (!m_mapping) {
        m_mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, AT_OVERLAY_FRAME_MAPPING);
        if (!m_mapping) return false;
    }
    m_view = MapViewOfFile(m_mapping, FILE_MAP_READ, 0, 0, 0);
    if (!m_view) {
        CloseHandle(m_mapping);
        m_mapping = nullptr;
        return false;
    }
    return true;
#elif defined(Q_OS_LINUX)
    if (m_shmFd < 0) {
        m_shmFd = shm_open(AT_OVERLAY_FRAME_MAPPING_POSIX, O_RDONLY, 0);
        if (m_shmFd < 0) return false;
    }
    m_view = mmap(nullptr, (size_t)AT_OVERLAY_FRAME_MAPPING_SIZE, PROT_READ, MAP_SHARED, m_shmFd, 0);
    if (m_view == MAP_FAILED) {
        m_view = nullptr;
        close(m_shmFd);
        m_shmFd = -1;
        return false;
    }
    return true;
#else
    return false;
#endif
}

#ifdef Q_OS_WIN
void ScreenCastOverlay::releaseD3D11SharedFrame()
{
    if (m_gpuMutex) { m_gpuMutex->Release(); m_gpuMutex = nullptr; }
    if (m_gpuStaging) { m_gpuStaging->Release(); m_gpuStaging = nullptr; }
    if (m_gpuTexture) { m_gpuTexture->Release(); m_gpuTexture = nullptr; }
    if (m_gpuContext) { m_gpuContext->Release(); m_gpuContext = nullptr; }
    if (m_gpuDevice) { m_gpuDevice->Release(); m_gpuDevice = nullptr; }
    m_gpuSharedHandle = 0;
    m_gpuGeneration = 0;
    m_gpuWidth = m_gpuHeight = m_gpuFormat = 0;
    m_gpuKeyedMutex = true;
}

bool ScreenCastOverlay::readD3D11SharedFrame(uint64_t sharedHandle, uint32_t width,
                                            uint32_t height, uint32_t format,
                                            uint32_t generation,
                                            uint32_t &outHeight, uint32_t &outWidth,
                                            void* &outData, bool keyedMutex, bool flipY)
{
    if (!sharedHandle || width == 0 || height == 0 ||
        width > AT_OVERLAY_FRAME_MAX_WIDTH || height > AT_OVERLAY_FRAME_MAX_HEIGHT) {
        return false;
    }

    if (m_gpuSharedHandle != sharedHandle || m_gpuGeneration != generation ||
        m_gpuWidth != width ||
        m_gpuHeight != height || m_gpuFormat != format ||
        m_gpuKeyedMutex != keyedMutex || !m_gpuTexture || !m_gpuStaging) {
        releaseD3D11SharedFrame();

        IDXGIFactory1* factory = nullptr;
        if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory)) || !factory)
            return false;

        const HANDLE handle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(sharedHandle));
        bool opened = false;
        for (UINT index = 0; !opened; ++index) {
            IDXGIAdapter1* adapter = nullptr;
            const HRESULT enumResult = factory->EnumAdapters1(index, &adapter);
            if (enumResult == DXGI_ERROR_NOT_FOUND) break;
            if (FAILED(enumResult) || !adapter) continue;

            ID3D11Device* device = nullptr;
            ID3D11DeviceContext* context = nullptr;
            D3D_FEATURE_LEVEL level = {};
            HRESULT hr = D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                &device, &level, &context);
            adapter->Release();
            if (FAILED(hr) || !device || !context) {
                if (context) context->Release();
                if (device) device->Release();
                continue;
            }

            ID3D11Texture2D* texture = nullptr;
            hr = device->OpenSharedResource(handle, __uuidof(ID3D11Texture2D),
                                             (void**)&texture);
            if (FAILED(hr) || !texture) {
                context->Release();
                device->Release();
                continue;
            }

            D3D11_TEXTURE2D_DESC desc = {};
            texture->GetDesc(&desc);
            if (desc.Width != width || desc.Height != height || desc.Format != format ||
                desc.SampleDesc.Count != 1 ||
                (desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM &&
                 desc.Format != DXGI_FORMAT_B8G8R8X8_UNORM &&
                 desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM &&
                 desc.Format != DXGI_FORMAT_R10G10B10A2_UNORM)) {
                texture->Release();
                context->Release();
                device->Release();
                continue;
            }

            IDXGIKeyedMutex* keyed = nullptr;
            if (keyedMutex) {
                hr = texture->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&keyed);
                if (FAILED(hr) || !keyed) {
                    texture->Release();
                    context->Release();
                    device->Release();
                    continue;
                }
            }

            D3D11_TEXTURE2D_DESC stagingDesc = desc;
            stagingDesc.Usage = D3D11_USAGE_STAGING;
            stagingDesc.BindFlags = 0;
            stagingDesc.MiscFlags = 0;
            stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            ID3D11Texture2D* staging = nullptr;
            hr = device->CreateTexture2D(&stagingDesc, nullptr, &staging);
            if (FAILED(hr) || !staging) {
                if (keyed) keyed->Release();
                texture->Release();
                context->Release();
                device->Release();
                continue;
            }

            m_gpuDevice = device;
            m_gpuContext = context;
            m_gpuTexture = texture;
            m_gpuStaging = staging;
            m_gpuMutex = keyed;
            m_gpuKeyedMutex = keyedMutex;
            m_gpuSharedHandle = sharedHandle;
            m_gpuGeneration = generation;
            m_gpuWidth = width;
            m_gpuHeight = height;
            m_gpuFormat = format;
            opened = true;
        }
        factory->Release();
        if (!opened) return false;
    }

    if (m_gpuMutex && m_gpuMutex->AcquireSync(1, 1000) != S_OK) return false;
    m_gpuContext->CopyResource(m_gpuStaging, m_gpuTexture);
    if (m_gpuMutex && m_gpuMutex->ReleaseSync(0) != S_OK) return false;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(m_gpuContext->Map(m_gpuStaging, 0, D3D11_MAP_READ, 0, &mapped)))
        return false;

    const size_t dstPitch = (size_t)width * 4;
    m_frameCopy.resize(dstPitch * height);
    const uint8_t* src = static_cast<const uint8_t*>(mapped.pData);
    uint8_t* dst = m_frameCopy.data();
    for (uint32_t y = 0; y < height; ++y) {
        const uint32_t srcY = flipY ? height - 1 - y : y;
        const uint8_t* srcRow = src + (size_t)srcY * mapped.RowPitch;
        uint8_t* dstRow = dst + (size_t)y * dstPitch;
        if (format == DXGI_FORMAT_R10G10B10A2_UNORM) {
            for (uint32_t x = 0; x < width; ++x) {
                uint32_t pixel;
                memcpy(&pixel, srcRow + x * 4, sizeof(pixel));
                const uint32_t r = pixel & 0x3ffu;
                const uint32_t g = (pixel >> 10) & 0x3ffu;
                const uint32_t b = (pixel >> 20) & 0x3ffu;
                dstRow[x * 4 + 0] = static_cast<uint8_t>((b * 255u + 511u) / 1023u);
                dstRow[x * 4 + 1] = static_cast<uint8_t>((g * 255u + 511u) / 1023u);
                dstRow[x * 4 + 2] = static_cast<uint8_t>((r * 255u + 511u) / 1023u);
                dstRow[x * 4 + 3] = static_cast<uint8_t>((pixel >> 30) * 85u);
            }
        } else if (format == DXGI_FORMAT_R8G8B8A8_UNORM) {
            for (uint32_t x = 0; x < width; ++x) {
                dstRow[x * 4 + 0] = srcRow[x * 4 + 2];
                dstRow[x * 4 + 1] = srcRow[x * 4 + 1];
                dstRow[x * 4 + 2] = srcRow[x * 4 + 0];
                dstRow[x * 4 + 3] = srcRow[x * 4 + 3];
            }
        } else {
            memcpy(dstRow, srcRow, dstPitch);
            if (format == DXGI_FORMAT_B8G8R8X8_UNORM) {
                for (uint32_t x = 0; x < width; ++x) dstRow[x * 4 + 3] = 0xff;
            }
        }
    }
    m_gpuContext->Unmap(m_gpuStaging, 0);

    outHeight = height;
    outWidth = width;
    outData = m_frameCopy.data();
    return true;
}
#endif

bool ScreenCastOverlay::requestAndReadFrame(uint32_t &outHeight, uint32_t &outWidth, void* &outData)
{
    if (!ensureHandles()) return false;

#ifdef Q_OS_WIN
    SetEvent(m_evRequest);
    if (WaitForSingleObject(m_evReady, 500) != WAIT_OBJECT_0) return false;
#elif defined(Q_OS_LINUX)
    sem_post(m_semRequest);
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_nsec += 500 * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) { deadline.tv_sec += 1; deadline.tv_nsec -= 1000000000L; }
    if (sem_timedwait(m_semReady, &deadline) != 0) return false;
#else
    return false;
#endif

    if (!ensureMapping()) return false;

    auto* hdr = reinterpret_cast<const at_overlay_frame_header*>(m_view);
#ifdef Q_OS_WIN
    if (hdr->format == AT_OVERLAY_FRAME_FORMAT_D3D11_SHARED) {
        const uint64_t handle = (uint64_t)hdr->reserved[0] |
                               ((uint64_t)hdr->reserved[1] << 32);
        return readD3D11SharedFrame(handle, hdr->width, hdr->height, hdr->reserved[2],
                                    hdr->frame_counter, outHeight, outWidth, outData,
                                    (hdr->stride & AT_OVERLAY_D3D11_FLAG_KEYED_MUTEX) != 0,
                                    (hdr->stride & AT_OVERLAY_D3D11_FLAG_FLIP_Y) != 0);
    }
#endif
    const uint8_t* src = reinterpret_cast<const uint8_t*>(m_view) + AT_OVERLAY_FRAME_HEADER_SIZE;

#ifdef Q_OS_LINUX
    const auto* sequence = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(m_view) + offsetof(at_overlay_frame_header, reserved));
    for (int attempt = 0; attempt < 4; ++attempt) {
        const uint32_t before = __atomic_load_n(sequence, __ATOMIC_ACQUIRE);
        if (before & 1u) continue;

        const uint32_t width = hdr->width;
        const uint32_t height = hdr->height;
        const uint32_t stride = hdr->stride;
        if (width == 0 || height == 0 ||
            width > AT_OVERLAY_FRAME_MAX_WIDTH || height > AT_OVERLAY_FRAME_MAX_HEIGHT || stride < width * 4 ||
            AT_OVERLAY_FRAME_HEADER_SIZE + (size_t)stride * height > (size_t)AT_OVERLAY_FRAME_MAPPING_SIZE) {
            return false;
        }

        const uint32_t dstPitch = width * 4;
        m_frameCopy.resize((size_t)dstPitch * height);
        for (uint32_t y = 0; y < height; ++y)
            memcpy(m_frameCopy.data() + (size_t)y * dstPitch, src + (size_t)y * stride, dstPitch);

        const uint32_t after = __atomic_load_n(sequence, __ATOMIC_ACQUIRE);
        if (before != after || (after & 1u)) continue;

        outHeight = height;
        outWidth = width;
        outData = m_frameCopy.data();
        return true;
    }

    return false;
#else
    const uint32_t width = hdr->width;
    const uint32_t height = hdr->height;
    const uint32_t stride = hdr->stride;
    if (width == 0 || height == 0 ||
        width > AT_OVERLAY_FRAME_MAX_WIDTH || height > AT_OVERLAY_FRAME_MAX_HEIGHT || stride < width * 4 ||
        AT_OVERLAY_FRAME_HEADER_SIZE + (size_t)stride * height > (size_t)AT_OVERLAY_FRAME_MAPPING_SIZE) {
        return false;
    }

    const uint32_t dstPitch = width * 4;
    m_frameCopy.resize((size_t)dstPitch * height);
    for (uint32_t y = 0; y < height; ++y)
        memcpy(m_frameCopy.data() + (size_t)y * dstPitch, src + (size_t)y * stride, dstPitch);

    outHeight = height;
    outWidth = width;
    outData = m_frameCopy.data();
    return true;
#endif
}

void ScreenCastOverlay::cleanup()
{
#ifdef Q_OS_WIN
    releaseD3D11SharedFrame();
    if (m_view) { UnmapViewOfFile(m_view); m_view = nullptr; }
    if (m_mapping) { CloseHandle(m_mapping); m_mapping = nullptr; }
    if (m_evRequest) { CloseHandle(m_evRequest); m_evRequest = nullptr; }
    if (m_evReady) { CloseHandle(m_evReady); m_evReady = nullptr; }
#elif defined(Q_OS_LINUX)
    if (m_view) { munmap(m_view, (size_t)AT_OVERLAY_FRAME_MAPPING_SIZE); m_view = nullptr; }
    if (m_shmFd >= 0) { close(m_shmFd); m_shmFd = -1; }
    if (m_semRequest) { sem_close(m_semRequest); m_semRequest = nullptr; }
    if (m_semReady) { sem_close(m_semReady); m_semReady = nullptr; }
#endif
}
