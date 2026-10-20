#include "StartupSampler.h"

#include <QtGlobal>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace {
std::atomic<bool> g_stop{false};
std::thread g_thread;
std::vector<std::vector<DWORD64>> g_samples;
HANDLE g_target = nullptr;

std::vector<DWORD64> walk(HANDLE thread)
{
    std::vector<DWORD64> frames;
    CONTEXT ctx = {};
    ctx.ContextFlags = CONTEXT_FULL;
    if (!GetThreadContext(thread, &ctx))
        return frames;
    STACKFRAME64 frame = {};
    frame.AddrPC.Offset = ctx.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = ctx.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = ctx.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    HANDLE process = GetCurrentProcess();
    for (int i = 0; i < 64; ++i) {
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &ctx, nullptr, SymFunctionTableAccess64,
                         SymGetModuleBase64, nullptr)
            || frame.AddrPC.Offset == 0)
            break;
        frames.push_back(frame.AddrPC.Offset);
    }
    return frames;
}

std::string name(DWORD64 address)
{
    char buffer[sizeof(SYMBOL_INFO) + 256] = {};
    auto *symbol = reinterpret_cast<SYMBOL_INFO *>(buffer);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = 255;
    DWORD64 displacement = 0;
    std::string module = "?";
    IMAGEHLP_MODULE64 info = {};
    info.SizeOfStruct = sizeof(info);
    if (SymGetModuleInfo64(GetCurrentProcess(), address, &info))
        module = info.ModuleName;
    if (SymFromAddr(GetCurrentProcess(), address, &displacement, symbol))
        return module + "!" + symbol->Name;
    return module + "!?";
}
} // namespace

void startSampling(int intervalMs)
{
    HANDLE pseudo = GetCurrentThread();
    DuplicateHandle(GetCurrentProcess(), pseudo, GetCurrentProcess(), &g_target, 0, FALSE, DUPLICATE_SAME_ACCESS);
    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
    SymInitialize(GetCurrentProcess(), nullptr, TRUE);
    g_stop = false;
    g_samples.clear();
    g_samples.reserve(20000);
    g_thread = std::thread([intervalMs]() {
        while (!g_stop) {
            Sleep(intervalMs);
            if (SuspendThread(g_target) == DWORD(-1))
                continue;
            std::vector<DWORD64> frames = walk(g_target);
            ResumeThread(g_target);
            if (!frames.empty())
                g_samples.push_back(std::move(frames));
        }
    });
}

void stopSamplingAndReport(int top)
{
    g_stop = true;
    if (g_thread.joinable())
        g_thread.join();
    std::map<std::string, int> inclusive, self;
    std::map<DWORD64, std::string> cache;
    auto resolve = [&](DWORD64 a) -> const std::string & {
        auto it = cache.find(a);
        if (it == cache.end())
            it = cache.emplace(a, name(a)).first;
        return it->second;
    };
    for (const auto &frames : g_samples) {
        std::set<std::string> seen;
        for (size_t i = 0; i < frames.size(); ++i) {
            const std::string &n = resolve(frames[i]);
            if (i == 0)
                ++self[n];
            if (seen.insert(n).second)
                ++inclusive[n];
        }
    }
    const int total = int(g_samples.size());
    auto print = [&](const char *title, const std::map<std::string, int> &counts) {
        std::vector<std::pair<std::string, int>> sorted(counts.begin(), counts.end());
        std::sort(sorted.begin(), sorted.end(), [](auto &a, auto &b) { return a.second > b.second; });
        std::fprintf(stderr, "SAMPLES %s (of %d samples)\n", title, total);
        for (int i = 0; i < top && i < int(sorted.size()); ++i)
            std::fprintf(stderr, "SAMPLES %5d  %5.1f%%  %s\n", sorted[size_t(i)].second, 100.0 * sorted[size_t(i)].second / std::max(1, total),
                         sorted[size_t(i)].first.c_str());
    };
    print("inclusive", inclusive);
    print("self", self);
    std::fflush(stderr);
}
#else
void startSampling(int) {}
void stopSamplingAndReport(int) {}
#endif
