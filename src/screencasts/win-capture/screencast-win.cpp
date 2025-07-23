#include "screencast-win.h"

ScreenCast::ScreenCast(QObject *parent)
    : QThread{parent}
{}

void ScreenCast::cleanup()
{
    if (pixelData) {
        free(pixelData);
        pixelData = nullptr;
    }
}

void ScreenCast::stop()
{
    m_loop = false;
    m_stopped = true;

    m_isProcessed = true;
    m_waitCondition.wakeOne();

    wait();
}

QVector<DisplayInfo> ScreenCast::getDisplays()
{
    QVector<DisplayInfo> listDisplays;

    DISPLAY_DEVICE displayDevice;
    displayDevice.cb = sizeof(DISPLAY_DEVICE);

    DEVMODE devMode;
    devMode.dmSize = sizeof(DEVMODE);
    devMode.dmDriverExtra = 0;

    for (int i = 0; EnumDisplayDevices(NULL, i, &displayDevice, 0); i++)
    {
        if (!(displayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
            continue;

        EnumDisplaySettings(displayDevice.DeviceName, ENUM_CURRENT_SETTINGS, &devMode);

        listDisplays.append({
            devMode.dmPosition.x, devMode.dmPosition.y,
            devMode.dmPelsWidth, devMode.dmPelsHeight,
            QString::fromWCharArray(displayDevice.DeviceString)
        });
    }

    return listDisplays;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    auto* windowList = reinterpret_cast<QVector<WindowInfo>*>(lParam);

    if (IsWindowVisible(hwnd)) {
        char title[256];
        GetWindowTextA(hwnd, title, sizeof(title));

        if (strlen(title) > 0) {
            RECT rect;
            GetWindowRect(hwnd, &rect);

            WindowInfo info;
            info.window_id = hwnd;
            info.title = title;

            windowList->push_back(info);
        }
    }
    return TRUE;
}

QVector<WindowInfo> ScreenCast::getWindows()
{
    QVector<WindowInfo> windows;
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&windows));

    return windows;
}

void ScreenCast::captureDesktop(const int currentIndex)
{
    cleanup();

    QVector<DisplayInfo> listDisplays = getDisplays();

    if (currentIndex != -1 && currentIndex <= listDisplays.size())
    {
        DisplayInfo monitor = listDisplays[currentIndex];

        HDC hScreen = GetDC(NULL);
        HDC hDC = CreateCompatibleDC(hScreen);
        HBITMAP hBitmap = CreateCompatibleBitmap(hScreen, monitor.width, monitor.height);
        HGDIOBJ old_obj = SelectObject(hDC, hBitmap);

        BitBlt(hDC, 0, 0, monitor.width, monitor.height, hScreen, monitor.x, monitor.y, SRCCOPY);

        BITMAPINFOHEADER bmi = {0};
        bmi.biSize = sizeof(BITMAPINFOHEADER);
        bmi.biWidth = monitor.width;
        bmi.biHeight = -monitor.height;
        bmi.biPlanes = 1;
        bmi.biBitCount = 32;  // BGRA
        bmi.biCompression = BI_RGB;

        pixelData = new uint8_t[monitor.width * monitor.height * 4];

        GetDIBits(hDC, hBitmap, 0, monitor.height, pixelData, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);

        SelectObject(hDC, old_obj);
        DeleteDC(hDC);
        ReleaseDC(NULL, hScreen);
        DeleteObject(hBitmap);

        emit currentFrameBuffer(monitor.height, monitor.width, pixelData);
    }
}

void ScreenCast::captureWindow(const HWND &window_id)
{
    cleanup();

    if (window_id == 0 || !IsWindow(window_id))
        return;

    RECT rect;
    GetWindowRect(window_id, &rect);

    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    HDC hdcScreen = GetDC(nullptr);
    HDC hDC = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, width, height);

    SelectObject(hDC, hBitmap);

    PrintWindow(window_id, hDC, PW_RENDERFULLCONTENT);

    BITMAPINFO bmi;
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32; // BGRA
    bmi.bmiHeader.biCompression = BI_RGB;

    pixelData = new uint8_t[width * height * 4];

    GetDIBits(hDC, hBitmap, 0, height, pixelData, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);

    DeleteDC(hDC);
    DeleteObject(hBitmap);

    if (pixelData != nullptr && width * height > 0) {
        emit currentFrameBuffer(height, width, pixelData);
    }
}

void ScreenCast::run()
{
    while(m_loop)
    {
        int frameTime = 1000 / m_framerate;
        msleep(frameTime);

        if (!m_stopped) {
            m_isProcessed = false;
        }

        if (m_isCaptureDesktop) {
            captureDesktop(m_currentDisplay);
        }
        else {
            captureWindow(m_currentWindow);
        }

        QMutexLocker locker(&m_mutex);
        while(!m_isProcessed) {
            m_waitCondition.wait(&m_mutex);
        }
    }
    cleanup();
}
