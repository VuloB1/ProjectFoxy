#include "StartupWarmup.h"

#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QString>
#include <thread>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
#ifdef Q_OS_WIN
// Creates (and drops) a hardware Direct3D 11 device: that is what loads the driver, whatever is done with the device.
void warmDirect3D()
{
    if (qEnvironmentVariable("QSG_RHI_BACKEND") != QLatin1String("d3d11"))
        return; // another backend was asked for
    HMODULE d3d11 = LoadLibraryW(L"d3d11.dll");
    if (!d3d11)
        return;
    using CreateDevice = HRESULT(WINAPI *)(void *, int, HMODULE, UINT, const void *, UINT, UINT, void **, void *, void **);
    auto create = reinterpret_cast<CreateDevice>(GetProcAddress(d3d11, "D3D11CreateDevice"));
    if (!create)
        return;
    constexpr int kHardware = 1;     // D3D_DRIVER_TYPE_HARDWARE
    constexpr UINT kSdkVersion = 7;  // D3D11_SDK_VERSION
    void *device = nullptr;
    if (SUCCEEDED(create(nullptr, kHardware, nullptr, 0, nullptr, 0, kSdkVersion, &device, nullptr, nullptr)) && device) {
        // IUnknown::Release is the third entry of every COM vtable
        auto vtable = *reinterpret_cast<void ***>(device);
        reinterpret_cast<ULONG(WINAPI *)(void *)>(vtable[2])(device);
    }
}
#endif

void warmFonts()
{
    // The interface font, in the weights it uses, with text of the scripts the interface is translated into and an
    // emoji: that is what makes Qt look at the fallback families.
    const QString sample = QStringLiteral("Aa Ññ Ç 日本語 한국어 中文 😀 →");
    for (bool bold : {false, true}) {
        QFont font(QStringLiteral("Segoe UI"), 10);
        font.setBold(bold);
        QFontMetrics(font).horizontalAdvance(sample);
    }
}
} // namespace

StartupWarmup::StartupWarmup()
{
    if (qEnvironmentVariableIsSet("PROJECTFOXY_NO_WARMUP"))
        return;
    m_helper = std::thread([]() {
        warmFonts();
#ifdef Q_OS_WIN
        warmDirect3D();
#endif
    });
}

StartupWarmup::~StartupWarmup()
{
    if (m_helper.joinable())
        m_helper.join();
}
