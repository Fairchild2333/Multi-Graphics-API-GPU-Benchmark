#include "pch.h"
#include "MainWindow.xaml.h"
#include "MainWindow.xaml.g.hpp"

#include "gpu_engine.h"          // shared result/engine declarations
#include "benchmark_results.h"   // gpu_bench::LoadResults
#include "path_service.h"        // gpu_bench::paths::{Results,Captures}Directory
#include "i18n.h"

#include <microsoft.ui.xaml.window.h>
#include <commctrl.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>

#include <winrt/Windows.UI.Text.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstring>
#include <chrono>
#include <cwctype>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#pragma comment(lib, "comctl32.lib")

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Input;
using namespace Microsoft::UI::Xaml::Media;
using namespace Microsoft::UI::Xaml::Media::Animation;
using namespace Windows::Foundation;

// Local IBufferByteAccess (avoid <robuffer.h>: its global ::Windows namespace
// clashes with the using-directives above).
struct __declspec(uuid("905a0fef-bc53-11df-8c49-001e4fc686da")) __declspec(novtable)
IBufferByteAccessLocal : ::IUnknown
{
    virtual HRESULT __stdcall Buffer(uint8_t** value) = 0;
};

namespace winrt::gpu_bench_gui::implementation
{
namespace
{
    // Load the embedded app icon (app.rc id 101) into a WriteableBitmap so it
    // can be shown in the XAML UI (title-bar logo + About). Built at runtime
    // from the Win32 resource, so it works without ms-appx packaging.
    Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap loadAppIconBitmap(int px)
    {
        HICON hIcon = reinterpret_cast<HICON>(LoadImageW(
            GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101),
            IMAGE_ICON, px, px, LR_DEFAULTCOLOR));
        if (!hIcon) return nullptr;

        ICONINFO ii{};
        if (!GetIconInfo(hIcon, &ii)) { DestroyIcon(hIcon); return nullptr; }

        BITMAP bm{};
        GetObject(ii.hbmColor, sizeof(bm), &bm);
        const int w = bm.bmWidth, h = bm.bmHeight;

        BITMAPINFO bi{};
        bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth       = w;
        bi.bmiHeader.biHeight      = -h;   // top-down rows
        bi.bmiHeader.biPlanes      = 1;
        bi.bmiHeader.biBitCount    = 32;
        bi.bmiHeader.biCompression = BI_RGB;

        std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * 4);
        HDC hdc = GetDC(nullptr);
        GetDIBits(hdc, ii.hbmColor, 0, h, pixels.data(), &bi, DIB_RGB_COLORS);
        ReleaseDC(nullptr, hdc);
        DeleteObject(ii.hbmColor);
        DeleteObject(ii.hbmMask);
        DestroyIcon(hIcon);

        bool anyAlpha = false;
        for (size_t i = 3; i < pixels.size(); i += 4)
            if (pixels[i]) { anyAlpha = true; break; }
        if (!anyAlpha)
            for (size_t i = 3; i < pixels.size(); i += 4) pixels[i] = 255;

        // WriteableBitmap expects premultiplied BGRA.
        for (size_t i = 0; i < pixels.size(); i += 4)
        {
            const uint8_t a = pixels[i + 3];
            pixels[i + 0] = static_cast<uint8_t>(pixels[i + 0] * a / 255);
            pixels[i + 1] = static_cast<uint8_t>(pixels[i + 1] * a / 255);
            pixels[i + 2] = static_cast<uint8_t>(pixels[i + 2] * a / 255);
        }

        Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap wb(w, h);
        uint8_t* dst = nullptr;
        wb.PixelBuffer().as<IBufferByteAccessLocal>()->Buffer(&dst);
        if (dst) memcpy(dst, pixels.data(), pixels.size());
        wb.Invalidate();
        return wb;
    }

    hstring locText(const char* en, const char* zh) { return winrt::to_hstring(i18n::tr(en, zh)); }
    IInspectable locContent(const char* en, const char* zh) { return winrt::box_value(locText(en, zh)); }

    // ComboBox disabled visuals barely dim selected text; force the theme
    // disabled brush onto the control and summary item while Detecting.
    Brush disabledTextBrush()
    {
        if (auto res = Application::Current().Resources().TryLookup(
                box_value(L"TextFillColorDisabledBrush")))
            if (auto brush = res.try_as<Brush>())
                return brush;
        return SolidColorBrush(Windows::UI::Color{ 255, 160, 160, 160 });
    }

    void applyComboEnabledLook(ComboBox const& box, bool enabled)
    {
        box.IsEnabled(enabled);
        box.Opacity(enabled ? 1.0 : 0.55);
        if (enabled)
        {
            box.ClearValue(Control::ForegroundProperty());
            if (auto item = box.SelectedItem().try_as<ComboBoxItem>())
                item.ClearValue(Control::ForegroundProperty());
        }
        else
        {
            auto brush = disabledTextBrush();
            box.Foreground(brush);
            if (auto item = box.SelectedItem().try_as<ComboBoxItem>())
                item.Foreground(brush);
        }
    }

    hstring u8(std::string const& s)
    {
        if (s.empty()) return {};
        int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring w(n, L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
        return hstring(w);
    }

    void appendGuiCrashLog(char const* where, char const* detail)
    {
        try
        {
            wchar_t dir[MAX_PATH]{};
            DWORD n = ::GetEnvironmentVariableW(L"LOCALAPPDATA", dir, MAX_PATH);
            if (n == 0 || n >= MAX_PATH) return;
            std::filesystem::path path =
                std::filesystem::path(dir) / L"GpuComputeBenchmark" / L"gui-crash.log";
            std::error_code ec;
            std::filesystem::create_directories(path.parent_path(), ec);
            std::ofstream out(path, std::ios::app);
            if (!out) return;
            SYSTEMTIME st{};
            ::GetLocalTime(&st);
            out << st.wYear << '-' << st.wMonth << '-' << st.wDay << ' '
                << st.wHour << ':' << st.wMinute << ':' << st.wSecond
                << " [" << (where ? where : "?") << "] "
                << (detail ? detail : "") << '\n';
        }
        catch (...) {}
    }

    // Keep WinUI TextBox updates bounded; multi-job CLI dumps can otherwise spike memory.
    std::string clipForUi(std::string s, std::size_t maxBytes = 256u * 1024u)
    {
        if (s.size() <= maxBytes) return s;
        return std::string("…[truncated]\n") + s.substr(s.size() - maxBytes);
    }

    std::string resultDatePrefix(std::string const& timestamp)
    {
        return timestamp.size() >= 10 ? timestamp.substr(0, 10) : timestamp;
    }

    std::shared_ptr<void> makeManualResetEvent()
    {
        HANDLE raw = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!raw) return {};
        return std::shared_ptr<void>(raw, [](void* p)
        {
            if (p) ::CloseHandle(static_cast<HANDLE>(p));
        });
    }

    std::filesystem::path pathFromUtf8(std::string const& value)
    {
        return std::filesystem::path(u8(value).c_str());
    }

    std::string pathToUtf8(std::filesystem::path const& value)
    {
        return winrt::to_string(hstring(value.wstring()));
    }

    std::string findEngineExe()
    {
        wchar_t buf[MAX_PATH]{};
        ::GetModuleFileNameW(nullptr, buf, MAX_PATH);
        const std::filesystem::path modulePath(buf);
        std::filesystem::path dir = modulePath.parent_path();

        // Installed/staged builds keep the CLI worker and GUI together.
        auto adjacent = dir / L"gpu_benchmark.exe";
        if (std::filesystem::exists(adjacent)) return pathToUtf8(adjacent);

        // Developer-tree compatibility for an IDE-launched GUI.
        for (int i = 0; i < 8; ++i)
        {
            for (auto rel : { L"build\\Release\\gpu_benchmark.exe", L"build\\gpu_benchmark.exe" })
            {
                auto cand = dir / rel;
                if (std::filesystem::exists(cand)) return pathToUtf8(cand);
            }
            if (!dir.has_parent_path()) break;
            dir = dir.parent_path();
        }

        // Benchmarks and driver probing require process isolation. Returning
        // the GUI itself here would recursively launch another GUI worker.
        return {};
    }

    std::filesystem::path findRenderDocCommand(std::string const& enginePath)
    {
        std::error_code ec;
        const auto binDir = std::filesystem::absolute(
            pathFromUtf8(enginePath), ec).parent_path();
        if (!ec)
        {
            const std::filesystem::path candidates[] = {
                binDir / L"tools" / L"RenderDoc" / L"renderdoccmd.exe",
                binDir / L".." / L"tools" / L"RenderDoc" / L"renderdoccmd.exe",
                binDir / L".." / L".." / L"tools" / L"RenderDoc" / L"renderdoccmd.exe",
            };
            for (auto const& candidate : candidates)
            {
                const auto command = candidate.lexically_normal();
                if (std::filesystem::is_regular_file(command, ec) && !ec)
                    return command;
                ec.clear();
            }
        }

        wchar_t programFiles[32768]{};
        const DWORD len = ::GetEnvironmentVariableW(
            L"ProgramFiles", programFiles,
            static_cast<DWORD>(std::size(programFiles)));
        if (len > 0 && len < std::size(programFiles))
        {
            const auto command = std::filesystem::path(programFiles)
                / L"RenderDoc" / L"renderdoccmd.exe";
            if (std::filesystem::is_regular_file(command, ec) && !ec)
                return command;
        }
        return {};
    }

    // Python is a developer fallback only. Release packages must eventually
    // ship a frozen report worker and must never claim reports were generated
    // merely because the benchmark itself completed.
    std::filesystem::path findPythonExecutable()
    {
        wchar_t path[32768]{};
        const DWORD len = ::SearchPathW(
            nullptr, L"python.exe", nullptr,
            static_cast<DWORD>(std::size(path)), path, nullptr);
        if (len > 0 && len < std::size(path))
            return std::filesystem::path(path);
        return {};
    }

    struct CliResult { std::string output; int exitCode; };

    // CommandLineToArgvW-compatible quoting for one Windows command-line
    // argument.  GPU workers are real child processes so a driver fault or a
    // RenderDoc hook failure cannot terminate the WinUI orchestrator.
    std::wstring quoteWindowsArg(std::wstring const& arg)
    {
        if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos)
            return arg;

        std::wstring quoted{ L'\"' };
        size_t slashes = 0;
        for (wchar_t ch : arg)
        {
            if (ch == L'\\')
            {
                ++slashes;
                continue;
            }
            if (ch == L'\"')
            {
                quoted.append(slashes * 2 + 1, L'\\');
                quoted.push_back(L'\"');
                slashes = 0;
                continue;
            }
            quoted.append(slashes, L'\\');
            slashes = 0;
            quoted.push_back(ch);
        }
        quoted.append(slashes * 2, L'\\');
        quoted.push_back(L'\"');
        return quoted;
    }

    CliResult captureProcess(std::filesystem::path const* application,
                             std::wstring command,
                             std::wstring const& cwd,
                             DWORD timeoutMs,
                             HANDLE cancelEvent = nullptr)
    {
        SECURITY_ATTRIBUTES security{};
        security.nLength = sizeof(security);
        security.bInheritHandle = TRUE;

        // CreatePipe cannot produce an overlapped read handle. Use a private
        // byte-mode named pipe so the reader can always be stopped with an
        // event + CancelIoEx, even if a crashed driver leaves a writer alive.
        static std::atomic_uint64_t nextPipeId{ 0 };
        const std::wstring pipeName = L"\\\\.\\pipe\\Mangekyo.GuiCapture." +
            std::to_wstring(::GetCurrentProcessId()) + L"." +
            std::to_wstring(nextPipeId.fetch_add(1, std::memory_order_relaxed));
        HANDLE readPipe = ::CreateNamedPipeW(
            pipeName.c_str(),
            PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED |
                FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1, 64u * 1024u, 64u * 1024u, 0, nullptr);
        if (readPipe == INVALID_HANDLE_VALUE)
            return { "[Process] CreateNamedPipe failed (" +
                     std::to_string(::GetLastError()) + ").\n", -1 };

        HANDLE connectEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!connectEvent)
        {
            const DWORD error = ::GetLastError();
            ::CloseHandle(readPipe);
            return { "[Process] Create pipe connect event failed (" +
                     std::to_string(error) + ").\n", -1 };
        }
        OVERLAPPED connection{};
        connection.hEvent = connectEvent;
        const BOOL connectedImmediately = ::ConnectNamedPipe(readPipe, &connection);
        const DWORD connectError = connectedImmediately
            ? ERROR_SUCCESS : ::GetLastError();
        if (!connectedImmediately && connectError != ERROR_IO_PENDING)
        {
            ::CloseHandle(connectEvent);
            ::CloseHandle(readPipe);
            return { "[Process] ConnectNamedPipe failed (" +
                     std::to_string(connectError) + ").\n", -1 };
        }

        HANDLE writePipe = ::CreateFileW(
            pipeName.c_str(), GENERIC_WRITE, 0, &security, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (writePipe == INVALID_HANDLE_VALUE)
        {
            const DWORD error = ::GetLastError();
            if (!connectedImmediately)
            {
                ::CancelIoEx(readPipe, &connection);
                DWORD ignored = 0;
                ::GetOverlappedResult(readPipe, &connection, &ignored, TRUE);
            }
            ::CloseHandle(connectEvent);
            ::CloseHandle(readPipe);
            return { "[Process] Open capture pipe writer failed (" +
                     std::to_string(error) + ").\n", -1 };
        }
        if (!connectedImmediately)
        {
            const DWORD connected = ::WaitForSingleObject(connectEvent, 5000);
            DWORD ignored = 0;
            if (connected != WAIT_OBJECT_0 ||
                !::GetOverlappedResult(
                    readPipe, &connection, &ignored, FALSE))
            {
                const DWORD error = connected == WAIT_TIMEOUT ? ERROR_TIMEOUT
                    : connected == WAIT_FAILED ? ::GetLastError()
                    : ::GetLastError();
                ::CancelIoEx(readPipe, &connection);
                ::GetOverlappedResult(readPipe, &connection, &ignored, TRUE);
                ::CloseHandle(writePipe);
                ::CloseHandle(connectEvent);
                ::CloseHandle(readPipe);
                return { "[Process] Capture pipe connection failed (" +
                         std::to_string(error) + ").\n", -1 };
            }
        }
        ::CloseHandle(connectEvent);

        HANDLE nullInput = ::CreateFileW(
            L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (nullInput == INVALID_HANDLE_VALUE)
        {
            const DWORD error = ::GetLastError();
            ::CloseHandle(writePipe);
            ::CloseHandle(readPipe);
            return { "[Process] Could not open NUL for stdin (" +
                     std::to_string(error) + ").\n", -1 };
        }

        // Create containment before the process. Starting suspended and
        // assigning the job before ResumeThread closes the crash-before-assign
        // race that otherwise lets a modal crash reporter escape the GUI.
        HANDLE job = ::CreateJobObjectW(nullptr, nullptr);
        if (!job)
        {
            const DWORD error = ::GetLastError();
            ::CloseHandle(nullInput);
            ::CloseHandle(writePipe);
            ::CloseHandle(readPipe);
            return { "[Process] CreateJobObject failed (" +
                     std::to_string(error) + ").\n", -1 };
        }
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!::SetInformationJobObject(
                job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
        {
            const DWORD error = ::GetLastError();
            ::CloseHandle(job);
            ::CloseHandle(nullInput);
            ::CloseHandle(writePipe);
            ::CloseHandle(readPipe);
            return { "[Process] SetInformationJobObject failed (" +
                     std::to_string(error) + ").\n", -1 };
        }

        // Restrict inheritance to the three redirected standard handles.  A
        // plain bInheritHandles=TRUE can leak unrelated WinUI/COM handles into
        // a worker and keep GUI resources (or this pipe) alive indefinitely.
        SIZE_T attributeBytes = 0;
        ::InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeBytes);
        std::vector<std::byte> attributeStorage(attributeBytes);
        STARTUPINFOEXW startup{};
        startup.StartupInfo.cb = sizeof(startup);
        startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        startup.StartupInfo.hStdInput = nullInput;
        startup.StartupInfo.hStdOutput = writePipe;
        startup.StartupInfo.hStdError = writePipe;
        startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(
            attributeStorage.data());
        if (attributeBytes == 0 || !::InitializeProcThreadAttributeList(
                startup.lpAttributeList, 1, 0, &attributeBytes))
        {
            const DWORD error = ::GetLastError();
            ::CloseHandle(job);
            ::CloseHandle(nullInput);
            ::CloseHandle(writePipe);
            ::CloseHandle(readPipe);
            return { "[Process] InitializeProcThreadAttributeList failed (" +
                     std::to_string(error) + ").\n", -1 };
        }
        HANDLE inheritedHandles[] = { nullInput, writePipe };
        if (!::UpdateProcThreadAttribute(
                startup.lpAttributeList, 0,
                PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                inheritedHandles, sizeof(inheritedHandles), nullptr, nullptr))
        {
            const DWORD error = ::GetLastError();
            ::DeleteProcThreadAttributeList(startup.lpAttributeList);
            ::CloseHandle(job);
            ::CloseHandle(nullInput);
            ::CloseHandle(writePipe);
            ::CloseHandle(readPipe);
            return { "[Process] UpdateProcThreadAttribute failed (" +
                     std::to_string(error) + ").\n", -1 };
        }
        PROCESS_INFORMATION process{};
        std::vector<wchar_t> mutableCommand(command.begin(), command.end());
        mutableCommand.push_back(L'\0');
        const BOOL created = ::CreateProcessW(
            application && !application->empty() ? application->c_str() : nullptr,
            mutableCommand.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT,
            nullptr,
            cwd.empty() ? nullptr : cwd.c_str(),
            &startup.StartupInfo, &process);
        const DWORD createError = created ? ERROR_SUCCESS : ::GetLastError();
        ::DeleteProcThreadAttributeList(startup.lpAttributeList);
        ::CloseHandle(nullInput);
        ::CloseHandle(writePipe);

        if (!created)
        {
            ::CloseHandle(job);
            ::CloseHandle(readPipe);
            return { "[Process] CreateProcess failed (" +
                     std::to_string(createError) + ").\n", -1 };
        }

        if (!::AssignProcessToJobObject(job, process.hProcess))
        {
            const DWORD error = ::GetLastError();
            ::TerminateProcess(process.hProcess, error);
            ::WaitForSingleObject(process.hProcess, 5000);
            ::CloseHandle(process.hThread);
            ::CloseHandle(process.hProcess);
            ::CloseHandle(job);
            ::CloseHandle(readPipe);
            return { "[Process] AssignProcessToJobObject failed (" +
                     std::to_string(error) + ").\n", -1 };
        }
        if (::ResumeThread(process.hThread) == static_cast<DWORD>(-1))
        {
            const DWORD error = ::GetLastError();
            ::TerminateJobObject(job, error);
            ::WaitForSingleObject(process.hProcess, 5000);
            ::CloseHandle(process.hThread);
            ::CloseHandle(process.hProcess);
            ::CloseHandle(job);
            ::CloseHandle(readPipe);
            return { "[Process] ResumeThread failed (" +
                     std::to_string(error) + ").\n", -1 };
        }
        ::CloseHandle(process.hThread);

        std::string output;
        DWORD readError = ERROR_SUCCESS;
        bool outputTruncated = false;
        HANDLE stopReaderEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        HANDLE readEvent = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!stopReaderEvent || !readEvent)
        {
            const DWORD error = ::GetLastError();
            ::TerminateJobObject(job, error);
            ::WaitForSingleObject(process.hProcess, 5000);
            if (readEvent) ::CloseHandle(readEvent);
            if (stopReaderEvent) ::CloseHandle(stopReaderEvent);
            ::CloseHandle(process.hProcess);
            ::CloseHandle(job);
            ::CloseHandle(readPipe);
            return { "[Process] Create reader event failed (" +
                     std::to_string(error) + ").\n", -1 };
        }

        std::thread reader([&]()
        {
          try
          {
            constexpr std::size_t kMaxOutputBytes = 4u * 1024u * 1024u;
            constexpr ULONGLONG kStopDrainMs = 1000;
            std::array<char, 8192> buffer{};
            ULONGLONG stopDeadline = 0;

            auto appendOutput = [&](DWORD bytesRead)
            {
                output.append(buffer.data(), bytesRead);
                if (output.size() > kMaxOutputBytes)
                {
                    output.erase(0, output.size() - kMaxOutputBytes);
                    outputTruncated = true;
                }
            };

            for (;;)
            {
                const bool stopping =
                    ::WaitForSingleObject(stopReaderEvent, 0) == WAIT_OBJECT_0;
                DWORD requestBytes = static_cast<DWORD>(buffer.size());
                if (stopping)
                {
                    if (stopDeadline == 0)
                        stopDeadline = ::GetTickCount64() + kStopDrainMs;
                    if (::GetTickCount64() >= stopDeadline) break;

                    DWORD available = 0;
                    if (!::PeekNamedPipe(readPipe, nullptr, 0, nullptr,
                                         &available, nullptr))
                    {
                        readError = ::GetLastError();
                        break;
                    }
                    if (available == 0) break;
                    requestBytes = (std::min)(available, requestBytes);
                }

                ::ResetEvent(readEvent);
                OVERLAPPED readOperation{};
                readOperation.hEvent = readEvent;
                DWORD bytesRead = 0;
                if (::ReadFile(readPipe, buffer.data(), requestBytes,
                               &bytesRead, &readOperation))
                {
                    if (bytesRead == 0) break;
                    appendOutput(bytesRead);
                    continue;
                }

                const DWORD startError = ::GetLastError();
                if (startError == ERROR_BROKEN_PIPE || startError == ERROR_NO_DATA)
                {
                    readError = startError;
                    break;
                }
                if (startError != ERROR_IO_PENDING)
                {
                    readError = startError;
                    break;
                }

                // Put the read event first so simultaneous read/stop signals
                // preserve already-completed output before entering drain mode.
                HANDLE waits[] = { readEvent, stopReaderEvent };
                const DWORD wait = ::WaitForMultipleObjects(
                    static_cast<DWORD>(std::size(waits)), waits, FALSE, INFINITE);
                if (wait == WAIT_OBJECT_0)
                {
                    if (!::GetOverlappedResult(
                            readPipe, &readOperation, &bytesRead, FALSE))
                    {
                        const DWORD error = ::GetLastError();
                        if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA)
                            readError = error;
                        else if (error != ERROR_OPERATION_ABORTED)
                            readError = error;
                        if (error != ERROR_OPERATION_ABORTED) break;
                    }
                    else if (bytesRead != 0)
                    {
                        appendOutput(bytesRead);
                    }
                    continue;
                }

                // A stop or wait failure cancels this exact overlapped read.
                // The named-pipe provider, rather than the GPU driver, owns the
                // request; wait for its cancellation completion before the
                // stack OVERLAPPED is reused or destroyed.
                const DWORD waitError = wait == WAIT_FAILED
                    ? ::GetLastError() : ERROR_SUCCESS;
                ::CancelIoEx(readPipe, &readOperation);
                if (::GetOverlappedResult(
                        readPipe, &readOperation, &bytesRead, TRUE))
                {
                    if (bytesRead != 0) appendOutput(bytesRead);
                }
                else
                {
                    const DWORD error = ::GetLastError();
                    if (error != ERROR_OPERATION_ABORTED &&
                        error != ERROR_BROKEN_PIPE && error != ERROR_NO_DATA)
                        readError = error;
                }
                if (wait == WAIT_FAILED)
                {
                    readError = waitError;
                    break;
                }
            }
          }
          catch (...)
          {
              readError = ERROR_OUTOFMEMORY;
          }
        });

        DWORD waitResult = WAIT_FAILED;
        DWORD waitError = ERROR_SUCCESS;
        bool timedOut = false;
        bool cancelled = false;
        if (cancelEvent)
        {
            // Process first so a natural exit wins over a late Cancel click.
            HANDLE waits[] = { process.hProcess, cancelEvent };
            waitResult = ::WaitForMultipleObjects(
                static_cast<DWORD>(std::size(waits)), waits, FALSE, timeoutMs);
            waitError = waitResult == WAIT_FAILED ? ::GetLastError() : ERROR_SUCCESS;
            timedOut = waitResult == WAIT_TIMEOUT;
            cancelled = waitResult == WAIT_OBJECT_0 + 1;
        }
        else
        {
            waitResult = ::WaitForSingleObject(process.hProcess, timeoutMs);
            waitError = waitResult == WAIT_FAILED ? ::GetLastError() : ERROR_SUCCESS;
            timedOut = waitResult == WAIT_TIMEOUT;
        }

        if (cancelled || timedOut || waitResult == WAIT_FAILED)
        {
            const DWORD killCode = cancelled ? ERROR_CANCELLED
                : timedOut ? ERROR_TIMEOUT : waitError;
            ::TerminateJobObject(job, killCode);
            ::WaitForSingleObject(process.hProcess, 5000);
        }

        DWORD exitCode = 1;
        ::GetExitCodeProcess(process.hProcess, &exitCode);

        // Closing the job first terminates any RenderDoc Bug Reporter child.
        // Signal the overlapped reader to cancel a pending read, drain buffered
        // bytes for a bounded interval, and exit even if a writer escaped.
        ::CloseHandle(job);
        ::SetEvent(stopReaderEvent);
        reader.join();
        ::CloseHandle(readEvent);
        ::CloseHandle(stopReaderEvent);
        ::CloseHandle(readPipe);
        ::CloseHandle(process.hProcess);

        if (readError != ERROR_SUCCESS && readError != ERROR_BROKEN_PIPE &&
            readError != ERROR_NO_DATA)
            output += "\n[Process] stdout pipe failed (" +
                      std::to_string(readError) + ").\n";
        if (outputTruncated)
            output.insert(0, "[Process] Output truncated to the last 4 MiB.\n");
        if (cancelled)
        {
            output += "\n[Process] Cancelled.\n";
            return { std::move(output), static_cast<int>(ERROR_CANCELLED) };
        }
        if (timedOut)
        {
            output += "\n[Process] Timed out and terminated.\n";
            return { std::move(output), -2 };
        }
        if (waitResult == WAIT_FAILED)
        {
            output += "\n[Process] WaitForSingleObject failed (" +
                      std::to_string(waitError) + ").\n";
            return { std::move(output), -3 };
        }
        return { std::move(output), static_cast<int>(exitCode) };
    }

    CliResult captureCliProcess(std::vector<std::string> const& args,
                                DWORD timeoutMs = 60u * 60u * 1000u,
                                HANDLE cancelEvent = nullptr)
    {
        if (args.empty()) return { "[GUI worker] Empty command.\n", -1 };

        const std::filesystem::path executable = pathFromUtf8(args.front());
        std::wstring command = quoteWindowsArg(executable.wstring());
        for (size_t i = 1; i < args.size(); ++i)
        {
            command += L' ';
            command += quoteWindowsArg(std::wstring(u8(args[i]).c_str()));
        }
        return captureProcess(&executable, std::move(command),
                              executable.parent_path().wstring(), timeoutMs,
                              cancelEvent);
    }

    // Every GUI benchmark carries either --time or --benchmark. Give timed
    // workloads their requested duration plus a generous startup/capture
    // allowance, while fixed-frame workloads receive a finite one-hour
    // watchdog. This keeps a VMware driver or RenderDoc failure from hanging a
    // full GPU matrix forever without penalising intentionally long timed runs.
    DWORD gpuWorkerTimeoutMs(std::vector<std::string> const& args)
    {
        for (auto const& a : args)
            if (a == "--no-time-limit")
                return INFINITE;

        constexpr std::uint64_t kMinuteMs = 60u * 1000u;
        constexpr std::uint64_t kDefaultMs = 60u * kMinuteMs;
        constexpr std::uint64_t kGraceMs = 5u * kMinuteMs;
        constexpr std::uint64_t kMinimumMs = 6u * kMinuteMs;
        constexpr std::uint64_t kMaximumMs = 24u * 60u * kMinuteMs;

        std::uint64_t timeout = kDefaultMs;
        for (size_t i = 0; i + 1 < args.size(); ++i)
        {
            if (args[i] != "--time") continue;
            try
            {
                size_t consumed = 0;
                const double seconds = std::stod(args[i + 1], &consumed);
                if (consumed == args[i + 1].size() &&
                    std::isfinite(seconds) && seconds >= 0.0)
                {
                    const auto requested = static_cast<std::uint64_t>(
                        (std::min)(seconds * 1000.0,
                                   static_cast<double>(kMaximumMs)));
                    timeout = (std::max)(kMinimumMs,
                        (std::min)(kMaximumMs, requested + kGraceMs));
                }
            }
            catch (...) {}
            break;
        }
        return static_cast<DWORD>(timeout);
    }

    // ProductVersion from this GUI's VERSIONINFO (app.rc), e.g. "0.1.3".
    std::wstring readGuiProductVersion()
    {
        wchar_t path[MAX_PATH]{};
        if (!GetModuleFileNameW(nullptr, path, MAX_PATH))
            return L"";

        DWORD handle = 0;
        const DWORD size = GetFileVersionInfoSizeW(path, &handle);
        if (!size) return L"";

        std::vector<char> block(size);
        if (!GetFileVersionInfoW(path, 0, size, block.data()))
            return L"";

        struct LANGANDCODEPAGE { WORD language; WORD codePage; };
        LANGANDCODEPAGE* translate = nullptr;
        UINT translateBytes = 0;
        if (!VerQueryValueW(block.data(), L"\\VarFileInfo\\Translation",
                            reinterpret_cast<void**>(&translate), &translateBytes) ||
            !translate || translateBytes < sizeof(LANGANDCODEPAGE))
            return L"";

        wchar_t subBlock[64]{};
        swprintf_s(subBlock, L"\\StringFileInfo\\%04x%04x\\ProductVersion",
                   translate[0].language, translate[0].codePage);
        wchar_t* value = nullptr;
        UINT valueChars = 0;
        if (!VerQueryValueW(block.data(), subBlock,
                            reinterpret_cast<void**>(&value), &valueChars) ||
            !value || valueChars == 0)
            return L"";
        return value;
    }

    std::string trimScoreLine(std::string line)
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t b = line.find_first_not_of(" \t");
        return b == std::string::npos ? std::string{} : line.substr(b);
    }

    std::string localizeScoreLineOne(std::string line)
    {
        // CLI prints "VRAM rate:" (device-local) or "RAM rate:" (system memory).
        // Older builds used "Memory rate:" — treat those as VRAM for display.
        auto replaceRate = [&](char const* from, char const* en, char const* zh) {
            auto pos = line.find(from);
            if (pos != std::string::npos)
                line.replace(pos, std::strlen(from),
                             i18n::currentLang() == i18n::Lang::Zh ? zh : en);
        };
        // VRAM must be handled before RAM: "VRAM rate:" contains "RAM rate:"
        // as a substring, so the reverse order produced "V内存速率:".
        replaceRate("VRAM rate:", "VRAM rate:", "显存速率:");
        replaceRate("RAM rate:", "RAM rate:", "内存速率:");
        replaceRate("Memory rate:", "VRAM rate:", "显存速率:");
        if (i18n::currentLang() != i18n::Lang::Zh) return line;
        struct Pair { char const* en; char const* zh; };
        constexpr Pair pairs[] = {
            { "Avg FPS:", "平均帧率:" },
            { "Compute rate:", "计算速率:" },
            { "Burn rate:", "Burn 速率:" },
            { "Stress rate:", "压力速率:" },
            { "Fill rate:", "填充速率:" },
            { "Render rate:", "渲染速率:" },
            { "Vol rate:", "体积速率:" },
            { "Fluid rate:", "流体速率:" },
            { "Liquid rate:", "液体速率:" },
            { "Peak FP", "峰值 FP" },
            { "Peak INT", "峰值 INT" },
        };
        for (auto const& p : pairs)
        {
            auto pos = line.find(p.en);
            if (pos != std::string::npos)
                line.replace(pos, std::strlen(p.en), p.zh);
        }
        return line;
    }

    // Multi-API runs show one score line per worker; localise each line.
    std::string localizeScoreLine(std::string const& text)
    {
        std::string out;
        size_t start = 0;
        while (true)
        {
            const auto nl = text.find('\n', start);
            out += localizeScoreLineOne(text.substr(
                start, nl == std::string::npos ? std::string::npos : nl - start));
            if (nl == std::string::npos) break;
            out += '\n';
            start = nl + 1;
        }
        return out;
    }

    // Returns an English score line (CLI wording). Call localizeScoreLine() for UI.
    std::string extractScore(std::string const& out)
    {
        const char* keys[] = { "RAM rate:", "VRAM rate:", "Memory rate:", "Compute rate:", "Burn rate:", "Stress rate:", "Fill rate:", "Render rate:", "Vol rate:", "Fluid rate:", "Liquid rate:", "Peak FP", "Peak INT" };
        std::istringstream ss(out);
        std::string line, rate, fps;
        while (std::getline(ss, line))
        {
            auto trimmed = trimScoreLine(line);
            if (trimmed.empty()) continue;
            if (fps.empty() && trimmed.find("Avg FPS:") != std::string::npos)
                fps = trimmed;
            if (rate.empty())
            {
                for (auto k : keys)
                    if (trimmed.find(k) != std::string::npos)
                    {
                        rate = trimmed;
                        break;
                    }
            }
        }

        std::string combined;
        if (!rate.empty()) combined = rate;
        if (!fps.empty())
        {
            // Collapse "Avg FPS:      123" → "Avg FPS: 123".
            auto fpsLabel = fps;
            auto colon = fpsLabel.find(':');
            if (colon != std::string::npos)
            {
                auto value = fpsLabel.substr(colon + 1);
                auto first = value.find_first_not_of(" \t");
                value = first == std::string::npos ? std::string{} : value.substr(first);
                fpsLabel = fpsLabel.substr(0, colon + 1) + " " + value;
            }
            if (!combined.empty()) combined += "  ·  ";
            combined += fpsLabel;
        }
        return combined;
    }

    // Stream summary's "Working set:  512.00 MiB"; 0 when absent.
    double extractWorkingSetMiB(std::string const& out)
    {
        double v = 0.0;
        std::istringstream ss(out);
        std::string line;
        while (std::getline(ss, line))
        {
            auto t = trimScoreLine(line);
            if (t.rfind("Working set:", 0) == 0)
            {
                try { v = std::stod(t.substr(12)); } catch (...) {}
            }
        }
        return v;
    }

    // Adapter the worker actually used, from the last "GPU:" summary line.
    std::string extractGpuName(std::string const& out)
    {
        std::istringstream ss(out);
        std::string line, name;
        while (std::getline(ss, line))
        {
            auto trimmed = trimScoreLine(line);
            if (trimmed.rfind("GPU:", 0) == 0)
            {
                auto value = trimmed.substr(4);
                auto b = value.find_first_not_of(" \t");
                if (b != std::string::npos) name = value.substr(b);
            }
        }
        // Drop the DX feature-level suffix, e.g. "… (FL 12_1)".
        auto fl = name.find(" (FL ");
        if (fl != std::string::npos) name.resize(fl);
        return name;
    }

    std::string padCol(std::string s, size_t w)
    {
        if (s.size() < w) s.append(w - s.size(), ' ');
        else if (s.size() > w && w > 1) s = s.substr(0, w - 1) + " ";
        return s;
    }

    CliResult runProcess(std::wstring cmd, std::wstring const& cwd,
                         DWORD timeoutMs = 10u * 60u * 1000u,
                         HANDLE cancelEvent = nullptr)
    {
        return captureProcess(nullptr, std::move(cmd), cwd, timeoutMs, cancelEvent);
    }

    enum class CpuLineType { Other, Progress, Result, Error, Meta, Topology };

    struct CpuProtocolLine
    {
        CpuLineType type{ CpuLineType::Other };
        std::map<std::string, std::string> fields;
    };

    std::string trimAscii(std::string value)
    {
        auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
            return std::isspace(c) != 0;
        });
        auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
            return std::isspace(c) != 0;
        }).base();
        if (first >= last) return {};
        return std::string(first, last);
    }

    std::string lowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    CpuProtocolLine parseCpuProtocolLine(std::string const& raw)
    {
        CpuProtocolLine parsed;
        std::istringstream stream(raw);
        std::string token;
        if (!std::getline(stream, token, '\t')) return parsed;
        token = trimAscii(token);
        if (token == "CPU_PROGRESS") parsed.type = CpuLineType::Progress;
        else if (token == "CPU_RESULT") parsed.type = CpuLineType::Result;
        else if (token == "CPU_ERROR") parsed.type = CpuLineType::Error;
        else if (token == "CPU_META") parsed.type = CpuLineType::Meta;
        else if (token == "CPU_TOPOLOGY") parsed.type = CpuLineType::Topology;
        else return parsed;

        // Unknown keys are deliberately retained. The UI consumes only the
        // fields it understands, so the CLI protocol can grow without breaking
        // older GUI builds.
        size_t positional = 0;
        while (std::getline(stream, token, '\t'))
        {
            token = trimAscii(token);
            if (token.empty()) continue;
            const auto equals = token.find('=');
            if (equals == std::string::npos)
            {
                parsed.fields["field" + std::to_string(positional++)] = token;
                continue;
            }
            auto key = lowerAscii(trimAscii(token.substr(0, equals)));
            auto value = trimAscii(token.substr(equals + 1));
            if (!key.empty()) parsed.fields[std::move(key)] = std::move(value);
        }
        return parsed;
    }

    std::string cpuField(CpuProtocolLine const& line,
                         std::initializer_list<char const*> names)
    {
        for (auto name : names)
            if (auto it = line.fields.find(name); it != line.fields.end())
                return it->second;
        return {};
    }

    std::optional<double> cpuNumber(CpuProtocolLine const& line,
                                    std::initializer_list<char const*> names)
    {
        const auto text = cpuField(line, names);
        if (text.empty()) return std::nullopt;
        try
        {
            size_t used = 0;
            const double value = std::stod(text, &used);
            if (used == text.size() && std::isfinite(value)) return value;
        }
        catch (...) {}
        return std::nullopt;
    }

    std::optional<int> cpuInteger(CpuProtocolLine const& line,
                                  std::initializer_list<char const*> names)
    {
        const auto value = cpuNumber(line, names);
        if (!value || *value < static_cast<double>(INT_MIN) ||
            *value > static_cast<double>(INT_MAX)) return std::nullopt;
        return static_cast<int>(*value);
    }

    // Validate the child protocol on the reader thread.  Exit code 0 alone is
    // not sufficient: a truncated pipe or an incompatible executable must not
    // be presented as a completed benchmark.
    struct CpuProtocolAudit
    {
        bool sawMeta{ false };
        bool sawSummary{ false };
        bool sawMulti{ false };
        bool sawEngineError{ false };
        int expectedCores{ -1 };
        std::string workloadVersion;
        std::set<int> topology;
        std::set<int> coreResults;
        std::string error;

        void fail(std::string message)
        {
            if (error.empty()) error = std::move(message);
        }

        bool countMatches(CpuProtocolLine const& line)
        {
            const auto count = cpuInteger(line, { "core_count" });
            if (!count || *count != expectedCores)
            {
                fail("CPU protocol core_count does not match CPU_META");
                return false;
            }
            return true;
        }

        void observe(CpuProtocolLine const& line)
        {
            if (line.type == CpuLineType::Other || line.type == CpuLineType::Progress)
                return;
            if (line.type == CpuLineType::Error)
            {
                sawEngineError = true;
                return;
            }
            if (line.type == CpuLineType::Meta)
            {
                if (sawMeta)
                {
                    fail("CPU protocol contains duplicate CPU_META records");
                    return;
                }
                sawMeta = true;
                workloadVersion = cpuField(line, { "workload_version" });
                expectedCores = cpuInteger(line, { "logical_count" }).value_or(-1);
                if (workloadVersion.rfind("cpu_mixed_v1_", 0) != 0)
                    fail("CPU protocol workload_version is missing or incompatible");
                if (expectedCores <= 0)
                    fail("CPU protocol logical_count is missing or invalid");
                return;
            }

            if (!sawMeta)
            {
                fail("CPU protocol data arrived before CPU_META");
                return;
            }
            if (line.type == CpuLineType::Topology)
            {
                const int core = cpuInteger(line, { "core_index" }).value_or(-1);
                if (core < 0 || core >= expectedCores || !topology.insert(core).second)
                    fail("CPU protocol topology is incomplete or contains duplicate cores");
                return;
            }
            if (line.type != CpuLineType::Result) return;

            const auto version = cpuField(line, { "workload_version" });
            if (version != workloadVersion)
                fail("CPU_RESULT workload_version does not match CPU_META");

            const auto kind = cpuField(line, { "kind", "type" });
            if (kind == "core")
            {
                countMatches(line);
                const int core = cpuInteger(line, { "core_index" }).value_or(-1);
                if (core < 0 || core >= expectedCores || !coreResults.insert(core).second)
                    fail("CPU protocol contains an invalid or duplicate core result");
                if (cpuInteger(line, { "valid" }).value_or(0) != 1)
                    fail("CPU protocol contains an invalid per-core result");
            }
            else if (kind == "summary")
            {
                if (sawSummary) fail("CPU protocol contains duplicate per-core summaries");
                sawSummary = true;
                countMatches(line);
                if (cpuInteger(line, { "completed" }).value_or(-1) != expectedCores ||
                    cpuInteger(line, { "invalid_count" }).value_or(-1) != 0 ||
                    cpuInteger(line, { "valid" }).value_or(0) != 1)
                    fail("CPU per-core summary is incomplete or invalid");
            }
            else if (kind == "multi")
            {
                if (sawMulti) fail("CPU protocol contains duplicate all-core results");
                sawMulti = true;
                countMatches(line);
                if (cpuInteger(line, { "thread_count" }).value_or(-1) != expectedCores ||
                    cpuInteger(line, { "valid" }).value_or(0) != 1)
                    fail("CPU all-core result is incomplete or invalid");
            }
        }

        std::string validate(std::string const& selectedMode)
        {
            if (!error.empty()) return error;
            if (!sawMeta) return "CPU protocol is missing CPU_META";
            if (static_cast<int>(topology.size()) != expectedCores)
                return "CPU protocol topology count does not match CPU_META";
            if (sawEngineError) return "CPU engine reported an error";

            const bool needPerCore = selectedMode == "per-core" || selectedMode == "all";
            const bool needMulti = selectedMode == "multi" || selectedMode == "all";
            if (needPerCore && static_cast<int>(coreResults.size()) != expectedCores)
                return "CPU protocol is missing one or more per-core results";
            if (needPerCore && !sawSummary)
                return "CPU protocol is missing the per-core summary";
            if (needMulti && !sawMulti)
                return "CPU protocol is missing the all-core result";
            return {};
        }
    };

    std::string formatCpuScore(double score, std::string const& unit)
    {
        std::ostringstream out;
        out.imbue(std::locale::classic());
        out << std::fixed << std::setprecision(score >= 1000.0 ? 1 : 3) << score;
        if (!unit.empty()) out << ' ' << unit;
        return out.str();
    }

    std::string win32ErrorText(DWORD error)
    {
        wchar_t* buffer = nullptr;
        const DWORD chars = ::FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
        if (!chars || !buffer) return "Win32 error " + std::to_string(error);
        std::wstring message(buffer, chars);
        ::LocalFree(buffer);
        while (!message.empty() && std::iswspace(message.back())) message.pop_back();
        return to_string(hstring(message));
    }

    Uri fileUri(std::filesystem::path const& p)
    {
        std::wstring s = p.wstring();
        for (auto& c : s) if (c == L'\\') c = L'/';
        return Uri(hstring(L"file:///" + s));
    }

    // Engine prints each RenderDoc capture as a "  -> <path>.rdc" line; collect them.
    std::vector<std::string> parseCapturePaths(std::string const& out)
    {
        std::vector<std::string> paths;
        std::istringstream ss(out); std::string line;
        while (std::getline(ss, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto pos = line.find("-> ");
            if (pos == std::string::npos) continue;
            std::string p = line.substr(pos + 3);
            while (!p.empty() && p.front() == ' ') p.erase(p.begin());
            while (!p.empty() && p.back()  == ' ') p.pop_back();
            if (p.size() > 4 && p.substr(p.size() - 4) == ".rdc") paths.push_back(p);
        }
        return paths;
    }

    // Min-size clamp + theme-matched resize fill (avoids the black flash during
    // a fast resize while Mica catches up). Copied from sdr2hdr.
    LRESULT CALLBACK WindowSubclassProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l,
                                        UINT_PTR, DWORD_PTR refData)
    {
        switch (msg)
        {
        case WM_GETMINMAXINFO:
        {
            UINT dpi = GetDpiForWindow(hwnd);
            double scale = dpi > 0 ? dpi / 96.0 : 1.0;
            auto* mmi = reinterpret_cast<MINMAXINFO*>(l);
            mmi->ptMinTrackSize.x = static_cast<LONG>(1120 * scale);
            mmi->ptMinTrackSize.y = static_cast<LONG>(680 * scale);
            return 0;
        }
        case WM_ERASEBKGND:
            if (auto brush = reinterpret_cast<HBRUSH>(refData))
            {
                RECT rc{}; GetClientRect(hwnd, &rc);
                FillRect(reinterpret_cast<HDC>(w), &rc, brush);
                return 1;
            }
            break;
        default: break;
        }
        return DefSubclassProc(hwnd, msg, w, l);
    }

    std::string detectCpuName()
    {
        HKEY key{};
        if (::RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                0, KEY_READ, &key) == ERROR_SUCCESS)
        {
            char buf[256]{}; DWORD sz = sizeof(buf);
            if (::RegQueryValueExA(key, "ProcessorNameString", nullptr, nullptr,
                                   reinterpret_cast<LPBYTE>(buf), &sz) == ERROR_SUCCESS)
            {
                ::RegCloseKey(key);
                std::string s(buf);
                while (!s.empty() && s.front() == ' ') s.erase(s.begin());
                while (!s.empty() && s.back()  == ' ') s.pop_back();
                return s;
            }
            ::RegCloseKey(key);
        }
        return "CPU";
    }

    bool isSoftwareGpu(std::string const& name)
    {
        return name.find("Basic Render") != std::string::npos
            || name.find("WARP") != std::string::npos
            || name.find("Software") != std::string::npos;
    }

    // "" = All; otherwise a YYYY-MM-DD cutoff (keep timestamps >= cutoff).
    std::string cutoffFor(int rangeIdx)
    {
        if (rangeIdx <= 0) return {};
        int days = (rangeIdx == 1) ? 0 : (rangeIdx == 2) ? 6 : 29;
        time_t t = time(nullptr) - static_cast<time_t>(days) * 86400;
        tm lt{}; localtime_s(&lt, &t);
        char buf[16];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
        return buf;
    }

    // Different APIs report the same physical GPU under decorated names
    // ("…RTX 5090", "…RTX 5090 (FL 12_1)", "…RTX 5090/PCIe/SSE2") — collapse to
    // a common base so they merge in the filter.
    std::string normalizeGpuName(std::string n)
    {
        auto p = n.find(" (");
        if (p != std::string::npos) n = n.substr(0, p);
        p = n.find('/');
        if (p != std::string::npos) n = n.substr(0, p);
        p = n.find(" #");                       // "…RTX 5090 #1" -> base
        if (p != std::string::npos) n = n.substr(0, p);
        // OEM-branded Adreno: "Mi Pad 5 Adreno 640 GPU" -> "Qualcomm Adreno 640"
        p = n.find("Adreno");
        if (p != std::string::npos)
        {
            std::string tail = n.substr(p);   // "Adreno 640 GPU" or "Adreno 640"
            auto g = tail.find(" GPU");
            if (g != std::string::npos) tail = tail.substr(0, g);
            n = "Qualcomm " + tail;
        }
        while (!n.empty() && n.back() == ' ') n.pop_back();
        return n.empty() ? "(unknown)" : n;
    }

    // Brand bucket from a (normalized) device name. Software renderers (WARP,
    // Basic Render Driver, SVGA…) run on the CPU, so they group under "CPU".
    std::string gpuBrand(std::string const& n)
    {
        auto has = [&](const char* s) { return n.find(s) != std::string::npos; };
        if (has("WARP") || has("Basic Render") || has("SVGA") || has("Software") || has("Microsoft"))
            return "CPU / Software";
        if (has("NVIDIA") || has("GeForce") || has("Quadro") || has("Tesla")) return "NVIDIA";
        if (has("AMD") || has("Radeon") || has("FirePro") || has("Vega"))     return "AMD";
        if (has("Intel") || has("Arc"))                                       return "Intel";
        if (has("Apple"))                                                     return "Apple";
        if (has("Adreno") || has("Qualcomm") || has("Snapdragon"))            return "Qualcomm";
        if (has("Mali") || has("ARM"))                                        return "ARM";
        if (has("Moore Threads") || has("MTT"))                               return "Moore Threads";
        return "Other";
    }

    // Series sub-bucket within a brand.
    std::string gpuSeries(std::string const& brand, std::string const& n)
    {
        auto has = [&](const char* s) { return n.find(s) != std::string::npos; };
        if (brand == "NVIDIA")
        {
            if (has("RTX"))    return "GeForce RTX";
            if (has("GTX"))    return "GeForce GTX";
            if (has("Quadro")) return "Quadro";
            if (has("Tesla"))  return "Tesla";
            return "GeForce / Other";
        }
        if (brand == "AMD")
        {
            if (has("RX"))      return "Radeon RX";
            if (has("Vega"))    return "Radeon Vega";
            if (has("FirePro")) return "Radeon Pro / FirePro";
            if (has("HD"))      return "Radeon HD";
            return "Radeon / Other";
        }
        if (brand == "Intel")
        {
            if (has("Arc"))                return "Arc";
            if (has("Iris"))               return "Iris";
            if (has("UHD") || has("HD"))   return "HD / UHD";
            return "Other";
        }
        if (brand == "Qualcomm")       return "Adreno";
        if (brand == "ARM")            return "Mali";
        if (brand == "Apple")          return "Apple Silicon";
        if (brand == "Moore Threads")  return "MTT";
        return brand;
    }

    // Forward declaration (defined below localizeTimestamp).
    std::string normalizeCpuName(std::string n);

    // A result's place in the brand -> series -> device tree. Software renderers
    // (WARP etc.) group by the CPU model they ran on.
    struct GpuLeaf { std::string brand, series, device; };
    GpuLeaf leafOf(gpu_bench::BenchmarkResult const& r)
    {
        std::string dev = normalizeGpuName(r.deviceName);
        std::string brand = gpuBrand(dev);
        std::string series = (brand == "CPU / Software")
            ? normalizeCpuName(r.cpuName.empty() ? "Unknown CPU" : r.cpuName)
            : gpuSeries(brand, dev);
        return { brand, series, dev };
    }
    std::string filterKey(GpuLeaf const& l)
    {
        return l.brand + "\x1f" + l.series + "\x1f" + l.device;
    }

    // CalendarDatePicker -> "YYYY-MM-DD" ("" if unset).
    std::string pickerDate(Controls::CalendarDatePicker const& p)
    {
        auto ref = p.Date();
        if (!ref) return {};
        time_t t = winrt::clock::to_time_t(ref.Value());
        tm lt{}; localtime_s(&lt, &t);
        char buf[16];
        snprintf(buf, sizeof(buf), "%04d-%02d-%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
        return buf;
    }

    std::string groupedNumber(std::uint32_t n)
    {
        std::string s = std::to_string(n);
        for (int i = static_cast<int>(s.size()) - 3; i > 0; i -= 3)
            s.insert(static_cast<size_t>(i), ",");
        return s;
    }

    std::string particleLabel(std::uint32_t n)
    {
        if (n == 0) return "-";
        if (n == 65536u)    return "65K (65,536)";
        if (n == 1048576u)  return "1M (1,048,576)";
        if (n == 4194304u)  return "4M (4,194,304)";
        if (n == 16777216u) return "16M (16,777,216)";
        return groupedNumber(n);
    }

    // Consolas treats most CJK as double-width. History columns used byte length
    // for padding, so Chinese workload labels shifted Particles/Score/FPS.
    size_t utf8DisplayWidth(std::string_view s)
    {
        size_t width = 0;
        for (size_t i = 0; i < s.size();)
        {
            auto c = static_cast<unsigned char>(s[i]);
            if (c < 0x80)
            {
                ++width;
                ++i;
                continue;
            }

            std::uint32_t cp = 0;
            size_t len = 1;
            if ((c & 0xE0) == 0xC0 && i + 1 < s.size())
            {
                cp = (std::uint32_t(c & 0x1F) << 6) |
                     (std::uint32_t(static_cast<unsigned char>(s[i + 1]) & 0x3F));
                len = 2;
            }
            else if ((c & 0xF0) == 0xE0 && i + 2 < s.size())
            {
                cp = (std::uint32_t(c & 0x0F) << 12) |
                     (std::uint32_t(static_cast<unsigned char>(s[i + 1]) & 0x3F) << 6) |
                     (std::uint32_t(static_cast<unsigned char>(s[i + 2]) & 0x3F));
                len = 3;
            }
            else if ((c & 0xF8) == 0xF0 && i + 3 < s.size())
            {
                cp = (std::uint32_t(c & 0x07) << 18) |
                     (std::uint32_t(static_cast<unsigned char>(s[i + 1]) & 0x3F) << 12) |
                     (std::uint32_t(static_cast<unsigned char>(s[i + 2]) & 0x3F) << 6) |
                     (std::uint32_t(static_cast<unsigned char>(s[i + 3]) & 0x3F));
                len = 4;
            }
            else
            {
                ++width;
                ++i;
                continue;
            }

            const bool wide =
                (cp >= 0x1100 && cp <= 0x115F) ||
                cp == 0x2329 || cp == 0x232A ||
                (cp >= 0x2E80 && cp <= 0xA4CF) ||
                (cp >= 0xAC00 && cp <= 0xD7A3) ||
                (cp >= 0xF900 && cp <= 0xFAFF) ||
                (cp >= 0xFE10 && cp <= 0xFE19) ||
                (cp >= 0xFE30 && cp <= 0xFE6F) ||
                (cp >= 0xFF00 && cp <= 0xFF60) ||
                (cp >= 0xFFE0 && cp <= 0xFFE6) ||
                (cp >= 0x20000 && cp <= 0x3FFFD);
            width += wide ? 2 : 1;
            i += len;
        }
        return width;
    }

    std::string padDisplay(std::string s, size_t width)
    {
        const size_t dw = utf8DisplayWidth(s);
        if (dw < width) s.append(width - dw, ' ');
        return s;
    }

    // Convert stored ISO timestamp "YYYY-MM-DD HH:MM:SS" to locale-appropriate
    // display format. English uses Australian DD/MM/YYYY 12-hour; Chinese keeps
    // YYYY-MM-DD 24-hour.
    std::string localizeTimestamp(std::string const& ts)
    {
        if (ts.size() < 19) return ts;  // malformed, return as-is
        if (i18n::currentLang() == i18n::Lang::Zh) return ts;  // 中文: 年-月-日 24h

        // Parse "YYYY-MM-DD HH:MM:SS"
        int Y = 0, M = 0, D = 0, h = 0, m = 0, s = 0;
        if (sscanf(ts.c_str(), "%d-%d-%d %d:%d:%d", &Y, &M, &D, &h, &m, &s) < 6)
            return ts;

        // Australian: DD/MM/YYYY hh:mm:ss AM/PM
        const char* ampm = (h < 12) ? "AM" : "PM";
        int h12 = h % 12;
        if (h12 == 0) h12 = 12;
        char buf[32];
        snprintf(buf, sizeof(buf), "%02d/%02d/%04d %d:%02d:%02d %s",
                 D, M, Y, h12, m, s, ampm);
        return buf;
    }

    // Clean up CPU names for display: strip "Nth Gen" Intel prefixes,
    // collapse internal whitespace.
    std::string normalizeCpuName(std::string n)
    {
        // Strip "12th Gen ", "13th Gen ", etc.
        for (auto const* prefix : { "th Gen ", "st Gen ", "nd Gen ", "rd Gen " })
        {
            auto pos = n.find(prefix);
            if (pos != std::string::npos && pos <= 4)
            {
                n = n.substr(pos + strlen(prefix));
                break;
            }
        }
        // Collapse multiple spaces to single
        std::string out;
        out.reserve(n.size());
        bool prevSpace = false;
        for (char c : n)
        {
            if (c == ' ') { if (!prevSpace) out += c; prevSpace = true; }
            else          { out += c; prevSpace = false; }
        }
        // Trim
        while (!out.empty() && out.front() == ' ') out.erase(out.begin());
        while (!out.empty() && out.back()  == ' ') out.pop_back();
        // Add "Qualcomm" prefix for Snapdragon CPUs
        if (out.find("Snapdragon") != std::string::npos
            && out.find("Qualcomm") == std::string::npos)
            out = "Qualcomm " + out;
        return out.empty() ? "(unknown)" : out;
    }

    std::string workloadLabel(std::string const& id)
    {
        if (id == "stream")     return i18n::tr("Particle — Memory Throughput", "粒子 —— 内存吞吐");
        if (id == "headless_compute")
            return i18n::tr("Headless Compute — Particle", "Headless 计算 —— 粒子");
        if (id == "gpu_burn")   return i18n::tr("Plasma x Kaleidoscope — GPU Burn", "等离子晶核 × 万花镜 —— GPU Burn");
        if (id == "gpu_stress") return i18n::tr("GraphicsBurn v1 / Component (Advanced)", "GraphicsBurn v1 / 分项（高级）");
        if (id == "nbody")      return i18n::tr("N-Body — Advanced Compute", "N-Body —— 高级计算");
        if (id == "synthpeak")  return i18n::tr("SynthPeak — Advanced Synthetic", "SynthPeak —— 高级合成测试");
        if (id == "stress")     return i18n::tr("Legacy Stress v1 — Fragment ALU/SFU", "旧版压力测试 v1 —— 片元 ALU/SFU");
        if (id == "render3d")   return i18n::tr("Legacy 3D Prototype — Billboards", "旧版 3D 原型 —— Billboard");
        if (id == "volumetric") return i18n::tr("Volumetric — Experimental Raymarch", "体积渲染 —— 实验性 Raymarch");
        if (id == "cinematic_liquid") return i18n::tr("Fluid — Interactive Pool", "流体 —— 互动水池");
        if (id == "cinematic_liquid_v1") return i18n::tr("Legacy Cinematic Liquid v1 — Dam Break", "旧版电影化液体 v1 —— 溃坝");
        if (id == "fluid")      return i18n::tr("Other / Legacy 2D Fluid", "其他 / 旧版 2D 流体");
        if (id == "cpu_single_core") return i18n::tr("CPU — Per-core", "CPU —— 逐核");
        if (id == "cpu_multi_core")  return i18n::tr("CPU — All-core", "CPU —— 多核");
        return id;
    }

    bool isCpuHistoryResult(gpu_bench::BenchmarkResult const& r)
    {
        return r.graphicsApi == "CPU" || r.workload.rfind("cpu_", 0) == 0;
    }

    bool workloadUsesParticles(std::string const& key)
    {
        return key == "stream" || key == "cinematic_liquid" ||
               key == "cinematic_liquid_v1" || key == "render3d" || key == "fluid";
    }

    bool workloadUsesBurnSteps(std::string const& key)
    {
        return key == "gpu_burn";
    }

    bool isPrimaryGpuWorkload(std::string const& key)
    {
        return key == "stream" || key == "gpu_burn" || key == "cinematic_liquid";
    }

    bool isLegacyGpuWorkload(std::string const& key)
    {
        return !isPrimaryGpuWorkload(key) && !key.empty() && key.rfind("cpu_", 0) != 0;
    }

    std::string cpuFilterKey(gpu_bench::BenchmarkResult const& r)
    {
        return normalizeCpuName(r.cpuName.empty() ? r.deviceName : r.cpuName);
    }

    // Parse "steps=N" from GPU Burn workloadConfig.
    std::string burnStepsKey(gpu_bench::BenchmarkResult const& r)
    {
        if (r.workload != "gpu_burn") return {};
        constexpr char key[] = "steps=";
        auto pos = r.workloadConfig.find(key);
        if (pos == std::string::npos) return {};
        pos += sizeof(key) - 1;
        auto end = r.workloadConfig.find(';', pos);
        auto steps = r.workloadConfig.substr(pos, end - pos);
        while (!steps.empty() && (steps.front() == ' ' || steps.front() == '\t'))
            steps.erase(steps.begin());
        while (!steps.empty() && (steps.back() == ' ' || steps.back() == '\t'))
            steps.pop_back();
        return steps;
    }

    std::string burnStepsLabel(std::string const& steps)
    {
        if (steps.empty()) return i18n::tr("(unknown)", "（未知）");
        return steps + i18n::tr(" steps", " 步");
    }

    // Older stream rows often stored vramMB=0; reuse a known value for the same GPU.
    std::uint32_t resolvedVramMB(gpu_bench::BenchmarkResult const& r,
                                 std::vector<gpu_bench::BenchmarkResult> const& all)
    {
        if (r.vramMB > 0) return r.vramMB;
        if (r.deviceName.empty()) return 0;
        const auto want = normalizeGpuName(r.deviceName);
        std::uint32_t best = 0;
        for (auto const& o : all)
        {
            if (o.vramMB == 0) continue;
            if (o.deviceName == r.deviceName || normalizeGpuName(o.deviceName) == want)
                best = (std::max)(best, o.vramMB);
        }
        return best;
    }

    std::string formatVramMB(std::uint32_t mb)
    {
        if (mb == 0) return "-";
        if (mb < 1024) return std::to_string(mb) + "MB";

        // DXGI DedicatedVideoMemory is often a few hundred MB below the
        // marketed size (e.g. 32187 MiB for a 32 GB RTX 5090). Snap to a
        // common advertised GiB label when we are within 5% below it.
        static constexpr std::uint32_t kCommonGiB[] = {
            4, 6, 8, 10, 12, 16, 20, 24, 32, 40, 48, 64, 80, 96, 128
        };
        for (std::uint32_t gib : kCommonGiB)
        {
            const std::uint32_t marketedMiB = gib * 1024u;
            if (mb > marketedMiB) continue;
            const std::uint32_t slack = marketedMiB - mb;
            if (slack <= marketedMiB / 20u)  // ≤ 5%
                return std::to_string(gib) + "GB";
        }

        const auto gibRounded = static_cast<std::uint32_t>(
            (mb + 512u) / 1024u);  // nearest GiB
        return std::to_string(gibRounded > 0 ? gibRounded : 1) + "GB";
    }

    // True if any checked workload checkbox matches predicate; if none checked,
    // fall back to scanning category results via scanFallback.
    template <typename Pred, typename Fallback>
    bool historyParamFilterNeeded(StackPanel const& workloadPanel, Pred pred, Fallback scanFallback)
    {
        bool anyChecked = false;
        bool checkedNeeds = false;
        for (auto const& c : workloadPanel.Children())
        {
            auto cb = c.try_as<CheckBox>();
            if (!cb || !cb.Tag()) continue;
            const auto key = to_string(unbox_value_or<hstring>(cb.Tag(), L""));
            if (!(cb.IsChecked() && cb.IsChecked().Value())) continue;
            anyChecked = true;
            if (pred(key)) checkedNeeds = true;
        }
        if (anyChecked) return checkedNeeds;
        return scanFallback();
    }

    std::string workloadRunLabel(gpu_bench::BenchmarkResult const& result)
    {
        if (result.headless && result.workload == "stream")
            return workloadLabel("headless_compute");
        auto label = workloadLabel(result.workload);
        if (result.workload == "cinematic_liquid" &&
            result.workloadVersion == "cinematic_liquid_v1")
            return workloadLabel("cinematic_liquid_v1");
        if (result.workload == "fluid")
            return i18n::tr("Legacy 2D Fluid — unverified", "旧版 2D 流体 —— 未验证");
        if (result.headless)
            label += " [headless]";
        if (result.workload != "gpu_burn") return label;
        constexpr char key[] = "steps=";
        auto pos = result.workloadConfig.find(key);
        if (pos == std::string::npos) return label;
        pos += sizeof(key) - 1;
        auto end = result.workloadConfig.find(';', pos);
        auto steps = result.workloadConfig.substr(pos, end - pos);
        if (!steps.empty())
            label += " [" + steps + i18n::tr(" steps]", " 步]");
        return label;
    }

    std::string historyWorkloadKey(gpu_bench::BenchmarkResult const& result)
    {
        // Headless Particle stays under stream for the workload filter; the
        // separate "Show headless" toggle controls visibility.
        // Persisted liquid v1/v2 rows share the historical public workload id
        // and are separated by workloadVersion. Give v1 its own UI filter
        // without rewriting old result files.
        if (result.workload == "cinematic_liquid" &&
            result.workloadVersion == "cinematic_liquid_v1")
            return "cinematic_liquid_v1";
        return result.workload;
    }

    std::string apiLabel(std::string const& id)
    {
        if (id == "DX12") return "DirectX 12";
        if (id == "DX11") return "DirectX 11";
        return id.empty() ? "(unknown)" : id;
    }

    std::set<std::string> checkedTags(StackPanel const& panel)
    {
        std::set<std::string> out;
        for (auto const& c : panel.Children())
            if (auto cb = c.try_as<CheckBox>(); cb && cb.Tag() && cb.IsChecked() && cb.IsChecked().Value())
                out.insert(to_string(unbox_value_or<hstring>(cb.Tag(), L"")));
        return out;
    }

    void setPanelChecks(StackPanel const& panel, bool value)
    {
        for (auto const& c : panel.Children())
            if (auto cb = c.try_as<CheckBox>(); cb && cb.Tag())
                cb.IsChecked(value);
    }

    std::string mostFrequentKey(std::map<std::string, int> const& counts)
    {
        std::string best;
        int bestCount = -1;
        for (auto const& [key, count] : counts)
            if (count > bestCount)
            {
                best = key;
                bestCount = count;
            }
        return best;
    }

    void appendFilterCheckBox(StackPanel const& panel,
                              std::string const& label,
                              std::string const& tag,
                              bool checked,
                              Thickness const& margin = Thickness{ 0, 0, 0, 0 })
    {
        CheckBox cb;
        cb.Content(box_value(u8(label)));
        cb.Tag(box_value(u8(tag)));
        cb.IsChecked(checked);
        cb.Margin(margin);
        panel.Children().Append(cb);
    }

    struct RunApiDefinition
    {
        const char* token;
        const char* label;
        size_t supportIndex;
    };

    constexpr RunApiDefinition kRunApis[] = {
        { "vulkan", "Vulkan",     0 },
        { "dx12",   "DirectX 12", 1 },
        { "dx11",   "DirectX 11", 2 },
        { "opengl", "OpenGL",     3 },
    };

    // Compact NumberBox template part: the popup holding the spin buttons.
    Microsoft::UI::Xaml::Controls::Primitives::Popup findSpinPopup(
        DependencyObject const& root)
    {
        if (!root) return nullptr;
        if (auto popup = root.try_as<Microsoft::UI::Xaml::Controls::Primitives::Popup>();
            popup && popup.Name() == L"UpDownPopup")
            return popup;
        const int count = VisualTreeHelper::GetChildrenCount(root);
        for (int i = 0; i < count; ++i)
            if (auto p = findSpinPopup(VisualTreeHelper::GetChild(root, i)))
                return p;
        return nullptr;
    }

    void attachSpinPopupTransition(NumberBox const& box)
    {
        try
        {
            if (auto popup = findSpinPopup(box);
                popup && (!popup.ChildTransitions() ||
                          popup.ChildTransitions().Size() == 0))
            {
                TransitionCollection transitions;
                transitions.Append(PopupThemeTransition{});
                popup.ChildTransitions(transitions);
            }
        }
        catch (...) {}
    }

    // Compact NumberBox template part: the editable text field.
    TextBox findNumberBoxInput(DependencyObject const& root)
    {
        if (!root) return nullptr;
        if (auto tb = root.try_as<TextBox>(); tb && tb.Name() == L"InputBox")
            return tb;
        const int count = VisualTreeHelper::GetChildrenCount(root);
        for (int i = 0; i < count; ++i)
            if (auto tb = findNumberBoxInput(VisualTreeHelper::GetChild(root, i)))
                return tb;
        return nullptr;
    }

    // Stepping select-alls the text (native NumberBox behaviour) — from the
    // popup arrows that reads as an accidental selection. Collapse it.
    void clearSpinSelection(NumberBox const& box)
    {
        try
        {
            if (auto input = findNumberBoxInput(box))
                input.Select(static_cast<int32_t>(input.Text().size()), 0);
        }
        catch (...) {}
    }

    // Typing the field empty produces NaN; snap back to the previous value so
    // the box never sits blank.
    bool restoreEmptyNumberBox(NumberBox const& box,
                               NumberBoxValueChangedEventArgs const& args)
    {
        if (!std::isnan(args.NewValue())) return false;

        double fallback = args.OldValue();
        if (!std::isfinite(fallback))
            fallback = std::isfinite(box.Minimum()) ? box.Minimum() : 0.0;
        if (std::isfinite(box.Minimum()))
            fallback = (std::max)(box.Minimum(), fallback);
        if (std::isfinite(box.Maximum()))
            fallback = (std::min)(box.Maximum(), fallback);
        box.Value(fallback);
        return true;
    }
}

MainWindow::MainWindow()
{
    InitializeComponent();
    m_dispatcher = Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread();
    m_enginePath = findEngineExe();
    m_cpuName = detectCpuName();

    if (auto native = this->try_as<::IWindowNative>())
        native->get_WindowHandle(&m_hwnd);

    auto window = this->try_as<Microsoft::UI::Xaml::Window>();
    window.ExtendsContentIntoTitleBar(true);
    window.SetTitleBar(AppTitleBar());

    if (m_hwnd)
    {
        auto windowId = Microsoft::UI::GetWindowIdFromWindow(m_hwnd);
        if (auto appWindow = Microsoft::UI::Windowing::AppWindow::GetFromWindowId(windowId))
        {
            UINT dpi = GetDpiForWindow(m_hwnd);
            double scale = dpi > 0 ? dpi / 96.0 : 1.0;
            const int32_t desiredWidth = static_cast<int32_t>(1320 * scale);
            const int32_t desiredHeight = static_cast<int32_t>(820 * scale);
            if (auto display = Microsoft::UI::Windowing::DisplayArea::GetFromWindowId(
                    windowId, Microsoft::UI::Windowing::DisplayAreaFallback::Primary))
            {
                const auto work = display.WorkArea();
                const int32_t width = (std::min)(desiredWidth, work.Width);
                const int32_t height = (std::min)(desiredHeight, work.Height);
                appWindow.MoveAndResize({
                    work.X + (work.Width - width) / 2,
                    work.Y + (work.Height - height) / 2,
                    width, height });
            }
            else
            {
                appWindow.Resize({ desiredWidth, desiredHeight });
            }
            if (Microsoft::UI::Windowing::AppWindowTitleBar::IsCustomizationSupported())
                appWindow.TitleBar().PreferredHeightOption(
                    Microsoft::UI::Windowing::TitleBarHeightOption::Tall);
        }
    }

    updateResizeBackdropColor();
    RootShell().ActualThemeChanged([this](auto&&, auto&&) {
        updateResizeBackdropColor();
        updateCaptionButtonColors();
    });
    m_gpuCancelEvent = makeManualResetEvent();
    Closed([this](auto&&, auto&&) {
        m_closing.store(true);
        cancelGpuBenchmark();
        cancelCpuBenchmark();
        // Drop our ref only; in-flight workers keep their shared_ptr copy alive
        // until WaitForMultipleObjects returns (avoids cancel-handle UAF).
        m_gpuCancelEvent.reset();
    });
    updateCaptionButtonColors();

    i18n::initLang(nullptr);
    applyLanguage();
    configureCpuNumberBoxes();

    // History filters apply once, when each dropdown closes (not on every toggle).
    auto applyOnClose = [this](auto&&, auto&&) { if (m_uiReady) applyHistoryView(); };
    ApiFilterFlyout().Closed(applyOnClose);
    WorkloadFilterFlyout().Closed(applyOnClose);
    ParticleFilterFlyout().Closed(applyOnClose);
    StepsFilterFlyout().Closed(applyOnClose);
    GpuFilterFlyout().Closed(applyOnClose);

    ApiPickerFlyout().Closed([this](auto&&, auto&&) {
        if (m_uiReady) updateApiPickerSummary();
    });
    // Headless disables RenderDoc capture (the engine never triggers captures
    // without a window); grey the controls to match.
    auto syncOnHeadlessToggle = [this](auto&&, auto&&) { syncCaptureControls(); };
    HeadlessBox().Checked(syncOnHeadlessToggle);
    HeadlessBox().Unchecked(syncOnHeadlessToggle);

    // Compact NumberBox popups follow focus, so Enter/Escape confirm by
    // handing focus back to the page (which closes the popup natively).
    // AddHandler with handledEventsToo: the inner TextBox marks Enter handled.
    auto numberBoxKeyDismiss = [this](IInspectable const& sender,
                                      Input::KeyRoutedEventArgs const& e)
    {
        if (e.Key() != Windows::System::VirtualKey::Enter &&
            e.Key() != Windows::System::VirtualKey::Escape)
            return;
        auto box = sender.try_as<NumberBox>();
        if (!box) return;
        DependencyObject runPage = RunPage();
        bool onRunPage = false;
        for (DependencyObject walk = box; walk; walk = VisualTreeHelper::GetParent(walk))
            if (walk == runPage) { onRunPage = true; break; }
        (onRunPage ? RunPage() : CpuPage()).Focus(FocusState::Programmatic);
    };
    for (auto const& numberBox :
         { DurationValueBox(), CaptureValueBox(), CpuTimeBox(), CpuWarmupBox() })
    {
        numberBox.AddHandler(UIElement::KeyDownEvent(),
                             winrt::box_value(Input::KeyEventHandler(numberBoxKeyDismiss)),
                             true /*handledEventsToo*/);
        // The template's UpDownPopup ships without transitions — attach the
        // standard popup animation. The popup may not be realised yet when
        // Loaded fires, so retry one dispatcher pass later.
        numberBox.Loaded([this](IInspectable const& sender, RoutedEventArgs const&)
        {
            auto box = sender.try_as<NumberBox>();
            if (!box) return;
            attachSpinPopupTransition(box);
            if (m_dispatcher)
                m_dispatcher.TryEnqueue(
                    Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
                    [box]() { attachSpinPopupTransition(box); });
        });
    }

    // Page-level pointer handlers via AddHandler: many controls mark
    // PointerPressed handled, which silently ate the XAML-wired version and
    // made outside-click dismissal intermittent.
    RunPage().AddHandler(UIElement::PointerPressedEvent(),
        winrt::box_value(Input::PointerEventHandler{ this, &MainWindow::OnGpuPagePointerPressed }),
        true /*handledEventsToo*/);
    CpuPage().AddHandler(UIElement::PointerPressedEvent(),
        winrt::box_value(Input::PointerEventHandler{ this, &MainWindow::OnCpuPagePointerPressed }),
        true /*handledEventsToo*/);
    // Whole-window listener so Compact spin popups dismiss no matter where
    // the click lands (page margins, nav pane, title bar, other cards).
    RootShell().AddHandler(UIElement::PointerPressedEvent(),
        winrt::box_value(Input::PointerEventHandler(
            [this](IInspectable const&, Input::PointerRoutedEventArgs const& e)
            {
                if (auto src = e.OriginalSource().try_as<DependencyObject>())
                    dismissNumberBoxPopupsOnOutsideClick(src);
            })),
        true /*handledEventsToo*/);
    SelectAllRunApis().Click([this](auto&&, auto&&) {
        setPanelChecks(SupportedApisPanel(), true);
        setPanelChecks(UnsupportedApisPanel(), true);
        updateApiPickerSummary();
    });
    ClearAllRunApis().Click([this](auto&&, auto&&) {
        setPanelChecks(SupportedApisPanel(), false);
        setPanelChecks(UnsupportedApisPanel(), false);
        updateApiPickerSummary();
    });

    SelectAllApis().Click([this](auto&&, auto&&) { setPanelChecks(ApiFilterPanel(), true); });
    ClearAllApis().Click([this](auto&&, auto&&) { setPanelChecks(ApiFilterPanel(), false); });
    SelectAllWorkloads().Click([this](auto&&, auto&&) { setPanelChecks(WorkloadFilterPanel(), true); });
    ClearAllWorkloads().Click([this](auto&&, auto&&) { setPanelChecks(WorkloadFilterPanel(), false); });
    SelectAllParticles().Click([this](auto&&, auto&&) { setPanelChecks(ParticleFilterPanel(), true); });
    ClearAllParticles().Click([this](auto&&, auto&&) { setPanelChecks(ParticleFilterPanel(), false); });
    SelectAllSteps().Click([this](auto&&, auto&&) { setPanelChecks(StepsFilterPanel(), true); });
    ClearAllSteps().Click([this](auto&&, auto&&) { setPanelChecks(StepsFilterPanel(), false); });
    SelectAllGpus().Click([this](auto&&, auto&&) { setPanelChecks(GpuFilterPanel(), true); });
    ClearAllGpus().Click([this](auto&&, auto&&) { setPanelChecks(GpuFilterPanel(), false); });

    // App icon: caption / taskbar (WM_SETICON) + title-bar logo + About card.
    if (m_hwnd)
        if (HICON hIcon = reinterpret_cast<HICON>(LoadImageW(
                GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101),
                IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED)))
        {
            SendMessageW(m_hwnd, WM_SETICON, ICON_BIG,   reinterpret_cast<LPARAM>(hIcon));
            SendMessageW(m_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
        }
    if (auto bmp = loadAppIconBitmap(40))  TitleBarIcon().Source(bmp);
    if (auto bmp = loadAppIconBitmap(128)) AboutIcon().Source(bmp);
    refreshAboutVersion();

    m_uiReady = true;
    applyWorkloadVisibility();
    syncCaptureControls();
    // App activates the window after this constructor returns. Touching ComboBox /
    // ListView before a live XamlRoot exists surfaces as UnhandledException
    // "xamlRoot" (seen in gui-crash.log during Detecting). Retry until rooted.
    auto strong = get_strong();
    auto tryStartup = std::make_shared<std::function<void()>>();
    *tryStartup = [this, strong, tryStartup]()
    {
        if (!uiAlive()) return;
        try
        {
            auto root = Content();
            if (!root || !root.XamlRoot())
            {
                m_dispatcher.TryEnqueue(
                    Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
                    [tryStartup]() { (*tryStartup)(); });
                return;
            }
            populateGpus();
            refreshHistory();
        }
        catch (winrt::hresult_error const& e)
        {
            appendGuiCrashLog("DeferredInit", winrt::to_string(e.message()).c_str());
            m_gpuEnumerationComplete = true;
            syncActionButtonsEnabled();
        }
        catch (...)
        {
            appendGuiCrashLog("DeferredInit", "unknown");
            m_gpuEnumerationComplete = true;
            syncActionButtonsEnabled();
        }
    };
    m_dispatcher.TryEnqueue(
        Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
        [tryStartup]() { (*tryStartup)(); });
}

// Only the three primary tests are listed by default; everything else stays
// hidden behind the explicit legacy/advanced toggle so the daily dropdown is
// Particle / Plasma x Kaleidoscope / Fluid.
void MainWindow::applyWorkloadVisibility()
{
    const bool showLegacy = ShowLegacyBox().IsChecked().Value();
    const auto visibility = showLegacy ? Visibility::Visible
                                       : Visibility::Collapsed;
    ComboBoxItem legacyItems[] = {
        WorkloadGpuStress(), WorkloadNBody(), WorkloadSynthPeak(),
        WorkloadStress(), WorkloadRender3D(), WorkloadVolumetric(),
        WorkloadFluid(), WorkloadCinematicLiquidV1()};
    for (auto const& item : legacyItems) item.Visibility(visibility);

    if (!showLegacy)
    {
        auto current = WorkloadBox().SelectedItem().try_as<ComboBoxItem>();
        if (current && current.Visibility() == Visibility::Collapsed)
            WorkloadBox().SelectedIndex(0);
    }
}

void MainWindow::OnShowLegacyChecked(IInspectable const&, RoutedEventArgs const&)
{
    if (m_uiReady) applyWorkloadVisibility();
}

void MainWindow::OnRenderDocChecked(IInspectable const&, RoutedEventArgs const&)
{
    if (!m_uiReady || m_suppressRenderDocUi) return;
    const bool on = RenderDocBox().IsChecked() && RenderDocBox().IsChecked().Value();
    if (!on)
    {
        m_suppressRenderDocUi = true;
        CaptureBox().IsChecked(false);
        m_suppressRenderDocUi = false;
    }
    syncCaptureControls();
}

void MainWindow::OnCaptureChecked(IInspectable const&, RoutedEventArgs const&)
{
    if (!m_uiReady || m_suppressRenderDocUi) return;
    const bool on = CaptureBox().IsChecked() && CaptureBox().IsChecked().Value();
    if (on && !(RenderDocBox().IsChecked() && RenderDocBox().IsChecked().Value()))
    {
        m_suppressRenderDocUi = true;
        RenderDocBox().IsChecked(true);
        m_suppressRenderDocUi = false;
    }
    syncCaptureControls();
}

// The select-all NumberBox performs after stepping may land before or after
// this handler runs; collapse now and once more on the next dispatcher pass.
void MainWindow::collapseSpinSelectionSoon(NumberBox const& box)
{
    clearSpinSelection(box);
    if (m_dispatcher)
        m_dispatcher.TryEnqueue(
            Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
            [box]() { clearSpinSelection(box); });
}

void MainWindow::OnCaptureValueChanged(NumberBox const& sender,
                                       NumberBoxValueChangedEventArgs const& args)
{
    if (restoreEmptyNumberBox(sender, args)) return;
    collapseSpinSelectionSoon(sender);
    if (!m_uiReady || m_suppressRenderDocUi) return;
    const double cur = sender.Value();
    const double minVal = CaptureValueBox().Minimum();
    const double maxVal = CaptureValueBox().Maximum();
    double clamped = (std::min)(maxVal, (std::max)(minVal, cur));
    clamped = std::floor(clamped + 1e-9); // integer steps for every unit
    if (clamped == cur) return;
    m_suppressRenderDocUi = true;
    CaptureValueBox().Value(clamped);
    m_suppressRenderDocUi = false;
}

void MainWindow::OnDurationValueChanged(NumberBox const& sender,
                                        NumberBoxValueChangedEventArgs const& args)
{
    if (restoreEmptyNumberBox(sender, args)) return;
    collapseSpinSelectionSoon(sender);
    if (!m_uiReady || m_suppressCombo) return;
    syncCaptureControls();
}

void MainWindow::OnApiPickerDropDownOpened(IInspectable const&, IInspectable const&)
{
    // MaxDropDownHeight=0 already suppresses the list, but closing the dropdown
    // synchronously inside DropDownOpened interrupts the open transition and can
    // leave the popup stuck on screen. Close it on the next dispatcher pass.
    closeApiPickerDropDown();
}

void MainWindow::closeApiPickerDropDown()
{
    if (!m_dispatcher) return;
    m_dispatcher.TryEnqueue(
        Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
        [this, strong = get_strong()]()
    {
        if (!uiAlive()) return;
        ApiPickerBox().IsDropDownOpen(false);
        // Belt and braces: the property write alone can leave the template
        // popup visible; force-close any open popup owned by the ComboBox.
        auto xamlRoot = ApiPickerBox().XamlRoot();
        if (!xamlRoot) return;
        DependencyObject owner = ApiPickerBox();
        for (auto const& popup : VisualTreeHelper::GetOpenPopupsForXamlRoot(xamlRoot))
        {
            for (auto walk = popup.as<DependencyObject>(); walk;
                 walk = VisualTreeHelper::GetParent(walk))
            {
                if (walk == owner) { popup.IsOpen(false); break; }
            }
        }
    });
}

void MainWindow::setTaskbarProgress(bool active, double fraction, bool indeterminate)
{
    if (!m_hwnd) return;
    if (!m_taskbar && !m_taskbarInitTried)
    {
        m_taskbarInitTried = true;
        winrt::com_ptr<ITaskbarList3> tb;
        if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, nullptr,
                                       CLSCTX_INPROC_SERVER, IID_PPV_ARGS(tb.put()))) &&
            SUCCEEDED(tb->HrInit()))
            m_taskbar = tb;
    }
    if (!m_taskbar) return;
    if (!active)
    {
        m_taskbar->SetProgressState(m_hwnd, TBPF_NOPROGRESS);
        return;
    }
    if (indeterminate)
    {
        m_taskbar->SetProgressState(m_hwnd, TBPF_INDETERMINATE);
        return;
    }
    m_taskbar->SetProgressState(m_hwnd, TBPF_NORMAL);
    const auto v = static_cast<ULONGLONG>(
        (std::min)(1.0, (std::max)(0.0, fraction)) * 1000.0);
    m_taskbar->SetProgressValue(m_hwnd, v, 1000ull);
}

void MainWindow::updateGpuProgressTick()
{
    if (m_gpuProgressJobs == 0) return;
    if (m_gpuProgressIndeterminate)
    {
        setTaskbarProgress(true, 0.0, true);
        return;
    }
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - m_gpuProgressJobStart).count();
    // Cap the within-job estimate below 100% — the worker finishes when it
    // finishes; only real completion may show 100%.
    const double within =
        (std::min)(elapsed / (std::max)(m_gpuProgressJobExpectedSec, 1.0), 0.98);
    const double fraction = (std::min)(0.99,
        (static_cast<double>(m_gpuProgressJobIndex) + within) /
            static_cast<double>(m_gpuProgressJobs));
    GpuProgressBar().Value(fraction * 100.0);
    std::wostringstream pct;
    pct << static_cast<int>(fraction * 100.0) << L'%';
    GpuProgressText().Text(hstring(pct.str()));
    setTaskbarProgress(true, fraction);
}

void MainWindow::stopGpuProgress(hstring const& stage, bool complete)
{
    if (m_gpuProgressTimer) m_gpuProgressTimer.Stop();
    m_gpuProgressJobs = 0;
    GpuProgressBar().IsIndeterminate(false);
    if (complete)
    {
        GpuProgressBar().Value(100.0);
        GpuProgressText().Text(L"100%");
    }
    GpuStageText().Text(stage);
    setTaskbarProgress(false, 0.0);
}

void MainWindow::renderResultScore()
{
    // Score text uses "# <GPU name>" marker lines to open per-adapter groups;
    // render those as small secondary eyebrows above the bold score lines.
    using winrt::Microsoft::UI::Xaml::Documents::LineBreak;
    using winrt::Microsoft::UI::Xaml::Documents::Run;
    auto inlines = ResultText().Inlines();
    inlines.Clear();
    if (m_lastScoreEn.empty()) return;

    Brush secondary{ nullptr };
    if (auto res = Application::Current().Resources().TryLookup(
            box_value(L"TextFillColorSecondaryBrush")))
        secondary = res.try_as<Brush>();

    const std::string text = localizeScoreLine(m_lastScoreEn);
    bool firstLine = true;
    size_t start = 0;
    while (start <= text.size())
    {
        const auto nl = text.find('\n', start);
        const std::string line = text.substr(
            start, nl == std::string::npos ? std::string::npos : nl - start);
        const bool isHeader = line.rfind("# ", 0) == 0;
        if (!firstLine)
        {
            inlines.Append(LineBreak{});
            if (isHeader) inlines.Append(LineBreak{}); // blank line between groups
        }
        Run run;
        if (isHeader)
        {
            run.Text(u8(line.substr(2)));
            run.FontSize(12.0);
            run.FontWeight(winrt::Windows::UI::Text::FontWeights::Normal());
            if (secondary) run.Foreground(secondary);
        }
        else
        {
            run.Text(u8(line));
        }
        inlines.Append(run);
        firstLine = false;
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
}

void MainWindow::updateResultHint()
{
    // Host-memory runs report "RAM rate:" (see localizeScoreLine). The number is
    // latency-bound PCIe zero-copy access, not RAM bandwidth — explain the low
    // score so it does not look like a broken result. Match must exclude
    // "VRAM rate:", which contains "RAM rate:" as a substring.
    bool hostMem = false;
    for (auto pos = m_lastScoreEn.find("RAM rate:"); pos != std::string::npos;
         pos = m_lastScoreEn.find("RAM rate:", pos + 1))
        if (pos == 0 || m_lastScoreEn[pos - 1] != 'V') { hostMem = true; break; }
    const bool cacheHint = !m_lastScoreEn.empty() && m_lastScoreCacheHint;

    hstring text;
    if (hostMem)
        text = locText(
            "System memory mode: the GPU reads and writes system RAM directly over PCIe "
            "in small per-particle transactions. Each one pays the full PCIe round-trip "
            "latency and bypasses the GPU caches, so the rate is latency-bound — typically "
            "only a few percent of PCIe link bandwidth, far below both PCIe and RAM limits. "
            "A much lower score than VRAM mode is expected.",
            "系统内存模式：GPU 经 PCIe 以逐粒子的小块读写直接访问系统内存，"
            "每次访问都要承担完整的 PCIe 往返延迟，且无法被 GPU 缓存，"
            "因此速率受延迟而非带宽限制——通常只有 PCIe 链路带宽的百分之几，"
            "远低于 PCIe 和内存的带宽上限；成绩比显存模式低很多属正常现象。");
    if (cacheHint)
    {
        auto cacheText = locText(
            "Small working set (under 128 MB): it can sit entirely in the GPU's L2 cache, "
            "so this rate mixes cache bandwidth with per-dispatch overhead — it may exceed "
            "the theoretical VRAM limit yet stay well below true L2 bandwidth, and does not "
            "represent either. For a real VRAM measurement choose Heavy (4M) particles or "
            "above.",
            "工作集较小（不足 128 MB）：可能整个驻留在 GPU 的 L2 缓存中，"
            "此时速率是缓存带宽与每次调度开销的混合值——可能超过显存理论带宽、"
            "又远低于真实 L2 带宽，两者都不代表。"
            "要测真实显存带宽，请选择重载（4M）及以上的粒子档。");
        text = text.empty() ? cacheText : text + L"\n" + cacheText;
    }
    if (!text.empty()) ResultHint().Text(text);
    ResultHint().Visibility(text.empty() ? Visibility::Collapsed : Visibility::Visible);
}

void MainWindow::OnApiPickerTapped(IInspectable const&,
                                   Microsoft::UI::Xaml::Input::TappedRoutedEventArgs const&)
{
    if (!uiAlive() || !ApiPickerBox().IsEnabled()) return;
    closeApiPickerDropDown();
    Microsoft::UI::Xaml::Controls::Primitives::FlyoutBase::ShowAttachedFlyout(ApiPickerBox());
}

std::string MainWindow::selected(ComboBox const& box)
{
    auto item = box.SelectedItem().try_as<ComboBoxItem>();
    if (!item) return "";
    if (item.Tag()) return to_string(unbox_value_or<hstring>(item.Tag(), L""));
    return to_string(unbox_value_or<hstring>(item.Content(), L""));
}

// ---- shell (from sdr2hdr) --------------------------------------------------
void MainWindow::applyTheme(int index)
{
    auto theme = index == 1 ? ElementTheme::Light
               : index == 2 ? ElementTheme::Dark
                            : ElementTheme::Default;
    if (auto root = RootShell().try_as<FrameworkElement>())
        root.RequestedTheme(theme);
    updateResizeBackdropColor();
    updateCaptionButtonColors();
}

void MainWindow::updateCaptionButtonColors()
{
    if (!m_hwnd) return;
    auto windowId = Microsoft::UI::GetWindowIdFromWindow(m_hwnd);
    auto appWindow = Microsoft::UI::Windowing::AppWindow::GetFromWindowId(windowId);
    if (!appWindow) return;
    auto tb = appWindow.TitleBar();

    using winrt::Windows::UI::Color;
    const bool dark = RootShell().ActualTheme() != ElementTheme::Light;
    const Color fg        = dark ? Color{ 255, 255, 255, 255 } : Color{ 255, 0, 0, 0 };
    const Color inactive  = dark ? Color{ 255, 155, 155, 155 } : Color{ 255, 120, 120, 120 };
    const Color clear     = Color{ 0, 0, 0, 0 };
    const Color hoverBg   = dark ? Color{ 25, 255, 255, 255 } : Color{ 18, 0, 0, 0 };
    const Color pressedBg = dark ? Color{ 50, 255, 255, 255 } : Color{ 36, 0, 0, 0 };

    tb.ButtonForegroundColor(fg);
    tb.ButtonBackgroundColor(clear);
    tb.ButtonInactiveBackgroundColor(clear);
    tb.ButtonInactiveForegroundColor(inactive);
    tb.ButtonHoverForegroundColor(fg);
    tb.ButtonHoverBackgroundColor(hoverBg);
    tb.ButtonPressedForegroundColor(fg);
    tb.ButtonPressedBackgroundColor(pressedBg);
}

void MainWindow::updateResizeBackdropColor()
{
    if (!m_hwnd) return;
    auto actual = RootShell().ActualTheme();
    COLORREF color = (actual == ElementTheme::Dark) ? RGB(32, 32, 32) : RGB(243, 243, 243);
    HBRUSH brush = CreateSolidBrush(color);
    SetWindowSubclass(m_hwnd, WindowSubclassProc, 1, reinterpret_cast<DWORD_PTR>(brush));
    if (m_bgBrush) DeleteObject(m_bgBrush);
    m_bgBrush = brush;
    InvalidateRect(m_hwnd, nullptr, TRUE);
}

void MainWindow::animatePageIn(FrameworkElement const& page)
{
    TranslateTransform tt; tt.Y(28.0);
    page.RenderTransform(tt);
    auto dur = DurationHelper::FromTimeSpan(std::chrono::milliseconds(300));
    CubicEase ease; ease.EasingMode(EasingMode::EaseOut);
    DoubleAnimation slide; slide.From(28.0); slide.To(0.0); slide.Duration(dur); slide.EasingFunction(ease);
    Storyboard::SetTarget(slide, tt); Storyboard::SetTargetProperty(slide, L"Y");
    DoubleAnimation fade; fade.From(0.0); fade.To(1.0); fade.Duration(dur);
    Storyboard::SetTarget(fade, page); Storyboard::SetTargetProperty(fade, L"Opacity");
    Storyboard sb; sb.Children().Append(slide); sb.Children().Append(fade); sb.Begin();
}

void MainWindow::showPage(int index)
{
    RunPage().Visibility(index == 0 ? Visibility::Visible : Visibility::Collapsed);
    CpuPage().Visibility(index == 5 ? Visibility::Visible : Visibility::Collapsed);
    HistoryPage().Visibility(index == 1 ? Visibility::Visible : Visibility::Collapsed);
    ChartsPage().Visibility(index == 2 ? Visibility::Visible : Visibility::Collapsed);
    SettingsPage().Visibility(index == 3 ? Visibility::Visible : Visibility::Collapsed);
    AboutPage().Visibility(index == 4 ? Visibility::Visible : Visibility::Collapsed);

    FrameworkElement active{ nullptr };
    switch (index)
    {
        case 0:  active = RunPage();      break;
        case 1:  active = HistoryPage();  break;
        case 2:  active = ChartsPage();   break;
        case 3:  active = SettingsPage(); break;
        case 4:  active = AboutPage();    break;
        case 5:  active = CpuPage();      break;
        default: active = RunPage();      break;
    }
    if (active) animatePageIn(active);
    if (index == 1) refreshHistory();
}

void MainWindow::OnNavSelectionChanged(NavigationView const&, NavigationViewSelectionChangedEventArgs const& args)
{
    if (auto item = args.SelectedItem().try_as<NavigationViewItem>())
    {
        auto tag = unbox_value_or<hstring>(item.Tag(), L"0");
        showPage(_wtoi(tag.c_str()));
    }
}

void MainWindow::OnThemeChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady || m_suppressCombo) return;
    applyTheme(ThemeBox().SelectedIndex());
}

void MainWindow::OnLangChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady || m_suppressCombo) return;
    int idx = LangBox().SelectedIndex();
    if (idx == 1)      i18n::initLang("en");
    else if (idx == 2) i18n::initLang("zh");
    else               i18n::initLang(nullptr);
    applyLanguage();
}

// ---- localization ----------------------------------------------------------
void MainWindow::applyLanguage()
{
    NavCpu().Content(locContent("CPU", "CPU"));
    CpuTitle().Text(locText("CPU Benchmark", "CPU 测试"));
    CpuNameText().Text(u8(m_cpuName));
    CpuInfo().Message(locText(
        "Measures CPU compute throughput with dense math loops on logical processors (single-core and multi-core).",
        "测 CPU 算力：用密集运算循环压测逻辑核心，对比单核与多核吞吐。"));
    CpuModeBox().Header(locContent("Test mode", "测试模式"));
    CpuModePerCore().Content(locContent("Each logical processor", "逐个逻辑处理器"));
    CpuModeMulti().Content(locContent("All cores together", "全部核心并行"));
    CpuModeAll().Content(locContent("Per-core + all-core", "逐核 + 多核"));
    CpuDurationPresetBox().Header(locContent("Duration preset", "时长预设"));
    CpuDurationQuick().Content(locContent("Quick (1 s)", "快速（1 秒）"));
    CpuDurationFormal().Content(locContent("Formal (15 s)", "正式（15 秒）"));
    CpuTimeBox().Header(locContent("Seconds per test", "每项测试秒数"));
    CpuWarmupBox().Header(locContent("Warm-up seconds", "预热秒数"));
    CpuDurationHint().Text(locText(
        "Per-core duration is applied separately to every logical processor; the formal preset can take a long time.",
        "该时长会分别应用到每个逻辑处理器；正式逐核测试可能需要较长时间。"));
    CpuPerCoreTitle().Text(locText("Per-logical-processor results", "逐逻辑处理器成绩"));
    CpuSummaryTitle().Text(locText("Summary", "汇总"));
    GpuSummaryTitle().Text(locText("Summary", "汇总"));
    CpuAverageLabel().Text(locText("Per-core average", "逐核平均"));
    CpuMultiLabel().Text(locText("All-core result", "多核成绩"));
    CpuOutputExpander().Header(locContent("Raw CLI output (live tail)", "原始 CLI 输出（实时尾部）"));
    CpuCancelButton().Content(locContent("Cancel", "取消"));
    CpuRunButton().Content(locContent("Run CPU Benchmark", "开始 CPU 测试"));
    if (!m_cpuRunning.load())
        setCpuStatus(StatusLight::Ready, locText("Ready", "就绪"));

    GpuOutputExpander().Header(locContent("Raw CLI output", "原始 CLI 输出"));

    NavRun().Content(locContent("GPU", "GPU"));
    NavHistory().Content(locContent("History", "历史"));
    NavCharts().Content(locContent("Charts", "图表"));
    NavSettings().Content(locContent("Settings", "设置"));
    NavAbout().Content(locContent("About", "关于"));

    RunTitle().Text(locText("GPU Benchmark", "GPU 测试"));
    PresetBox().Header(locContent("Preset", "预设"));
    PresetQuick().Content(locContent("Quick run (best API / GPU, Medium)",
                                     "快速运行（最佳 API / GPU，中等）"));
    PresetCustom().Content(locContent("Custom run (choose API / GPU / workload)",
                                      "自定义运行（选择 API / GPU / 测试项目）"));
    PresetFullOne().Content(locContent("Full analysis — selected workload / one GPU (selected APIs + RenderDoc + charts)",
                                       "完整分析 —— 所选测试 / 单 GPU（所选 API + RenderDoc + 图表）"));
    PresetFullAll().Content(locContent("Full analysis — selected workload / all GPUs × selected APIs (+ RenderDoc + charts)",
                                       "完整分析 —— 所选测试 / 全部 GPU × 所选 API（+ RenderDoc + 图表）"));
    PresetFlights().Content(locContent("Flights test — one GPU (selected APIs, custom flights)",
                                       "Flights 测试 —— 单 GPU（所选 API，自定义 flights）"));
    PresetParticles().Content(locContent("Particle test — one GPU (selected APIs, custom particles)",
                                         "粒子测试 —— 单 GPU（所选 API，自定义粒子数）"));
    PresetHeadless().Content(locContent("Headless compute — one GPU (selected APIs, pure compute)",
                                        "Headless 计算 —— 单 GPU（所选 API，纯计算）"));
    GpuHeader().Text(locText("GPU / Renderer", "GPU / 渲染器"));
    ApiPickerHeader().Text(locText("Graphics API", "图形 API"));
    Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
        ApiPickerBox(), locText("Graphics API", "图形 API"));
    SelectAllRunApis().Content(locContent("Select all", "全选"));
    ClearAllRunApis().Content(locContent("Select none", "全不选"));
    SupportedApisLabel().Text(locText("Supported", "支持"));
    UnsupportedApisLabel().Text(locText("Not reported as supported", "未报告支持"));
    Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
        SupportedApisGroup(), locText("Supported APIs", "支持的 API"));
    Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
        UnsupportedApisGroup(), locText("Unsupported APIs", "不支持的 API"));
    UnsupportedApisHint().Text(locText(
        "Unsupported selections remain available and will be reported by the CLI.",
        "不支持的 API 仍可勾选；运行后由 CLI 返回明确错误。"));
    WorkloadHeader().Text(locText("Workload", "测试项目"));
    WorkloadStream().Content(locContent("Particle — Memory Throughput",
                                        "粒子 —— 内存吞吐"));
    WorkloadGpuBurn().Content(locContent("Plasma x Kaleidoscope — GPU Burn",
                                          "等离子晶核 × 万花镜 —— GPU Burn"));
    WorkloadCinematicLiquid().Content(locContent("Fluid — Interactive Pool",
                                                  "流体 —— 互动水池"));
    WorkloadCinematicLiquid().Tag(box_value(hstring(L"cinematic_liquid")));
    WorkloadCinematicLiquidV1().Content(locContent("Other / Legacy Cinematic Liquid v1 — Dam Break",
                                                    "其他 / 旧版电影化液体 v1 —— 溃坝"));
    WorkloadCinematicLiquidV1().Tag(box_value(hstring(L"cinematic_liquid_v1")));
    WorkloadStream().Tag(box_value(hstring(L"stream")));
    WorkloadGpuBurn().Tag(box_value(hstring(L"gpu_burn")));
    WorkloadGpuStress().Tag(box_value(hstring(L"gpu_stress")));
    WorkloadNBody().Tag(box_value(hstring(L"nbody")));
    WorkloadSynthPeak().Tag(box_value(hstring(L"synthpeak")));
    WorkloadStress().Tag(box_value(hstring(L"stress")));
    WorkloadRender3D().Tag(box_value(hstring(L"render3d")));
    WorkloadVolumetric().Tag(box_value(hstring(L"volumetric")));
    WorkloadFluid().Tag(box_value(hstring(L"fluid")));
    WorkloadGpuStress().Content(locContent("GraphicsBurn v1 / Component (Advanced)",
                                           "GraphicsBurn v1 / 分项（高级）"));
    WorkloadNBody().Content(locContent("N-Body — Advanced Compute",
                                       "N-Body —— 高级计算"));
    WorkloadSynthPeak().Content(locContent("SynthPeak — Advanced Synthetic",
                                           "SynthPeak —— 高级合成测试"));
    WorkloadStress().Content(locContent("Legacy Stress v1 — Fragment ALU/SFU",
                                        "旧版压力测试 v1 —— 片元 ALU/SFU"));
    WorkloadRender3D().Content(locContent("Legacy 3D Prototype — Billboards",
                                          "旧版 3D 原型 —— Billboard"));
    WorkloadVolumetric().Content(locContent("Volumetric — Experimental Raymarch",
                                             "体积渲染 —— 实验性 Raymarch"));
    WorkloadFluid().Content(locContent("Other / Legacy 2D Fluid",
                                        "其他 / 旧版 2D 流体"));
    ShowLegacyBox().Content(locContent("Show legacy & advanced tests",
                                       "显示旧版 / 高级测试"));
    PrecisionHeader().Text(locText("Precision", "精度"));
    PrecisionFp32().Content(locContent("fp32 — standard float", "fp32 —— 标准浮点"));
    PrecisionFp16().Content(locContent("fp16 — half precision", "fp16 —— 半精度"));
    PrecisionFp64().Content(locContent("fp64 — double precision", "fp64 —— 双精度"));
    PrecisionInt32().Content(locContent("int32 — integer ops", "int32 —— 整数运算"));
    DurationUnitBox().Header(locContent("Duration", "运行时长"));
    DurationSeconds().Content(locContent("Seconds", "秒"));
    DurationMinutes().Content(locContent("Minutes", "分钟"));
    DurationHours().Content(locContent("Hours", "小时"));
    DurationFrames().Content(locContent("Frames", "帧数"));
    DurationUnlimited().Content(locContent("Until Cancel", "直到取消"));
    DurationValueBox().Header(locContent("Value", "数值"));
    ParticlePresetBox().Header(locContent("Particles", "粒子数"));
    ParticlesLight().Content(locContent("Light — 65K", "轻量 —— 65K"));
    ParticlesMedium().Content(locContent("Medium — 1M", "中等 —— 1M"));
    ParticlesHeavy().Content(locContent("Heavy — 4M", "重载 —— 4M"));
    ParticlesExtreme().Content(locContent("Extreme — 16M", "极限 —— 16M"));
    ParticlesCustom().Content(locContent("Custom…", "自定义…"));
    CustomParticleBox().Header(locContent("Custom particles", "自定义粒子数"));
    CustomParticleBox().PlaceholderText(locText("multiple of 256", "256 的倍数"));
    AdvancedLabel().Text(locText("Advanced", "高级选项"));
    HeadlessBox().Content(box_value(hstring(L"Headless")));
    auto headlessTip = locContent(
        "Pure compute mode: no swapchain, no rendering, no present. "
        "Useful for measuring raw compute throughput.",
        "纯计算模式：无交换链、无渲染、无 present。用于测量原始计算吞吐。");
    ToolTipService::SetToolTip(HeadlessBox(), headlessTip);
    ToolTipService::SetToolTip(HeadlessInfo(), headlessTip);
    VsyncBox().Content(locContent("V-Sync", "垂直同步"));
    HostMemBox().Content(locContent("System memory", "系统内存"));
    auto hostMemTip = locContent(
        "Keep the particle buffer in system RAM instead of VRAM. This reproduces the "
        "access path a real game hits when VRAM overflows and resources are demoted "
        "to system memory — representative of that failure mode, though not identical "
        "to a full game pipeline (games also stream in large DMA blocks and keep hot "
        "resources resident). The rate is PCIe-latency-bound, not a RAM/PCIe bandwidth "
        "measurement. Vulkan and OpenGL only — DirectX passes fall back to VRAM with "
        "a warning.",
        "把粒子缓冲放在系统内存而非显存。这复现了真实游戏爆显存、资源被降级到系统内存后的"
        "访问路径——能反映这种故障形态，但不完全等同于游戏管线（游戏还会用 DMA 大块流送、"
        "并优先把热点资源留在显存）。速率受 PCIe 延迟限制，并非内存 / PCIe 带宽测试。"
        "仅 Vulkan 和 OpenGL 支持 —— DirectX 会回退到显存并输出警告。");
    ToolTipService::SetToolTip(HostMemBox(), hostMemTip);
    ToolTipService::SetToolTip(HostMemInfo(), hostMemTip);
    RenderDocBox().Content(locContent("RenderDoc", "RenderDoc"));
    CaptureBox().Content(locContent("Capture at", "捕获于"));
    auto renderDocTip = locContent(
        "RenderDoc is a free graphics debugger for inspecting GPU frames "
        "(draw calls, pipelines, resources, and shaders) from Vulkan, D3D, and OpenGL.",
        "RenderDoc 是免费的图形调试器，用于抓取并检查 GPU 帧内容"
        "（绘制调用、管线、资源与着色器等），支持 Vulkan / D3D / OpenGL。");
    ToolTipService::SetToolTip(RenderDocInfo(), renderDocTip);
    ToolTipService::SetToolTip(RenderDocBox(), renderDocTip);
    auto captureTip = locContent(
        "RenderDoc capture may reduce scores and add overhead.",
        "启用 RenderDoc 捕获可能会影响成绩并增加开销。");
    ToolTipService::SetToolTip(CaptureInfo(), captureTip);
    ToolTipService::SetToolTip(CaptureBox(), captureTip);
    syncCaptureControls();
    RunButton().Content(locContent("Run GPU Benchmark", "开始 GPU 测试"));
    GpuCancelButton().Content(locContent("Cancel", "取消"));
    if (m_activeTask.load() != ActiveTask::GpuBenchmark)
        setGpuStatus(StatusLight::Ready, locText("Ready", "就绪"));

    HistoryTitle().Text(locText("History", "历史"));
    RefreshButton().Content(locContent("Refresh", "刷新"));
    DeleteButton().Content(locContent("Delete selected", "删除所选"));
    OpenResultsFolderButton().Content(locContent("Open results folder", "打开成绩目录"));
    OpenCapturesFolderButton().Content(locContent("Open captures folder", "打开抓帧目录"));
    GpuOpenResultsButton().Content(locContent("Open results folder", "打开成绩目录"));
    GpuOpenCapturesButton().Content(locContent("Open RenderDoc captures", "打开 RenderDoc 抓帧目录"));
    SortBox().Header(locContent("Sort by", "排序"));
    SortTime().Content(locContent("Time (newest)", "时间（最新）"));
    SortScore().Content(locContent("Score (high→low)", "分数（高→低）"));
    SortApi().Content(locContent("Graphics API", "图形 API"));
    SortDevice().Content(locContent("GPU / Renderer", "GPU / 渲染器"));
    SortWorkload().Content(locContent("Workload", "测试项目"));
    WorkloadFilterBox().Header(locContent("Workload", "测试项目"));
    WorkloadFilterAll().Content(locContent("All workloads", "全部项目"));
    ParticleFilterBox().Header(locContent("Particles", "粒子数"));
    ParticleFilterAll().Content(locContent("All particle counts", "全部粒子数"));
    StepsFilterLabel().Text(locText("Steps", "步数"));
    SelectAllSteps().Content(locContent("All", "全选"));
    ClearAllSteps().Content(locContent("None", "清空"));
    TimeRangeBox().Header(locContent("Time range", "时间范围"));
    RangeAll().Content(locContent("All", "全部"));
    RangeToday().Content(locContent("Today", "今天"));
    Range7().Content(locContent("Last 7 days", "近 7 天"));
    Range30().Content(locContent("Last 30 days", "近 30 天"));
    RangeCustom().Content(locContent("Custom range…", "自定义范围…"));
    FromDate().Header(locContent("From", "起始"));
    ToDate().Header(locContent("To", "结束"));
    FromDate().PlaceholderText(locText("select a date", "选择日期"));
    ToDate().PlaceholderText(locText("select a date", "选择日期"));
    FromDate().DateFormat(i18n::currentLang() == i18n::Lang::Zh
        ? hstring(L"{year.full}-{month.integer(2)}-{day.integer(2)}")
        : hstring(L"{day.integer(2)}/{month.integer(2)}/{year.full}"));
    ToDate().DateFormat(i18n::currentLang() == i18n::Lang::Zh
        ? hstring(L"{year.full}-{month.integer(2)}-{day.integer(2)}")
        : hstring(L"{day.integer(2)}/{month.integer(2)}/{year.full}"));
    GpuFilterLabel().Text(m_historyCategory == HistoryCategory::Cpu
        ? locText("CPUs", "处理器")
        : locText("GPUs", "显卡"));
    SelectAllGpus().Content(locContent("All", "全选"));
    ClearAllGpus().Content(locContent("None", "清空"));
    HistoryGpuTab().Text(locText("GPU", "GPU"));
    HistoryCpuTab().Text(locText("CPU", "CPU"));
    HistoryLegacyBox().Content(locContent("Show legacy tests", "显示旧版测试"));
    HistoryHeadlessBox().Content(locContent("Show headless", "显示 headless"));
    ChartsTitle().Text(locText("Charts", "图表"));
    GenChartsButton().Content(locContent("Generate Charts", "生成图表"));

    SettingsTitle().Text(locText("Settings", "设置"));
    ThemeBox().Header(locContent("Theme", "主题"));
    ThemeSystem().Content(locContent("Use system setting", "跟随系统"));
    ThemeLight().Content(locContent("Light", "浅色"));
    ThemeDark().Content(locContent("Dark", "深色"));
    LangBox().Header(locContent("Language", "语言"));
    AboutTitle().Text(locText("About", "关于"));
    AboutDesc().Text(locText("Cross-API CPU & GPU Benchmark Suite — native C++/WinRT frontend.",
                             "跨 API CPU 与 GPU 测试套件 —— 原生 C++/WinRT 前端。"));
    refreshAboutVersion();

    {
        const wchar_t* sys = (i18n::detectOsLang() == i18n::Lang::Zh) ? L"中文" : L"English";
        std::wstring label = std::wstring(locText("Auto", "自动").c_str()) + L" (" + sys + L")";
        LangAuto().Content(winrt::box_value(hstring(label)));
    }

    m_suppressCombo = true;
    auto refreshCombo = [](ComboBox const& cb) { int s = cb.SelectedIndex(); cb.SelectedIndex(-1); cb.SelectedIndex(s); };
    refreshCombo(ThemeBox());
    refreshCombo(LangBox());
    refreshCombo(SortBox());
    refreshCombo(WorkloadFilterBox());
    refreshCombo(ParticleFilterBox());
    refreshCombo(TimeRangeBox());
    refreshCombo(PresetBox());
    refreshCombo(DurationUnitBox());
    refreshCombo(WorkloadBox());
    refreshCombo(PrecisionBox());
    refreshCombo(ParticlePresetBox());
    refreshCombo(CpuModeBox());
    refreshCombo(CpuDurationPresetBox());
    if (GpuBox().Items().Size() > 0)
    {
        if (auto autoItem = GpuBox().Items().GetAt(0).try_as<ComboBoxItem>())
            autoItem.Content(locContent("(auto)", "（自动）"));
        // Force closed caption refresh after Content change (same ComboBox cache).
        const int gpuSel = GpuBox().SelectedIndex();
        GpuBox().SelectedIndex(-1);
        GpuBox().SelectedIndex(gpuSel >= 0 ? gpuSel : 0);
    }
    m_suppressCombo = false;

    rebuildHistoryFilters();
    if (m_gpuEnumerationComplete) rebuildApiPicker(true);
    updateExtraLabel();
    updateDurationValueEnabled();
    if (!m_lastScoreEn.empty())
        renderResultScore();
    updateResultHint();
    // Mid-run language switch: refresh progress / status that were baked in the
    // previous language when the worker started.
    if (m_activeTask.load() == ActiveTask::GpuBenchmark)
        refreshActiveGpuStatusLanguage();
    // updateExtraLabel → updateApiPickerSummary already localises Detecting/All APIs;
    // re-apply disabled greying after the SelectedItem dance.
    {
        const int preset = PresetBox().SelectedIndex();
        applyComboEnabledLook(GpuBox(),
            m_gpuEnumerationComplete && preset != 0 && preset != 3);
        applyComboEnabledLook(ApiPickerBox(),
            m_gpuEnumerationComplete && preset != 0);
        if (m_gpuEnumerationComplete && preset != 0)
            ApiPickerHeader().ClearValue(TextBlock::ForegroundProperty());
        else
        {
            ApiPickerHeader().Opacity(0.55);
            ApiPickerHeader().Foreground(disabledTextBrush());
        }
    }
}

void MainWindow::configureCpuNumberBoxes()
{
    // NumberBox otherwise prints binary float noise like 0.200000003 for 0.2.
    using Windows::Globalization::NumberFormatting::DecimalFormatter;
    using Windows::Globalization::NumberFormatting::IncrementNumberRounder;
    using Windows::Globalization::NumberFormatting::RoundingAlgorithm;

    auto makeFormatter = [](double increment, int32_t fractionDigits)
    {
        IncrementNumberRounder rounder;
        rounder.Increment(increment);
        rounder.RoundingAlgorithm(RoundingAlgorithm::RoundHalfUp);
        DecimalFormatter formatter;
        formatter.IntegerDigits(1);
        formatter.FractionDigits(fractionDigits);
        formatter.IsGrouped(false);
        formatter.NumberRounder(rounder);
        return formatter;
    };

    // Integer seconds for test durations; warm-up keeps one decimal because
    // the published formal contract is exactly 15.0 s + 0.2 s warm-up.
    CpuTimeBox().NumberFormatter(makeFormatter(1.0, 0));
    CpuWarmupBox().NumberFormatter(makeFormatter(0.1, 1));
    DurationValueBox().NumberFormatter(makeFormatter(1.0, 0));
    // Refresh so the formatter applies to the XAML default Value="0.2".
    CpuWarmupBox().Value(0.2);
    CpuTimeBox().Value(CpuTimeBox().Value());
}

void MainWindow::setStatusLight(
    Microsoft::UI::Xaml::Shapes::Ellipse const& light, StatusLight kind)
{
    // Solid colors stay readable on both light and dark themes.
    // Avoid bare "Ellipse"/"Color" names — they collide with Win32 GDI.
    Windows::UI::Color color{};
    switch (kind)
    {
    case StatusLight::Running:
        color = Windows::UI::Color{ 255, 245, 188, 42 }; // amber
        break;
    case StatusLight::Error:
        color = Windows::UI::Color{ 255, 232, 17, 35 };  // red
        break;
    case StatusLight::Ready:
    default:
        color = Windows::UI::Color{ 255, 108, 203, 95 }; // green
        break;
    }
    light.Fill(SolidColorBrush(color));
}

void MainWindow::setGpuStatus(StatusLight kind, hstring const& text)
{
    setStatusLight(GpuStatusLight(), kind);
    Status().Text(text);
}

void MainWindow::setCpuStatus(StatusLight kind, hstring const& text)
{
    setStatusLight(CpuStatusLight(), kind);
    CpuStatusText().Text(text);
}

hstring MainWindow::gpuRunningStatusText() const
{
    if (m_gpuProgressJobs == 0) return locText("Running…", "运行中…");
    const auto progress = std::to_string(m_gpuProgressJobIndex + 1) + "/" +
                          std::to_string(m_gpuProgressJobs);
    const std::string api = m_gpuProgressApiLabel.empty() ? "?" : m_gpuProgressApiLabel;
    return i18n::currentLang() == i18n::Lang::Zh
        ? u8("运行中… (" + progress + "：" + api + ")")
        : u8("Running… (" + progress + ": " + api + ")");
}

void MainWindow::refreshActiveGpuStatusLanguage()
{
    if (m_activeTask.load() != ActiveTask::GpuBenchmark) return;
    if (m_gpuCancelRequested.load())
    {
        const auto text = locText("Cancelling...", "正在取消…");
        setGpuStatus(StatusLight::Running, text);
        GpuStageText().Text(text);
        return;
    }
    const auto text = gpuRunningStatusText();
    setGpuStatus(StatusLight::Running, text);
    GpuStageText().Text(text);
}

bool MainWindow::uiAlive() const
{
    return m_uiReady && !m_closing.load();
}

void MainWindow::syncActionButtonsEnabled()
{
    if (m_closing.load()) return;
    try
    {
        const bool busy = m_activeTask.load() != ActiveTask::None;
        // Block GPU Run while adapters/APIs are still being probed — concurrent
        // ProbeGpus + benchmark has crashed hosts via driver resets before.
        RunButton().IsEnabled(!busy && m_gpuEnumerationComplete);
        CpuRunButton().IsEnabled(!busy && !m_enginePath.empty());
        GenChartsButton().IsEnabled(!busy && !m_enginePath.empty());
    }
    catch (...) {}
}

// ---- CPU benchmark page ----------------------------------------------------
bool MainWindow::tryBeginTask(ActiveTask task)
{
    auto expected = ActiveTask::None;
    if (!m_activeTask.compare_exchange_strong(expected, task)) return false;
    syncActionButtonsEnabled();
    return true;
}

void MainWindow::endTask(ActiveTask task)
{
    auto expected = task;
    if (!m_activeTask.compare_exchange_strong(expected, ActiveTask::None)) return;
    syncActionButtonsEnabled();
}

void MainWindow::OnCpuDurationPresetChanged(IInspectable const&,
                                             SelectionChangedEventArgs const&)
{
    if (!m_uiReady || m_suppressCombo || m_cpuRunning.load()) return;
    const auto value = selected(CpuDurationPresetBox());
    if (value.empty()) return;
    try
    {
        const double seconds = std::stod(value);
        if (std::isfinite(seconds) && seconds > 0.0)
        {
            m_suppressCombo = true;
            CpuTimeBox().Value(seconds);
            // The published formal contract is exactly 15.0 s + 0.2 s
            // warm-up. Presets set the complete pair, not only duration.
            CpuWarmupBox().Value(0.2);
            m_suppressCombo = false;
        }
    }
    catch (...) {}
}

void MainWindow::OnCpuTimeChanged(NumberBox const& sender,
                                  NumberBoxValueChangedEventArgs const& args)
{
    if (restoreEmptyNumberBox(sender, args)) return;
    collapseSpinSelectionSoon(sender);
    if (!m_uiReady || m_suppressCombo || m_cpuRunning.load()) return;
    const double value = CpuTimeBox().Value();
    const double warmup = CpuWarmupBox().Value();
    int preset = -1;
    if (std::isfinite(value) && std::isfinite(warmup) &&
        std::abs(warmup - 0.2) < 0.0001)
    {
        if (std::abs(value - 1.0) < 0.0001) preset = 0;
        else if (std::abs(value - 15.0) < 0.0001) preset = 1;
    }

    m_suppressCombo = true;
    CpuDurationPresetBox().SelectedIndex(preset);
    m_suppressCombo = false;
}

void MainWindow::OnCpuRun(IInspectable const&, RoutedEventArgs const&)
{
    if (m_cpuRunning.load()) return;

    auto mode = selected(CpuModeBox());
    if (mode != "per-core" && mode != "multi" && mode != "all") mode = "all";
    const double seconds = CpuTimeBox().Value();
    const double warmup = CpuWarmupBox().Value();
    if (!std::isfinite(seconds) || seconds < 0.1 || seconds > 3600.0 ||
        !std::isfinite(warmup) || warmup < 0.0 || warmup > 60.0)
    {
        setCpuStatus(StatusLight::Error, locText(
            "Enter a valid duration and warm-up time.",
            "请输入有效的测试时长和预热时间。"));
        return;
    }

    const std::filesystem::path engine = pathFromUtf8(m_enginePath);
    std::error_code ec;
    if (!std::filesystem::is_regular_file(engine, ec) ||
        _wcsicmp(engine.filename().c_str(), L"gpu_benchmark.exe") != 0)
    {
        setCpuStatus(StatusLight::Error, locText(
            "gpu_benchmark.exe was not found beside the GUI or in the build directory.",
            "未在 GUI 同目录或构建目录中找到 gpu_benchmark.exe。"));
        return;
    }

    launchCpuBenchmark(std::move(mode), seconds, warmup);
}

void MainWindow::OnCpuCancel(IInspectable const&, RoutedEventArgs const&)
{
    cancelCpuBenchmark();
}

void MainWindow::OnGpuCancel(IInspectable const&, RoutedEventArgs const&)
{
    cancelGpuBenchmark();
}

void MainWindow::cancelGpuBenchmark()
{
    if (m_activeTask.load() != ActiveTask::GpuBenchmark) return;
    m_gpuCancelRequested.store(true);
    if (m_gpuCancelEvent)
        ::SetEvent(static_cast<HANDLE>(m_gpuCancelEvent.get()));
    try
    {
        GpuCancelButton().IsEnabled(false);
        setGpuStatus(StatusLight::Running, locText("Cancelling...", "正在取消…"));
    }
    catch (...) {}
}

void MainWindow::OnGpuCliHostSizeChanged(IInspectable const&, SizeChangedEventArgs const& args)
{
    // Keep long CLI lines inside the box; never widen the page.
    const double inner = args.NewSize().Width - 24.0; // Border Padding 12*2
    if (inner > 0.0)
        OutputBox().MaxWidth(inner);
}

void MainWindow::OnCpuCliHostSizeChanged(IInspectable const&, SizeChangedEventArgs const& args)
{
    const double inner = args.NewSize().Width - 24.0;
    if (inner > 0.0)
        CpuOutputBox().MaxWidth(inner);
}

namespace
{
    bool isUnder(DependencyObject const& node, DependencyObject const& ancestor)
    {
        for (auto walk = node; walk; walk = VisualTreeHelper::GetParent(walk))
            if (walk == ancestor) return true;
        return false;
    }

    void clearReadOnlyTextBoxFocus(TextBox const& box, FrameworkElement const& fallback)
    {
        box.Select(0, 0);
        auto focused = FocusManager::GetFocusedElement(box.XamlRoot()).try_as<TextBox>();
        if (focused && focused == box)
            fallback.Focus(FocusState::Programmatic);
    }

}

void MainWindow::OnGpuPagePointerPressed(IInspectable const&, PointerRoutedEventArgs const& args)
{
    auto src = args.OriginalSource().try_as<DependencyObject>();
    if (!src) return;
    if (!isUnder(src, GpuOutputExpander()))
        clearReadOnlyTextBoxFocus(OutputBox(), RunPage());
}

void MainWindow::OnCpuPagePointerPressed(IInspectable const&, PointerRoutedEventArgs const& args)
{
    auto src = args.OriginalSource().try_as<DependencyObject>();
    if (!src) return;
    if (!isUnder(src, CpuOutputExpander()))
        clearReadOnlyTextBoxFocus(CpuOutputBox(), CpuPage());
}

// The Compact spin popup follows focus: WinUI closes it when its NumberBox
// loses focus and reopens it while focus stays, so force-closing the popup
// never sticks. This runs on every window click (root handler,
// handledEventsToo) and defers the check one dispatcher pass — by then a
// focusable click target owns focus (nothing to do), while clicks on blank
// or non-focusable space leave focus in the box / popup, so we take it back.
void MainWindow::dismissNumberBoxPopupsOnOutsideClick(DependencyObject const& src)
{
    const std::array<NumberBox, 4> boxes{
        DurationValueBox(), CaptureValueBox(), CpuTimeBox(), CpuWarmupBox() };
    for (auto const& box : boxes)
        if (isUnder(src, box)) return; // the box's own business

    if (!m_dispatcher) return;
    m_dispatcher.TryEnqueue(
        Microsoft::UI::Dispatching::DispatcherQueuePriority::Low,
        [this, strong = get_strong()]()
    {
        if (!uiAlive()) return;
        auto focused = FocusManager::GetFocusedElement(RootShell().XamlRoot())
                           .try_as<DependencyObject>();
        if (!focused) return;

        bool dismiss = false;
        const std::array<NumberBox, 4> boxes{
            DurationValueBox(), CaptureValueBox(), CpuTimeBox(), CpuWarmupBox() };
        for (auto const& box : boxes)
            if (isUnder(focused, box)) { dismiss = true; break; }
        if (!dismiss)
        {
            // After spinning, focus sits on the popup's own repeat buttons,
            // which live in a separate popup tree (WinUI template names).
            if (auto fe = focused.try_as<FrameworkElement>())
            {
                const auto name = fe.Name();
                dismiss = name == L"PopupUpSpinButton" ||
                          name == L"PopupDownSpinButton";
            }
        }
        if (!dismiss) return;

        auto page = CpuPage().Visibility() == Visibility::Visible
            ? CpuPage() : RunPage();
        page.Focus(FocusState::Programmatic);
    });
}

void MainWindow::cancelCpuBenchmark()
{
    if (!m_cpuRunning.load()) return;
    m_cpuCancelRequested.store(true);
    CpuCancelButton().IsEnabled(false);
    setCpuStatus(StatusLight::Running, locText("Cancelling...", "正在取消…"));
    CpuCurrentCoreText().Text(locText("Stopping the CPU worker", "正在停止 CPU 测试进程"));

    std::lock_guard<std::mutex> lock(m_cpuProcessMutex);
    if (m_cpuProcess) ::TerminateProcess(m_cpuProcess, ERROR_CANCELLED);
}

void MainWindow::launchCpuBenchmark(std::string mode, double seconds,
                                    double warmupSeconds)
{
    if (!tryBeginTask(ActiveTask::CpuBenchmark))
    {
        setCpuStatus(StatusLight::Error, locText(
            "Another benchmark or report task is already running.",
            "另一个测试或报告任务正在运行。"));
        return;
    }
    m_cpuRunning.store(true);
    m_cpuCancelRequested.store(false);
    m_cpuHadProtocolError = false;
    m_cpuCoreLabels.clear();

    CpuCoreResultsList().Items().Clear();
    CpuAverageResult().Text(L"\x2014");
    CpuMultiResult().Text(L"\x2014");
    CpuOutputBox().Text(L"");
    CpuProgressBar().Value(0.0);
    CpuProgressText().Text(L"0%");
    setTaskbarProgress(true, 0.0);
    CpuCurrentCoreText().Text(locText("Starting CPU worker...", "正在启动 CPU 测试进程…"));
    setCpuStatus(StatusLight::Running, locText("Running", "运行中"));
    CpuCancelButton().IsEnabled(true);
    CpuModeBox().IsEnabled(false);
    CpuDurationPresetBox().IsEnabled(false);
    CpuTimeBox().IsEnabled(false);
    CpuWarmupBox().IsEnabled(false);

    const auto engine = std::filesystem::absolute(pathFromUtf8(m_enginePath));
    const auto workingDirectory = engine.parent_path();
    auto strong = get_strong();
    auto dispatcher = m_dispatcher;

    std::thread([this, strong, dispatcher, engine, workingDirectory,
                 mode = std::move(mode), seconds, warmupSeconds]()
    {
      try
      {
        CpuProtocolAudit protocolAudit;
        auto finish = [this, strong, dispatcher](DWORD exitCode,
                                                 std::string errorMessage)
        {
            dispatcher.TryEnqueue([this, strong, exitCode,
                                   errorMessage = std::move(errorMessage)]()
            {
                // A late Cancel click must not turn an already-successful exit
                // into "Cancelled" while this callback waits in the UI queue.
                // TerminateProcess uses ERROR_CANCELLED as the exit code.
                const bool cancelled = exitCode == ERROR_CANCELLED ||
                    (exitCode != 0 && m_cpuCancelRequested.load());
                m_cpuRunning.store(false);
                endTask(ActiveTask::CpuBenchmark);
                setTaskbarProgress(false, 0.0);
                CpuCancelButton().IsEnabled(false);
                CpuModeBox().IsEnabled(true);
                CpuDurationPresetBox().IsEnabled(true);
                CpuTimeBox().IsEnabled(true);
                CpuWarmupBox().IsEnabled(true);

                if (cancelled)
                {
                    setCpuStatus(StatusLight::Ready, locText("Cancelled", "已取消"));
                    CpuCurrentCoreText().Text(locText("Benchmark cancelled", "CPU 测试已取消"));
                    return;
                }
                if (exitCode != 0 || !errorMessage.empty() || m_cpuHadProtocolError)
                {
                    if (!errorMessage.empty())
                    {
                        auto output = std::wstring(CpuOutputBox().Text().c_str());
                        output += L"\r\n[GUI] ";
                        output += u8(errorMessage).c_str();
                        output += L"\r\n";
                        CpuOutputBox().Text(output);
                    }
                    setCpuStatus(StatusLight::Error, locText("Failed; see raw output", "失败；请查看原始输出"));
                    CpuCurrentCoreText().Text(locText("CPU benchmark failed", "CPU 测试失败"));
                    return;
                }

                CpuProgressBar().Value(100.0);
                CpuProgressText().Text(L"100%");
                setCpuStatus(StatusLight::Ready, locText("Done", "完成"));
                CpuCurrentCoreText().Text(locText("CPU benchmark completed", "CPU 测试完成"));
            });
        };

        SECURITY_ATTRIBUTES security{};
        security.nLength = sizeof(security);
        security.bInheritHandle = TRUE;
        HANDLE readPipe = nullptr;
        HANDLE writePipe = nullptr;
        if (!::CreatePipe(&readPipe, &writePipe, &security, 0))
        {
            const DWORD pipeCreateError = ::GetLastError();
            finish(pipeCreateError,
                   "CreatePipe failed: " + win32ErrorText(pipeCreateError));
            return;
        }
        ::SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

        std::wostringstream command;
        command.imbue(std::locale::classic());
        command << L'"' << engine.wstring() << L"\" --cpu-benchmark ";
        command << std::wstring(mode.begin(), mode.end());
        command << L" --cpu-time " << std::setprecision(12) << seconds
                << L" --cpu-warmup " << std::setprecision(12) << warmupSeconds;
        auto commandLine = command.str();
        std::vector<wchar_t> mutableCommand(commandLine.begin(), commandLine.end());
        mutableCommand.push_back(L'\0');

        STARTUPINFOW startup{};
        startup.cb = sizeof(startup);
        startup.dwFlags = STARTF_USESTDHANDLES;
        startup.hStdOutput = writePipe;
        startup.hStdError = writePipe;
        // A WinUI process normally has no console stdin. Passing that NULL or
        // non-inheritable pseudo handle with STARTF_USESTDHANDLES can make
        // CreateProcess fail with ERROR_INVALID_HANDLE on clean installs.
        HANDLE nullInput = ::CreateFileW(
            L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
            &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (nullInput == INVALID_HANDLE_VALUE)
        {
            const DWORD nullError = ::GetLastError();
            ::CloseHandle(writePipe);
            ::CloseHandle(readPipe);
            finish(nullError, "Could not open NUL for child stdin: " +
                   win32ErrorText(nullError));
            return;
        }
        startup.hStdInput = nullInput;
        PROCESS_INFORMATION process{};
        const BOOL created = ::CreateProcessW(
            engine.c_str(), mutableCommand.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, workingDirectory.c_str(), &startup, &process);
        const DWORD createError = created ? ERROR_SUCCESS : ::GetLastError();
        ::CloseHandle(nullInput);
        ::CloseHandle(writePipe);
        writePipe = nullptr;
        if (!created)
        {
            ::CloseHandle(readPipe);
            finish(createError, "CreateProcess failed: " + win32ErrorText(createError));
            return;
        }
        ::CloseHandle(process.hThread);

        {
            std::lock_guard<std::mutex> lock(m_cpuProcessMutex);
            m_cpuProcess = process.hProcess;
            if (m_cpuCancelRequested.load())
                ::TerminateProcess(process.hProcess, ERROR_CANCELLED);
        }

        auto dispatchLines = [this, strong, dispatcher](std::vector<std::string> lines)
        {
            if (lines.empty()) return;
            dispatcher.TryEnqueue([this, strong, lines = std::move(lines)]()
            {
                auto raw = std::wstring(CpuOutputBox().Text().c_str());
                for (auto const& line : lines)
                {
                    raw += u8(line).c_str();
                    raw += L"\r\n";
                }
                // A formal per-core run can emit tens of thousands of progress
                // records. Lines are batched on the reader thread so this
                // growing TextBox is copied at most a few times per second.
                constexpr std::size_t kMaxVisibleCpuOutput = 256u * 1024u;
                if (raw.size() > kMaxVisibleCpuOutput)
                {
                    const auto firstLine = raw.find(L'\n', raw.size() - kMaxVisibleCpuOutput);
                    raw.erase(0, firstLine == std::wstring::npos ?
                        raw.size() - kMaxVisibleCpuOutput : firstLine + 1);
                }
                CpuOutputBox().Text(raw);

                std::size_t lastProgress = lines.size();
                for (std::size_t i = 0; i < lines.size(); ++i)
                    if (parseCpuProtocolLine(lines[i]).type == CpuLineType::Progress)
                        lastProgress = i;

                auto processLine = [this](std::string const& line)
                {
                const auto parsed = parseCpuProtocolLine(line);
                if (parsed.type == CpuLineType::Other) return;

                if (parsed.type == CpuLineType::Meta)
                {
                    const auto name = cpuField(parsed, { "name", "cpu_name" });
                    const auto logical = cpuInteger(parsed, { "logical_count" });
                    const auto physical = cpuInteger(parsed, { "physical_count" });
                    const auto affinity = cpuField(parsed, { "affinity_capability" });
                    std::ostringstream label;
                    label << (name.empty() ? m_cpuName : name);
                    if (logical) label << "  |  " << *logical << " logical";
                    if (physical) label << " / " << *physical << " physical";
                    if (!affinity.empty()) label << "  |  " << affinity;
                    CpuNameText().Text(u8(label.str()));
                    return;
                }

                if (parsed.type == CpuLineType::Topology)
                {
                    const auto core = cpuInteger(parsed, { "core_index", "ordinal" });
                    if (!core) return;
                    const int logical = cpuInteger(parsed, { "logical_index" }).value_or(*core);
                    const int physical = cpuInteger(parsed, { "physical_core" }).value_or(*core);
                    const int smt = cpuInteger(parsed, { "smt_index" }).value_or(0);
                    const int width = cpuInteger(parsed, { "smt_width" }).value_or(1);
                    const int group = cpuInteger(parsed, { "group" }).value_or(0);
                    const auto coreClass = cpuField(parsed, { "core_class", "class" });
                    std::ostringstream label;
                    label << "LP " << logical << " / physical " << (physical + 1)
                          << " / SMT " << (smt + 1) << '/' << width;
                    if (group != 0) label << " / group " << group;
                    if (!coreClass.empty()) label << " / " << coreClass;
                    m_cpuCoreLabels[*core] = label.str();
                    return;
                }

                if (parsed.type == CpuLineType::Progress)
                {
                    const double fraction = cpuNumber(parsed, { "overall_fraction", "fraction" })
                        .value_or(cpuNumber(parsed, { "core_fraction" }).value_or(0.0));
                    const double percent = std::clamp(fraction * 100.0, 0.0, 100.0);
                    CpuProgressBar().Value(percent);
                    std::wostringstream progress;
                    progress << std::fixed << std::setprecision(1) << percent << L'%';
                    CpuProgressText().Text(progress.str());
                    setTaskbarProgress(true, percent / 100.0);

                    const auto modeName = cpuField(parsed, { "mode" });
                    const auto phase = cpuField(parsed, { "phase" });
                    const int core = cpuInteger(parsed, { "core_index" }).value_or(-1);
                    hstring phaseText = phase == "warmup"
                        ? locText("warming up", "预热")
                        : phase == "measure"
                            ? locText("measuring", "测量")
                            : locText("finishing", "收尾");
                    if (modeName == "multi" || core < 0)
                    {
                        std::wstring status = locText("All-core", "多核").c_str();
                        status += L" · "; status += phaseText.c_str();
                        CpuCurrentCoreText().Text(status);
                    }
                    else
                    {
                        std::string coreLabel = "logical processor " + std::to_string(core + 1);
                        if (auto it = m_cpuCoreLabels.find(core); it != m_cpuCoreLabels.end())
                            coreLabel = it->second;
                        std::wstring status = u8(coreLabel).c_str();
                        status += L" · "; status += phaseText.c_str();
                        CpuCurrentCoreText().Text(status);
                    }
                    return;
                }

                if (parsed.type == CpuLineType::Error)
                {
                    m_cpuHadProtocolError = true;
                    auto message = cpuField(parsed, { "message", "error" });
                    std::replace(message.begin(), message.end(), '_', ' ');
                    setCpuStatus(StatusLight::Error, message.empty()
                        ? locText("CPU engine error", "CPU 引擎错误") : u8(message));
                    return;
                }

                const auto kind = cpuField(parsed, { "kind", "type" });
                const auto unit = cpuField(parsed, { "unit" });
                if (kind == "core")
                {
                    const int core = cpuInteger(parsed, { "core_index" }).value_or(-1);
                    const auto score = cpuNumber(parsed, { "score" });
                    if (core < 0 || !score) return;
                    const auto affinity = cpuField(parsed, { "affinity" });
                    const bool valid = cpuInteger(parsed, { "valid" }).value_or(1) != 0;
                    std::string label = "logical processor " + std::to_string(core + 1);
                    if (auto it = m_cpuCoreLabels.find(core); it != m_cpuCoreLabels.end())
                        label = it->second;
                    std::string row = "#" + std::to_string(core + 1) + "  " + label
                        + "  |  " + formatCpuScore(*score, unit);
                    if (!affinity.empty()) row += "  [" + affinity + "]";
                    if (!valid || affinity == "failed" || affinity == "partial")
                        row = "!  " + row;
                    CpuCoreResultsList().Items().Append(box_value(u8(row)));
                    return;
                }
                if (kind == "summary")
                {
                    const auto average = cpuNumber(parsed, { "average_score", "score" });
                    if (average)
                    {
                        std::string value = formatCpuScore(*average, unit);
                        if (const auto contract = cpuField(parsed, { "score_contract" });
                            !contract.empty()) value += "  [" + contract + "]";
                        CpuAverageResult().Text(u8(value));
                    }
                    return;
                }
                if (kind == "multi")
                {
                    const auto score = cpuNumber(parsed, { "score" });
                    if (!score) return;
                    std::string value = formatCpuScore(*score, unit);
                    if (const auto threads = cpuInteger(parsed, { "thread_count" }))
                        value += "  (" + std::to_string(*threads) + " threads)";
                    const auto affinity = cpuField(parsed, { "affinity" });
                    if (!affinity.empty()) value += "  [" + affinity + "]";
                    if (const auto contract = cpuField(parsed, { "score_contract" });
                        !contract.empty()) value += "  [" + contract + "]";
                    CpuMultiResult().Text(u8(value));
                }
                };

                // Apply only the newest progress record in this batch. Meta,
                // topology, errors and result records are never dropped.
                for (std::size_t i = 0; i < lines.size(); ++i)
                {
                    if (parseCpuProtocolLine(lines[i]).type == CpuLineType::Progress &&
                        i != lastProgress) continue;
                    processLine(lines[i]);
                }
            });
        };

        std::string pending;
        std::vector<std::string> uiBatch;
        auto lastUiDispatch = std::chrono::steady_clock::now();
        auto flushUiBatch = [&]()
        {
            if (uiBatch.empty()) return;
            dispatchLines(std::move(uiBatch));
            uiBatch.clear();
            lastUiDispatch = std::chrono::steady_clock::now();
        };
        auto queueLine = [&](std::string line)
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const auto parsed = parseCpuProtocolLine(line);
            protocolAudit.observe(parsed);
            uiBatch.push_back(std::move(line));
            const auto now = std::chrono::steady_clock::now();
            const bool important = parsed.type == CpuLineType::Result ||
                                   parsed.type == CpuLineType::Error;
            if (important || uiBatch.size() >= 128 ||
                now - lastUiDispatch >= std::chrono::milliseconds(250))
                flushUiBatch();
        };
        std::array<char, 4096> buffer{};
        DWORD bytesRead = 0;
        DWORD readError = ERROR_SUCCESS;
        while (::ReadFile(readPipe, buffer.data(), static_cast<DWORD>(buffer.size()),
                          &bytesRead, nullptr) && bytesRead > 0)
        {
            pending.append(buffer.data(), bytesRead);
            for (;;)
            {
                const auto newline = pending.find('\n');
                if (newline == std::string::npos) break;
                queueLine(pending.substr(0, newline));
                pending.erase(0, newline + 1);
            }
        }
        readError = ::GetLastError();
        if (!pending.empty()) queueLine(std::move(pending));
        flushUiBatch();
        ::CloseHandle(readPipe);

        ::WaitForSingleObject(process.hProcess, INFINITE);
        DWORD exitCode = 1;
        ::GetExitCodeProcess(process.hProcess, &exitCode);
        {
            std::lock_guard<std::mutex> lock(m_cpuProcessMutex);
            if (m_cpuProcess == process.hProcess) m_cpuProcess = nullptr;
        }
        ::CloseHandle(process.hProcess);

        std::string pipeError;
        if (readError != ERROR_SUCCESS && readError != ERROR_BROKEN_PIPE)
            pipeError = "stdout pipe failed: " + win32ErrorText(readError);
        // A normal exit is always audited, even if Cancel was clicked after
        // the process had already ended. Late UI input cannot bypass protocol
        // completeness checks.
        if (exitCode == 0 && pipeError.empty())
            pipeError = protocolAudit.validate(mode);
        finish(exitCode, std::move(pipeError));
      }
      catch (...)
      {
          dispatcher.TryEnqueue([this, strong]()
          {
              m_cpuRunning.store(false);
              endTask(ActiveTask::CpuBenchmark);
              CpuCancelButton().IsEnabled(false);
              CpuModeBox().IsEnabled(true);
              CpuDurationPresetBox().IsEnabled(true);
              CpuTimeBox().IsEnabled(true);
              CpuWarmupBox().IsEnabled(true);
              setCpuStatus(StatusLight::Error, locText("Failed", "运行失败"));
              CpuCurrentCoreText().Text(locText(
                  "CPU worker failed with an unexpected exception.",
                  "CPU 工作线程发生意外异常。"));
          });
      }
    }).detach();
}

void MainWindow::updateExtraLabel()
{
    auto wl = selected(WorkloadBox());
    int preset = PresetBox().SelectedIndex();
    bool customRun = preset == 1;
    bool fullAnalysis = preset == 2 || preset == 3;
    bool workloadSelectable = customRun || fullAnalysis;
    bool particleTest = preset == 5;
    bool flightsTest = preset == 4;
    bool particleWorkload = wl == "stream" || wl == "render3d";
    bool gpuBurnWorkload = wl == "gpu_burn";
    bool showParticles = particleTest || (workloadSelectable && particleWorkload);
    bool fixedQualityLiquid = wl == "cinematic_liquid" || wl == "cinematic_liquid_v1";
    bool showExtra = flightsTest ||
        (workloadSelectable && !particleWorkload && !fixedQualityLiquid);
    bool fluidWorkload = workloadSelectable && wl == "fluid";
    std::string infoWl = workloadSelectable ? wl : "stream";

    // Custom and Full Analysis both honour the selected workload. Other
    // specialised presets retain their historical Stream-only behaviour.
    WorkloadBox().IsEnabled(workloadSelectable);
    // Quick chooses automatically; Full-All schedules every enumerated GPU.
    // Disable a selector that those presets intentionally do not consume.
    const bool gpuPickerOn = m_gpuEnumerationComplete && preset != 0 && preset != 3;
    const bool apiPickerOn = m_gpuEnumerationComplete && preset != 0;
    updateApiPickerSummary();
    applyComboEnabledLook(GpuBox(), gpuPickerOn);
    applyComboEnabledLook(ApiPickerBox(), apiPickerOn);
    ApiPickerHeader().Opacity(apiPickerOn ? 1.0 : 0.55);
    if (apiPickerOn)
        ApiPickerHeader().ClearValue(TextBlock::ForegroundProperty());
    else
        ApiPickerHeader().Foreground(disabledTextBrush());
    const bool showPrecision = workloadSelectable && wl == "synthpeak";
    PrecisionColumn().Visibility(showPrecision ? Visibility::Visible : Visibility::Collapsed);
    PrecisionBox().IsEnabled(showPrecision);
    bool headlessSupported = customRun && !(gpuBurnWorkload || wl == "gpu_stress" || wl == "stress"
        || wl == "render3d" || wl == "volumetric" || wl == "fluid"
        || wl == "cinematic_liquid" || wl == "cinematic_liquid_v1");
    if (!headlessSupported) HeadlessBox().IsChecked(false);
    HeadlessBox().IsEnabled(headlessSupported);
    bool hostMemorySupported = !(workloadSelectable && fixedQualityLiquid);
    if (!hostMemorySupported) HostMemBox().IsChecked(false);
    HostMemBox().IsEnabled(hostMemorySupported);

    ParticlePresetBox().Visibility(showParticles ? Visibility::Visible : Visibility::Collapsed);
    CustomParticleBox().Visibility(showParticles && selected(ParticlePresetBox()) == "custom"
        ? Visibility::Visible : Visibility::Collapsed);
    ExtraBox().Visibility(showExtra ? Visibility::Visible : Visibility::Collapsed);
    FluidJacobiBox().Visibility(fluidWorkload ? Visibility::Visible : Visibility::Collapsed);

    if (flightsTest)             ExtraBox().Header(locContent("Flights (--flights)", "Flights (--flights)"));
    else if (wl == "nbody")      ExtraBox().Header(locContent("Bodies (--bodies)", "天体数 (--bodies)"));
    else if (gpuBurnWorkload)     ExtraBox().Header(locContent("Fixed steps (--iter)", "固定步数 (--iter)"));
    else if (wl == "gpu_stress") ExtraBox().Header(locContent("Iterations (--iter)", "迭代次数 (--iter)"));
    else if (wl == "stress")     ExtraBox().Header(locContent("Iterations (--iter)", "迭代次数 (--iter)"));
    else if (wl == "synthpeak")  ExtraBox().Header(locContent("Iterations (--iter)", "迭代次数 (--iter)"));
    else if (wl == "volumetric") ExtraBox().Header(locContent("Ray steps (--steps)", "光线步数 (--steps)"));
    else if (wl == "fluid")      ExtraBox().Header(locContent("Grid size (--grid)", "网格尺寸 (--grid)"));
    else                          ExtraBox().Header(locContent("Extra", "额外参数"));

    ExtraBox().PlaceholderText(gpuBurnWorkload
        ? locText("leave empty: safe auto-tune; fixed 16-32", "留空：安全自动标定；固定值 16-32")
        : wl == "nbody"
        ? locText("default 65536; DX11 FL10/SM4: max 4096", "默认 65536；DX11 FL10/SM4：最多 4096")
        : wl == "volumetric"
        ? locText("optional; default 96", "可选；默认 96")
        : wl == "fluid"
        ? locText("optional; default 256", "可选；默认 256")
        : locText("optional", "可选"));
    FluidJacobiBox().Header(locContent("Jacobi iterations (--jacobi)", "Jacobi 迭代次数 (--jacobi)"));
    FluidJacobiBox().PlaceholderText(locText("optional; default 30", "可选；默认 30"));

    updateDurationValueEnabled();

    WorkloadInfo().IsOpen(true);
    WorkloadInfo().Severity(InfoBarSeverity::Informational);
    if (infoWl == "stream")
    {
        WorkloadInfo().Title(locText("Particle", "粒子"));
        WorkloadInfo().Message(locText(
            "Moves many particles each frame to measure GPU memory / bandwidth throughput.",
            "每帧移动大量粒子，主要衡量 GPU 显存带宽与内存吞吐。"));
    }
    else if (infoWl == "gpu_burn")
    {
        WorkloadInfo().Title(locText("Plasma x Kaleidoscope", "等离子晶核 × 万花镜"));
        WorkloadInfo().Message(locText(
            "Combines a solid raymarched Plasma Bloom foreground with the woven v2 Mangekyo kaleidoscope background for a short graphics burst (or Until Cancel for open-ended stress).",
            "将实心光线步进等离子晶核作为前景，与 v2 织纹万花镜背景合成；默认可限时跑分，也可选「直到取消」持续压测。"));
    }
    else if (infoWl == "cinematic_liquid")
    {
        WorkloadInfo().Title(locText("Fluid", "流体"));
        WorkloadInfo().Message(locText(
            "Simulates and renders a 3D liquid scene with solid objects (Vulkan only).",
            "模拟并渲染带固体的 3D 液体场景（目前仅支持 Vulkan）。"));
    }
    else if (infoWl == "cinematic_liquid_v1")
    {
        WorkloadInfo().Title(locText("Legacy Fluid v1", "旧版流体 v1"));
        WorkloadInfo().Message(locText(
            "Older dam-break liquid scene, kept for comparison with earlier results.",
            "旧版溃坝液体场景，用于和历史成绩对比。"));
    }
    else if (infoWl == "gpu_stress")
    {
        WorkloadInfo().Title(locText("GraphicsBurn Component", "GraphicsBurn 分项"));
        WorkloadInfo().Message(locText(
            "Breaks graphics load into smaller parts for deeper diagnosis.",
            "把图形负载拆成更小分项，便于深入诊断瓶颈。"));
    }
    else if (infoWl == "nbody")
    {
        WorkloadInfo().Title(locText("N-Body", "N-Body"));
        WorkloadInfo().Message(locText(
            "Computes gravity between many particles to measure GPU compute throughput.",
            "计算大量粒子间的引力相互作用，衡量 GPU 计算吞吐。"));
    }
    else if (infoWl == "synthpeak")
    {
        WorkloadInfo().Title(locText("SynthPeak", "SynthPeak"));
        WorkloadInfo().Message(locText(
            "Runs synthetic math loops to estimate peak ALU throughput.",
            "跑合成运算循环，估算峰值 ALU 吞吐。"));
    }
    else if (infoWl == "stress")
    {
        WorkloadInfo().Title(locText("Legacy Stress", "旧版压力测试"));
        WorkloadInfo().Message(locText(
            "Fragment-shader math stress test from an earlier prototype.",
            "早期片元着色器运算压力测试原型。"));
    }
    else if (infoWl == "render3d")
    {
        WorkloadInfo().Title(locText("Legacy 3D", "旧版 3D"));
        WorkloadInfo().Message(locText(
            "Draws many billboard sprites to measure simple 3D draw throughput.",
            "绘制大量 Billboard 精灵，衡量简单 3D 绘制吞吐。"));
    }
    else if (infoWl == "volumetric")
    {
        WorkloadInfo().Title(locText("Volumetric", "体积渲染"));
        WorkloadInfo().Message(locText(
            "Raymarches a procedural volume field to stress fill-rate and shader cost.",
            "对程序化体积场做光线步进，压测填充率与着色器开销。"));
    }
    else if (infoWl == "fluid")
    {
        WorkloadInfo().Severity(InfoBarSeverity::Warning);
        WorkloadInfo().Title(locText("Legacy 2D Fluid", "旧版 2D 流体"));
        WorkloadInfo().Message(locText(
            "Old 2D fluid simulation (Vulkan / DX12 / DX11 / OpenGL); mainly for reference, not a primary score.",
            "旧版 2D 流体模拟（Vulkan / DX12 / DX11 / OpenGL），主要作参考，不是主成绩项。"));
    }
}

std::vector<std::string> MainWindow::selectedApis()
{
    auto selected = checkedTags(SupportedApisPanel());
    auto unsupported = checkedTags(UnsupportedApisPanel());
    selected.insert(unsupported.begin(), unsupported.end());

    std::vector<std::string> ordered;
    for (auto const& api : kRunApis)
        if (selected.find(api.token) != selected.end())
            ordered.emplace_back(api.token);
    return ordered;
}

void MainWindow::updateApiPickerSummary()
{
    hstring summary;
    if (!m_gpuEnumerationComplete)
    {
        summary = locText("Detecting…", "检测中…");
    }
    else if (PresetBox().SelectedIndex() == 0)
    {
        summary = locText("Automatic (quick preset)", "自动（快速预设）");
    }
    else
    {
        const auto apis = selectedApis();
        if (apis.empty())
            summary = locText("Select APIs", "选择 API");
        else if (apis.size() == std::size(kRunApis))
            summary = locText("All APIs (4)", "全部 API（4）");
        else if (apis.size() == 1)
        {
            for (auto const& api : kRunApis)
                if (apis.front() == api.token)
                {
                    summary = u8(api.label);
                    break;
                }
        }
        else
        {
            const auto count = std::to_string(apis.size());
            summary = i18n::currentLang() == i18n::Lang::Zh
                ? u8("已选 " + count + " 个 API")
                : u8(count + " APIs selected");
        }
    }

    ApiPickerSummaryItem().Content(box_value(summary));
    // ComboBox caches the closed-state caption; re-select so language switches
    // (e.g. Detecting… → 检测中…) actually repaint.
    ApiPickerBox().SelectedItem(nullptr);
    ApiPickerBox().SelectedItem(ApiPickerSummaryItem());
    std::wstring accessible = locText("Graphics API", "图形 API").c_str();
    accessible += L": ";
    accessible += summary.c_str();
    Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
        ApiPickerBox(), hstring(accessible));
}

void MainWindow::rebuildApiPicker(bool preserveSelection)
{
    if (!m_gpuEnumerationComplete) return;

    const bool allGpusPreset = PresetBox().SelectedIndex() == 3;
    const bool hasAllGpuTargets = allGpusPreset && !m_gpuApiSupport.empty();
    const auto supportedHeading = hasAllGpuTargets
        ? locText("Supported by every GPU", "所有 GPU 均支持")
        : locText("Supported", "支持");
    const auto unsupportedHeading = hasAllGpuTargets
        ? locText("Unavailable on one or more GPUs", "至少一个 GPU 不支持")
        : locText("Not reported as supported", "未报告支持");
    SupportedApisLabel().Text(supportedHeading);
    UnsupportedApisLabel().Text(unsupportedHeading);
    Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
        SupportedApisGroup(), supportedHeading);
    Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(
        UnsupportedApisGroup(), unsupportedHeading);

    std::set<std::string> previous;
    if (preserveSelection && m_apiSelectionInitialized)
    {
        auto supported = checkedTags(SupportedApisPanel());
        auto unsupported = checkedTags(UnsupportedApisPanel());
        previous.insert(supported.begin(), supported.end());
        previous.insert(unsupported.begin(), unsupported.end());
    }

    SupportedApisPanel().Children().Clear();
    UnsupportedApisPanel().Children().Clear();
    const int gpuRow = allGpusPreset ? 0 : GpuBox().SelectedIndex();
    const auto workload = selected(WorkloadBox());
    // Prefer named ComboBoxItem identity: Tag can be lost after Content-only
    // language refresh, which previously left all four APIs selectable on Fluid.
    auto workloadItem = WorkloadBox().SelectedItem().try_as<ComboBoxItem>();
    const bool vulkanOnlyWorkload =
        workload == "cinematic_liquid" || workload == "cinematic_liquid_v1" ||
        (workloadItem &&
         (workloadItem == WorkloadCinematicLiquid() ||
          workloadItem == WorkloadCinematicLiquidV1()));
    for (auto const& api : kRunApis)
    {
        if (vulkanOnlyWorkload && api.token != "vulkan")
            continue; // Interactive pool / legacy liquid: Vulkan is the only API.

        bool supported = false;
        std::size_t supportCount = 0;
        for (auto const& capabilities : m_gpuApiSupport)
            if (capabilities[api.supportIndex]) ++supportCount;

        if (allGpusPreset)
        {
            supported = !m_gpuApiSupport.empty() &&
                        supportCount == m_gpuApiSupport.size();
        }
        else if (gpuRow > 0 && static_cast<size_t>(gpuRow - 1) < m_gpuApiSupport.size())
        {
            supported = m_gpuApiSupport[gpuRow - 1][api.supportIndex];
        }
        else
        {
            // "Auto" can choose the best adapter independently for each API.
            for (auto const& capabilities : m_gpuApiSupport)
                supported = supported || capabilities[api.supportIndex];
        }

        bool checked = false;
        if (vulkanOnlyWorkload)
            checked = (api.token == "vulkan" && supported);
        else if (m_apiSelectionInitialized)
            checked = previous.find(api.token) != previous.end();
        else
            checked = supported;
        std::string label = api.label;
        if (allGpusPreset && supportCount > 0 && supportCount < m_gpuApiSupport.size())
            label += " (" + std::to_string(supportCount) + "/" +
                     std::to_string(m_gpuApiSupport.size()) +
                     (i18n::currentLang() == i18n::Lang::Zh ? " 个 GPU)" : " GPUs)");
        // Vulkan-only workloads never list other APIs, even as "unsupported"
        // checkboxes — those entries made it look like DX12/11/GL were still options.
        appendFilterCheckBox(
            supported ? SupportedApisPanel() : UnsupportedApisPanel(),
            label, api.token, checked);
    }

    if (vulkanOnlyWorkload)
    {
        UnsupportedApisPanel().Children().Clear();
        SelectAllRunApis().IsEnabled(false);
        ClearAllRunApis().IsEnabled(false);
    }
    else
    {
        SelectAllRunApis().IsEnabled(true);
        ClearAllRunApis().IsEnabled(true);
    }

    SupportedApisGroup().Visibility(
        SupportedApisPanel().Children().Size() > 0
            ? Visibility::Visible : Visibility::Collapsed);
    UnsupportedApisGroup().Visibility(
        UnsupportedApisPanel().Children().Size() > 0
            ? Visibility::Visible : Visibility::Collapsed);
    m_apiSelectionInitialized = true;
    updateApiPickerSummary();
    if (vulkanOnlyWorkload)
    {
        UnsupportedApisHint().Text(locText(
            "This fluid workload currently supports Vulkan only.",
            "该流体测试目前仅支持 Vulkan。"));
    }
    else
    {
        UnsupportedApisHint().Text(locText(
            "Unsupported selections remain available and will be reported by the CLI.",
            "不支持的 API 仍可勾选；运行后由 CLI 返回明确错误。"));
    }
}

void MainWindow::OnGpuSelectionChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady || !m_gpuEnumerationComplete) return;
    rebuildApiPicker(true);
}

// ---- GPU enumeration -------------------------------------------------------
void MainWindow::populateGpus()
{
    m_gpuEnumerationComplete = false;
    m_apiSelectionInitialized = false;
    syncActionButtonsEnabled();
    GpuBox().Items().Clear();
    m_gpuIndices.clear();
    m_gpuApiSupport.clear();
    SupportedApisPanel().Children().Clear();
    UnsupportedApisPanel().Children().Clear();
    updateApiPickerSummary();
    applyComboEnabledLook(ApiPickerBox(), false);
    applyComboEnabledLook(GpuBox(), false);
    ApiPickerHeader().Opacity(0.55);
    ApiPickerHeader().Foreground(disabledTextBrush());
    auto autoItem = ComboBoxItem();
    autoItem.Content(locContent("(auto)", "（自动）"));
    GpuBox().Items().Append(autoItem);
    GpuBox().SelectedIndex(0);
    applyComboEnabledLook(GpuBox(), false);
    if (m_enginePath.empty())
    {
        ApiPickerSummaryItem().Content(locContent("Engine not found", "未找到引擎"));
        ApiPickerBox().SelectedItem(ApiPickerSummaryItem());
        m_gpuEnumerationComplete = true;
        syncActionButtonsEnabled();
        return;
    }

    // One row of `--list-gpus`: GPU \t idx \t name \t vk \t dx12 \t dx11
    // \t opengl \t dx11FeatureLevel \t dx11Compute. The last two fields are
    // optional so a newer GUI can still drive an older engine build.
    struct GpuRow { int idx; std::string name; std::array<bool, 5> api; };

    auto strong = get_strong();
    auto disp = m_dispatcher;
    std::string engine = m_enginePath;
    std::thread([this, strong, disp, engine]()
    {
      try
      {
        // --gui-worker: same crash-handler hygiene as benchmark workers while probing.
        auto detection = captureCliProcess(
            { engine, "--gui-worker", "--list-gpus" }, 60u * 1000u);
        std::string const& out = detection.output;
        std::vector<GpuRow> gpus;
        std::istringstream ss(out); std::string line;
        while (std::getline(ss, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("GPU\t", 0) != 0) continue;
            std::vector<std::string> f; std::stringstream ls(line); std::string tok;
            while (std::getline(ls, tok, '\t')) f.push_back(tok);
            if (f.size() < 3) continue;
            GpuRow r;
            try { r.idx = std::stoi(f[1]); } catch (...) { continue; }
            r.name = f[2];
            // Default to supported when the field is absent (older engine).
            for (int k = 0; k < 4; ++k)
                r.api[k] = (f.size() > (size_t)(3 + k)) ? (f[3 + k] == "1") : true;
            r.api[4] = f.size() > 8 ? (f[8] == "1") : r.api[2];
            gpus.push_back(std::move(r));
        }
        if (!disp || !disp.TryEnqueue([this, strong, gpus, detection = std::move(detection)]()
        {
            if (!uiAlive()) return;
            try
            {
                for (auto& g : gpus)
                {
                    m_gpuIndices.push_back(g.idx);
                    m_gpuApiSupport.push_back(g.api);
                    std::string label = isSoftwareGpu(g.name)
                        ? std::to_string(g.idx) + ": " + (m_cpuName.empty() ? "CPU" : m_cpuName) + "  (CPU / WARP)"
                        : std::to_string(g.idx) + ": " + g.name;
                auto it = ComboBoxItem();
                it.Content(box_value(u8(label)));
                GpuBox().Items().Append(it);
                }
                m_gpuEnumerationComplete = true;
                rebuildApiPicker(false);
                updateExtraLabel();
                syncActionButtonsEnabled();
                if (detection.exitCode != 0)
                {
                    OutputBox().Text(u8(clipForUi(detection.output)));
                    setGpuStatus(StatusLight::Error, detection.exitCode == -2
                        ? locText("GPU detection timed out; APIs remain manually selectable.",
                                  "GPU 检测超时；仍可手动勾选 API。")
                        : locText("GPU detection failed; APIs remain manually selectable.",
                                  "GPU 检测失败；仍可手动勾选 API。"));
                }
            }
            catch (winrt::hresult_error const& e)
            {
                appendGuiCrashLog("populateGpus.ui", winrt::to_string(e.message()).c_str());
                m_gpuEnumerationComplete = true;
                syncActionButtonsEnabled();
            }
            catch (...)
            {
                appendGuiCrashLog("populateGpus.ui", "unknown");
                m_gpuEnumerationComplete = true;
                syncActionButtonsEnabled();
            }
        }))
        {
            appendGuiCrashLog("populateGpus", "TryEnqueue failed");
        }
      }
      catch (...)
      {
          appendGuiCrashLog("populateGpus.worker", "exception");
          if (!disp || !disp.TryEnqueue([this, strong]()
          {
              if (!uiAlive()) return;
              try
              {
                  m_gpuEnumerationComplete = true;
                  rebuildApiPicker(false);
                  updateExtraLabel();
                  syncActionButtonsEnabled();
                  setGpuStatus(StatusLight::Error, locText(
                      "GPU detection failed; APIs remain manually selectable.",
                      "GPU 检测失败；仍可手动勾选 API。"));
              }
              catch (...)
              {
                  m_gpuEnumerationComplete = true;
                  syncActionButtonsEnabled();
              }
          }))
          {
              // Last resort: mark complete so a later language refresh can recover.
              m_gpuEnumerationComplete = true;
          }
      }
    }).detach();
}

void MainWindow::OnWorkloadChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady) return;
    if (!m_suppressCombo)
    {
        // Parameter meanings differ radically between workloads. Do not carry
        // an N-body count into GPU Stress as an unsafe explicit iteration count.
        ExtraBox().Text(L"");
        FluidJacobiBox().Text(L"");
    }
    updateExtraLabel();
    if (m_gpuEnumerationComplete) rebuildApiPicker(true);
}

void MainWindow::OnParticlePresetChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady || m_suppressCombo) return;
    updateExtraLabel();
}

void MainWindow::OnDurationUnitChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady || m_suppressCombo) return;

    const auto unit = durationUnitTag();
    auto cur = to_string(DurationValueBox().Text());
    const bool looksDefault =
        cur.empty() || cur == "15" || cur == "5" || cur == "1" || cur == "60" || cur == "600";

    if (unit == "seconds" && looksDefault)       DurationValueBox().Text(L"15");
    else if (unit == "minutes" && looksDefault)  DurationValueBox().Text(L"5");
    else if (unit == "hours" && looksDefault)    DurationValueBox().Text(L"1");
    else if (unit == "frames" && looksDefault)   DurationValueBox().Text(L"600");

    updateDurationValueEnabled();
    syncCaptureControls();
}

std::string MainWindow::durationUnitTag()
{
    return selected(DurationUnitBox());
}

bool MainWindow::isUnlimitedDuration()
{
    return durationUnitTag() == "unlimited";
}

void MainWindow::updateDurationValueEnabled()
{
    DurationValueBox().IsEnabled(!isUnlimitedDuration());
}

double MainWindow::durationAmountValue()
{
    const auto unit = durationUnitTag();
    auto val = to_string(DurationValueBox().Text());
    double amount = 15.0;
    if (unit == "frames") amount = 600.0;
    else if (unit == "minutes") amount = 5.0;
    else if (unit == "hours") amount = 1.0;

    if (!val.empty())
    {
        try {
            size_t used = 0;
            const double parsed = std::stod(val, &used);
            if (used == val.size() && std::isfinite(parsed) && parsed > 0.0)
                amount = parsed;
        } catch (...) {}
    }
    return amount;
}

void MainWindow::syncCaptureControls()
{
    if (!m_uiReady) return;

    const bool unlimited = isUnlimitedDuration();
    const auto unit = durationUnitTag();
    const bool frames = (unit == "frames");
    const bool renderDocOn = RenderDocBox().IsChecked() &&
                             RenderDocBox().IsChecked().Value();
    const bool captureOn = CaptureBox().IsChecked() &&
                           CaptureBox().IsChecked().Value();

    if (unit == "frames")
        CaptureUnitLabel().Text(locText("frames", "帧"));
    else if (unit == "minutes")
        CaptureUnitLabel().Text(locText("min", "分钟"));
    else if (unit == "hours")
        CaptureUnitLabel().Text(locText("h", "小时"));
    else
        CaptureUnitLabel().Text(locText("s", "秒"));

    // Integer capture points for every unit — fractional seconds added noise
    // and the decimal formatter confused more than it helped.
    const double minVal = 1.0;
    double maxVal = unlimited ? minVal : std::floor(durationAmountValue() + 1e-9);
    if (!(maxVal >= minVal)) maxVal = minVal;

    using Windows::Globalization::NumberFormatting::DecimalFormatter;
    using Windows::Globalization::NumberFormatting::IncrementNumberRounder;
    using Windows::Globalization::NumberFormatting::RoundingAlgorithm;
    IncrementNumberRounder rounder;
    rounder.Increment(1.0);
    rounder.RoundingAlgorithm(RoundingAlgorithm::RoundHalfUp);
    DecimalFormatter formatter;
    formatter.IntegerDigits(1);
    formatter.FractionDigits(0);
    formatter.IsGrouped(false);
    formatter.NumberRounder(rounder);

    double cur = CaptureValueBox().Value();
    if (std::isnan(cur) || cur < minVal)
        cur = (std::min)(5.0, maxVal);
    if (cur > maxVal) cur = maxVal;
    cur = std::floor(cur + 1e-9);

    m_suppressRenderDocUi = true;
    CaptureValueBox().NumberFormatter(formatter);
    CaptureValueBox().Minimum(minVal);
    CaptureValueBox().Maximum(maxVal);
    CaptureValueBox().SmallChange(1.0);
    CaptureValueBox().LargeChange(frames ? 10.0 : 5.0);
    CaptureValueBox().Value(cur);

    const bool headlessOn = HeadlessBox().IsChecked() &&
                            HeadlessBox().IsChecked().Value();
    RenderDocBox().IsEnabled(!headlessOn);
    const bool canCapture = renderDocOn && !unlimited && !headlessOn;
    CaptureBox().IsEnabled(canCapture);
    CaptureValueBox().IsEnabled(canCapture && captureOn);
    m_suppressRenderDocUi = false;
}

void MainWindow::appendCaptureArgs(std::vector<std::string>& dest)
{
    if (isUnlimitedDuration()) return;
    // Headless runs never trigger captures in the engine — don't pass the args.
    if (HeadlessBox().IsChecked() && HeadlessBox().IsChecked().Value()) return;
    if (!(RenderDocBox().IsChecked() && RenderDocBox().IsChecked().Value())) return;
    if (!(CaptureBox().IsChecked() && CaptureBox().IsChecked().Value())) return;

    const auto unit = durationUnitTag();
    double amount = CaptureValueBox().Value();
    if (std::isnan(amount) || amount <= 0.0) return;

    const double maxAmount = durationAmountValue();
    if (amount > maxAmount) amount = maxAmount;

    if (unit == "frames")
    {
        const auto frame = static_cast<long long>((std::max)(1.0, std::floor(amount + 1e-9)));
        dest.push_back("--capture-frame");
        dest.push_back(std::to_string(frame));
        return;
    }

    double seconds = amount;
    if (unit == "minutes") seconds = amount * 60.0;
    else if (unit == "hours") seconds = amount * 3600.0;

    std::string secText;
    if (std::floor(seconds) == seconds)
        secText = std::to_string(static_cast<long long>(seconds));
    else
    {
        std::ostringstream oss;
        oss << std::setprecision(6) << std::defaultfloat << seconds;
        secText = oss.str();
    }
    dest.push_back("--capture");
    dest.push_back(secText);
}

void MainWindow::refreshAboutVersion()
{
    const auto ver = readGuiProductVersion();
    if (ver.empty())
    {
        AboutVersion().Text(locText("Version unknown", "版本未知"));
        return;
    }
    AboutVersion().Text(locText("Version ", "版本 ") + hstring(ver));
}

// ---- run -------------------------------------------------------------------
std::string MainWindow::particleValue()
{
    auto p = selected(ParticlePresetBox());
    if (p == "custom") return to_string(CustomParticleBox().Text());
    return p;                            // preset particle count
}

// Duration as engine args. Time units convert to --time <seconds>; Frames uses
// --benchmark; Until Cancel uses --no-time-limit (stop via Cancel).
std::vector<std::string> MainWindow::durationArgs()
{
    const auto unit = durationUnitTag();
    if (unit == "unlimited")
        return { "--no-time-limit" };

    auto val = to_string(DurationValueBox().Text());
    if (unit == "frames")
    {
        if (val.empty()) val = "600";
        return { "--benchmark", val };
    }

    double amount = 15.0;
    if (!val.empty())
    {
        try {
            size_t used = 0;
            amount = std::stod(val, &used);
            if (used != val.size() || !std::isfinite(amount) || amount < 0.0)
                amount = 15.0;
        } catch (...) {
            amount = 15.0;
        }
    }
    else if (unit == "minutes") amount = 5.0;
    else if (unit == "hours") amount = 1.0;
    else amount = 15.0;

    double seconds = amount;
    if (unit == "minutes") seconds = amount * 60.0;
    else if (unit == "hours") seconds = amount * 3600.0;

    // Keep CLI arg tidy: whole seconds when possible.
    std::string secText;
    if (std::floor(seconds) == seconds)
        secText = std::to_string(static_cast<long long>(seconds));
    else
    {
        std::ostringstream oss;
        oss << std::setprecision(6) << std::defaultfloat << seconds;
        secText = oss.str();
    }
    return { "--time", secText };
}

// Build child-process CLI invocation(s) for the selected preset.
std::vector<std::vector<std::string>> MainWindow::buildPresetJobs(bool& needCharts)
{
    needCharts = false;
    int p = PresetBox().SelectedIndex();

    std::vector<std::string> dur = durationArgs();   // --time <s> or --benchmark <frames>
    std::string extra = to_string(ExtraBox().Text());
    std::string particles = particleValue();

    // Arguments shared by the workload-aware Full Analysis presets. Keep the
    // mapping identical to Custom Run so a selected GPU Burn really reaches
    // every scheduled API/device instead of silently falling back to Stream.
    auto selectedWorkloadArgs = [&]() {
        std::vector<std::string> ex;
        const auto wl = selected(WorkloadBox());
        if (wl != "stream") { ex.push_back("--workload"); ex.push_back(wl); }
        if (wl == "synthpeak") {
            ex.push_back("--precision"); ex.push_back(selected(PrecisionBox()));
        }
        if ((wl == "stream" || wl == "render3d") && !particles.empty()) {
            ex.push_back("--particles"); ex.push_back(particles);
        } else if (!extra.empty()) {
            std::string flag = (wl == "nbody") ? "--bodies"
                             : (wl == "gpu_burn" || wl == "gpu_stress" || wl == "stress" || wl == "synthpeak") ? "--iter"
                             : (wl == "volumetric") ? "--steps"
                             : (wl == "fluid") ? "--grid" : "--particles";
            ex.push_back(flag); ex.push_back(extra);
        }
        if (wl == "fluid") {
            auto jacobi = to_string(FluidJacobiBox().Text());
            if (!jacobi.empty()) { ex.push_back("--jacobi"); ex.push_back(jacobi); }
        }
        if (VsyncBox().IsChecked() && VsyncBox().IsChecked().Value())
            ex.push_back("--vsync");
        if (HostMemBox().IsChecked() && HostMemBox().IsChecked().Value())
            ex.push_back("--host-memory");
        return ex;
    };

    const int gpuRow = GpuBox().SelectedIndex();   // 0 = automatic adapter
    int selectedGpu = -1;
    if (gpuRow > 0 && static_cast<size_t>(gpuRow - 1) < m_gpuIndices.size())
        selectedGpu = m_gpuIndices[gpuRow - 1];

    // Each API×GPU combination gets a fresh process. Unsupported choices are
    // deliberately retained: the CLI reports a precise capability error and
    // the remaining jobs continue.
    auto makeJobs = [&](std::vector<std::string> extraArgs, bool everyGpu) {
        std::vector<std::vector<std::string>> jobs;
        std::vector<int> targets;
        if (everyGpu)
        {
            targets = m_gpuIndices;
            if (targets.empty()) targets.push_back(-1);
        }
        else
        {
            targets.push_back(selectedGpu);
        }

        for (int gpu : targets)
            for (auto const& api : selectedApis())
            {
            std::vector<std::string> a = { m_enginePath, "--gui-worker" };
            for (auto& d : dur) a.push_back(d);
            a.push_back("--backend"); a.push_back(api);
            if (gpu >= 0) { a.push_back("--gpu"); a.push_back(std::to_string(gpu)); }
            for (auto& e : extraArgs) a.push_back(e);
            jobs.push_back(std::move(a));
            }
        return jobs;
    };

    auto appendCapture = [this](std::vector<std::string>& dest)
    {
        appendCaptureArgs(dest);
    };

    switch (p)
    {
        case 0:  // [0] Quick run — best API / GPU, Medium, stream
        {
            std::vector<std::string> a = { m_enginePath, "--gui-worker" };
            for (auto& d : dur) a.push_back(d);
            appendCapture(a);
            return { a };
        }

        case 1:  // [1] Custom run — honour every visible control
        {
            auto ex = selectedWorkloadArgs();
            const bool headless = HeadlessBox().IsChecked() &&
                                  HeadlessBox().IsChecked().Value();
            if (headless)
                ex.push_back("--headless");
            else
                appendCapture(ex);
            return makeJobs(std::move(ex), false);
        }

        case 2:  // [5] Full analysis, one GPU × selected APIs
        {
            needCharts = true;
            auto ex = selectedWorkloadArgs();
            appendCapture(ex);
            return makeJobs(std::move(ex), false);
        }

        case 3:  // [6] Selected workload, all GPUs × selected APIs
        {
            needCharts = true;
            auto ex = selectedWorkloadArgs();
            appendCapture(ex);
            return makeJobs(std::move(ex), true);
        }
        case 4:  // [7] Flights test, one GPU × selected APIs
        {
            std::vector<std::string> ex;
            if (!extra.empty()) { ex.push_back("--flights"); ex.push_back(extra); }
            appendCapture(ex);
            return makeJobs(std::move(ex), false);
        }
        case 5:  // [8] Particle test, one GPU × selected APIs
        {
            std::vector<std::string> ex;
            if (!particles.empty()) { ex.push_back("--particles"); ex.push_back(particles); }
            appendCapture(ex);
            return makeJobs(std::move(ex), false);
        }
        case 6:  // [9] Headless compute, one GPU × selected APIs
            return makeJobs({ "--headless" }, false);
    }
    return {};
}

void MainWindow::launchJobs(std::vector<std::vector<std::string>> jobs, bool needCharts)
{
    if (!tryBeginTask(ActiveTask::GpuBenchmark))
    {
        setGpuStatus(StatusLight::Error, locText(
            "Another benchmark or report task is already running.",
            "另一个测试或报告任务正在运行。"));
        return;
    }
    m_gpuCancelRequested.store(false);
    auto cancelEvent = m_gpuCancelEvent; // keep alive for the worker lifetime
    if (cancelEvent) ::ResetEvent(static_cast<HANDLE>(cancelEvent.get()));
    Busy().IsActive(true);
    GpuCancelButton().IsEnabled(true);
    setGpuStatus(StatusLight::Running, jobs.size() > 1
        ? locText("Running… (multiple passes; render windows may appear)", "运行中…（多次；可能弹出渲染窗口）")
        : locText("Running… (a separate render window may appear)", "运行中…（可能弹出独立渲染窗口）"));
    m_lastScoreEn.clear();
    m_lastScoreCacheHint = false;
    ResultText().Text(L"—");
    updateResultHint();
    OutputBox().Text({});

    // Progress: workers report no live protocol, so estimate per-job duration
    // from the configured run length and advance a timer between job starts.
    {
        const auto unit = durationUnitTag();
        const double amount = durationAmountValue();
        double sec = 15.0; // frames / unknown units: rough estimate
        if (unit == "seconds")      sec = amount;
        else if (unit == "minutes") sec = amount * 60.0;
        else if (unit == "hours")   sec = amount * 3600.0;
        m_gpuProgressIndeterminate = isUnlimitedDuration();
        m_gpuProgressJobExpectedSec = sec + 6.0; // warmup + init + save overhead
        m_gpuProgressJobs = jobs.size();
        m_gpuProgressJobIndex = 0;
        m_gpuProgressApiLabel.clear();
        m_gpuProgressJobStart = std::chrono::steady_clock::now();
        GpuProgressBar().IsIndeterminate(m_gpuProgressIndeterminate);
        GpuProgressBar().Value(0.0);
        GpuProgressText().Text(m_gpuProgressIndeterminate ? L"—" : L"0%");
        GpuStageText().Text(locText("Starting…", "正在启动…"));
        if (!m_gpuProgressTimer)
        {
            m_gpuProgressTimer = DispatcherTimer{};
            m_gpuProgressTimer.Interval(std::chrono::milliseconds(250));
            m_gpuProgressTimer.Tick([this](auto&&, auto&&)
            {
                if (uiAlive()) updateGpuProgressTick();
            });
        }
        m_gpuProgressTimer.Start();
        setTaskbarProgress(true, 0.0, m_gpuProgressIndeterminate);
    }

    std::string repo = m_enginePath.empty() ? std::string{}
        : pathToUtf8(pathFromUtf8(m_enginePath)
            .parent_path().parent_path().parent_path());
    HANDLE cancelHandle = cancelEvent
        ? static_cast<HANDLE>(cancelEvent.get()) : nullptr;

    auto strong = get_strong();
    auto disp = m_dispatcher;
    std::thread([this, strong, disp, jobs, needCharts, repo, cancelEvent, cancelHandle]()
    {
      try
      {
        std::string all, lastScore;
        size_t failedJobs = 0;
        size_t succeededJobs = 0;
        bool cancelled = false;
        std::vector<std::string> caps;
        // Scores accumulate grouped by adapter: a "# <GPU name>" header line
        // opens each group (rendered as an eyebrow by renderResultScore), and
        // multi-worker runs label each score line with its API.
        auto jobArgValue = [](std::vector<std::string> const& job,
                              char const* key) -> std::string
        {
            for (size_t i = 1; i + 1 < job.size(); ++i)
                if (job[i] == key) return job[i + 1];
            return {};
        };
        std::string currentGpuGroup;
        bool cacheResident = false;
        for (size_t jobIndex = 0; jobIndex < jobs.size(); ++jobIndex)
        {
            if (m_gpuCancelRequested.load())
            {
                cancelled = true;
                all += "\n[GUI] Remaining workers skipped after cancel.\n";
                break;
            }

            auto const& job = jobs[jobIndex];
            all += "\n========== Worker " + std::to_string(jobIndex + 1) +
                   "/" + std::to_string(jobs.size()) + " ==========";
            for (size_t i = 1; i < job.size(); ++i) all += " " + job[i];
            all += "\n";

            // Live progress in the status bar — headless runs have no window,
            // so this is the only sign that anything is happening.
            if (disp)
            {
                std::string apiLabel = jobArgValue(job, "--backend");
                for (auto const& api : kRunApis)
                    if (apiLabel == api.token) { apiLabel = api.label; break; }
                disp.TryEnqueue([this, strong, apiLabel, jobIndex]()
                {
                    if (!uiAlive()) return;
                    m_gpuProgressApiLabel = apiLabel;
                    m_gpuProgressJobIndex = jobIndex;
                    m_gpuProgressJobStart = std::chrono::steady_clock::now();
                    const auto text = gpuRunningStatusText();
                    setGpuStatus(StatusLight::Running, text);
                    GpuStageText().Text(text);
                });
            }

            auto res = captureCliProcess(job, gpuWorkerTimeoutMs(job), cancelHandle);
            all += res.output; all += "\n";
            if (all.size() > 4u * 1024u * 1024u)
                all = clipForUi(std::move(all), 3u * 1024u * 1024u);
            if (res.exitCode == static_cast<int>(ERROR_CANCELLED) ||
                m_gpuCancelRequested.load())
            {
                cancelled = true;
                all += "[GUI] Benchmark cancelled.\n";
                break;
            }
            if (res.exitCode != 0)
            {
                ++failedJobs;
                std::ostringstream code;
                code << "[GUI worker] Exited with code 0x" << std::hex
                     << static_cast<std::uint32_t>(res.exitCode) << std::dec << ".\n";
                all += code.str();
            }
            else
            {
                ++succeededJobs;
                auto workerCaps = parseCapturePaths(res.output);
                caps.insert(caps.end(), workerCaps.begin(), workerCaps.end());
                std::string sc = extractScore(res.output);
                if (!sc.empty())
                {
                    // Small VRAM working sets can sit entirely in a big GPU L2
                    // (e.g. 96 MB on GB202), inflating the apparent bandwidth.
                    if (res.output.find("VRAM rate:") != std::string::npos)
                    {
                        const double ws = extractWorkingSetMiB(res.output);
                        if (ws > 0.0 && ws < 128.0) cacheResident = true;
                    }
                    if (jobs.size() > 1)
                    {
                        std::string label = jobArgValue(job, "--backend");
                        for (auto const& api : kRunApis)
                            if (label == api.token) { label = api.label; break; }
                        sc = label + " — " + sc;
                    }
                    std::string gpuName = normalizeGpuName(extractGpuName(res.output));
                    if (gpuName.empty() || gpuName == "(unknown)")
                    {
                        const auto g = jobArgValue(job, "--gpu");
                        gpuName = g.empty() ? "GPU" : "GPU " + g;
                    }
                    if (gpuName != currentGpuGroup)
                    {
                        currentGpuGroup = gpuName;
                        if (!lastScore.empty()) lastScore += '\n';
                        lastScore += "# " + gpuName;
                    }
                    lastScore += '\n';
                    lastScore += sc;
                }
            }
        }

        bool postProcessFailed = false;
        bool renderDocConversionFailed = false;
        std::string postProcessStatus;
        auto recordPostProcessFailure = [&](std::string const& message)
        {
            postProcessFailed = true;
            if (!postProcessStatus.empty()) postProcessStatus += " ";
            postProcessStatus += message;
            all += "[Post-processing] " + message + "\n";
        };

        // Post-processing mirrors the CLI Full-Analysis / Flights / Particle
        // paths, but all optional-tool failures are reported truthfully.
        if (!cancelled && !repo.empty())
        {
            const auto repoPath = pathFromUtf8(repo);
            const auto repoW = repoPath.wstring();
            const std::string enginePath = jobs.empty() || jobs.front().empty()
                ? std::string{} : jobs.front().front();

            // Convert captures with bundled RenderDoc first. A conventional
            // developer-machine installation remains a compatibility fallback.
            if (!caps.empty())
            {
                const auto rdccmd = findRenderDocCommand(enginePath);
                if (rdccmd.empty())
                {
                    renderDocConversionFailed = true;
                    recordPostProcessFailure(
                        "RenderDoc capture conversion unavailable: renderdoccmd.exe was not found.");
                }
                else
                {
                    std::size_t conversionIndex = 0;
                    for (auto const& rdc : caps)
                    {
                        std::filesystem::path rdcP = pathFromUtf8(rdc);
                        std::filesystem::path jsonOut = rdcP;
                        jsonOut.replace_extension(L".json");

                        std::error_code fileError;
                        const bool validCapture =
                            std::filesystem::is_regular_file(rdcP, fileError) &&
                            !fileError && std::filesystem::file_size(rdcP, fileError) > 0 &&
                            !fileError;
                        if (!validCapture)
                        {
                            renderDocConversionFailed = true;
                            recordPostProcessFailure(
                                "RenderDoc capture is missing or empty: " +
                                pathToUtf8(rdcP.filename()) + ".");
                            ++conversionIndex;
                            continue;
                        }

                        std::filesystem::path tempJson = jsonOut.parent_path() /
                            (jsonOut.stem().wstring() + L".tmp-" +
                             std::to_wstring(::GetCurrentProcessId()) + L"-" +
                             std::to_wstring(conversionIndex++) + L".json");
                        if (m_gpuCancelRequested.load())
                        {
                            cancelled = true;
                            break;
                        }
                        auto converted = runProcess(
                            L"\"" + rdccmd.wstring() + L"\" convert -f \""
                            + rdcP.wstring() + L"\" -c chrome.json -o \""
                            + tempJson.wstring() + L"\"", repoW,
                            5u * 60u * 1000u, cancelHandle);
                        if (converted.exitCode == static_cast<int>(ERROR_CANCELLED) ||
                            m_gpuCancelRequested.load())
                        {
                            cancelled = true;
                            if (!converted.output.empty())
                            {
                                all += "[RenderDoc converter: " +
                                       pathToUtf8(rdcP.filename()) + "]\n";
                                all += converted.output;
                                if (all.empty() || all.back() != '\n') all += '\n';
                            }
                            break;
                        }
                        if (!converted.output.empty())
                        {
                            all += "[RenderDoc converter: " +
                                   pathToUtf8(rdcP.filename()) + "]\n";
                            all += converted.output;
                            if (all.empty() || all.back() != '\n') all += '\n';
                        }

                        bool convertedFileValid = false;
                        fileError.clear();
                        if (converted.exitCode == 0)
                            convertedFileValid =
                                std::filesystem::is_regular_file(tempJson, fileError) &&
                                !fileError &&
                                std::filesystem::file_size(tempJson, fileError) > 0 &&
                                !fileError;

                        bool committed = false;
                        if (convertedFileValid)
                            committed = ::MoveFileExW(
                                tempJson.c_str(), jsonOut.c_str(),
                                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;

                        if (converted.exitCode != 0 || !convertedFileValid || !committed)
                        {
                            renderDocConversionFailed = true;
                            std::ostringstream failure;
                            if (converted.exitCode == -2)
                                failure << "RenderDoc capture conversion timed out.";
                            else if (converted.exitCode != 0)
                                failure << "RenderDoc capture conversion failed (exit "
                                        << converted.exitCode << ", 0x" << std::hex
                                        << static_cast<std::uint32_t>(converted.exitCode)
                                        << std::dec << ").";
                            else if (!convertedFileValid)
                                failure << "RenderDoc conversion produced no valid JSON output.";
                            else
                                failure << "RenderDoc JSON could not be committed (Win32 "
                                        << ::GetLastError() << ").";
                            recordPostProcessFailure(
                                failure.str());
                            fileError.clear();
                            std::filesystem::remove(tempJson, fileError);
                        }
                    }
                }
            }
            else if (needCharts)
            {
                recordPostProcessFailure(
                    "No RenderDoc capture was produced for Full Analysis.");
            }

            if (!cancelled && needCharts)
            {
                const auto python = findPythonExecutable();
                const auto scripts = repoPath / L"scripts";
                const std::filesystem::path requiredScripts[] = {
                    scripts / L"rdoc_analyse.py",
                    scripts / L"plot_results.py",
                    scripts / L"export_report.py",
                    scripts / L"plot_workloads.py",
                };
                bool scriptsAvailable = true;
                std::error_code ec;
                for (auto const& script : requiredScripts)
                {
                    if (!std::filesystem::is_regular_file(script, ec) || ec)
                    {
                        scriptsAvailable = false;
                        break;
                    }
                }

                if (python.empty() || !scriptsAvailable)
                {
                    recordPostProcessFailure(
                        "Benchmark completed, but automatic reports are unavailable; "
                        "this package does not yet include the frozen report worker.");
                }
                else
                {
                    const auto resultsPath =
                        pathFromUtf8(gpu_bench::ResultsFilePath());
                    const auto dataRoot = resultsPath.parent_path().parent_path();
                    const auto reportsDir = dataRoot / L"reports";
                    const auto imagesDir = reportsDir / L"images";
                    std::filesystem::create_directories(imagesDir, ec);
                    if (ec)
                    {
                        recordPostProcessFailure(
                            "Could not create the per-user reports directory.");
                    }
                    else
                    {
                        const auto py = L"\"" + python.wstring() + L"\" ";
                        auto runReport = [&](std::wstring const& command,
                                             char const* label) -> bool
                        {
                            if (m_gpuCancelRequested.load())
                            {
                                cancelled = true;
                                return false;
                            }
                            auto report = runProcess(command, repoW,
                                                     10u * 60u * 1000u, cancelHandle);
                            if (!report.output.empty())
                            {
                                all += std::string("[") + label + "]\n" + report.output;
                                if (all.empty() || all.back() != '\n') all += '\n';
                            }
                            if (report.exitCode == static_cast<int>(ERROR_CANCELLED) ||
                                m_gpuCancelRequested.load())
                            {
                                cancelled = true;
                                return false;
                            }
                            if (report.exitCode != 0)
                                recordPostProcessFailure(
                                    std::string(label) + " failed (exit "
                                    + std::to_string(report.exitCode) + ").");
                            return true;
                        };

                        bool keepReporting = true;
                        if (!caps.empty() && !renderDocConversionFailed)
                        {
                            const auto captureDir =
                                pathFromUtf8(caps.front()).parent_path();
                            keepReporting = runReport(
                                py + L"\"scripts\\rdoc_analyse.py\" --captures \""
                                + captureDir.wstring() + L"\" --results \""
                                + resultsPath.wstring() + L"\" --output \""
                                + (reportsDir / L"rdoc_comparison.md").wstring()
                                + L"\"", "RenderDoc timing analysis");
                        }
                        if (keepReporting)
                            keepReporting = runReport(
                                py + L"\"scripts\\plot_results.py\" --json \""
                                + resultsPath.wstring() + L"\" --save \""
                                + imagesDir.wstring() + L"\"", "Result chart generation");
                        if (keepReporting)
                            keepReporting = runReport(
                                py + L"\"scripts\\export_report.py\" --json \""
                                + resultsPath.wstring() + L"\" --md \""
                                + (reportsDir / L"results-table.md").wstring()
                                + L"\"", "Markdown report generation");
                        if (keepReporting)
                            keepReporting = runReport(
                                py + L"\"scripts\\export_report.py\" --json \""
                                + resultsPath.wstring() + L"\" --html \""
                                + (reportsDir / L"report.html").wstring()
                                + L"\"", "HTML report generation");
                        if (keepReporting)
                            runReport(
                                py + L"\"scripts\\plot_workloads.py\" --input \""
                                + resultsPath.wstring() + L"\" --out \""
                                + imagesDir.wstring() + L"\"", "Workload chart generation");
                    }
                }
            }
        }

        if (cancelled)
            all += "\n[GUI] Cancelled by user.\n";

        all = clipForUi(std::move(all));
        if (!disp || !disp.TryEnqueue([this, strong, all, lastScore, cacheResident,
                         needCharts, failedJobs,
                         succeededJobs, postProcessFailed, postProcessStatus,
                         cancelled]()
        {
            if (!uiAlive())
            {
                endTask(ActiveTask::GpuBenchmark);
                return;
            }
            try
            {
                OutputBox().Text(u8(all));
                GpuCancelButton().IsEnabled(false);
                m_lastScoreCacheHint = cacheResident;
                if (cancelled)
                    stopGpuProgress(locText("Cancelled", "已取消"), false);
                else if (failedJobs > 0 && succeededJobs == 0)
                    stopGpuProgress(locText("Failed", "运行失败"), false);
                else if (failedJobs > 0)
                    stopGpuProgress(locText("Completed with errors", "部分完成"), true);
                else
                    stopGpuProgress(locText("Done", "完成"), true);
                auto showScoreOr = [&](hstring const& fallback)
                {
                    if (lastScore.empty())
                    {
                        m_lastScoreEn.clear();
                        ResultText().Text(fallback);
                    }
                    else
                    {
                        m_lastScoreEn = lastScore;
                        renderResultScore();
                    }
                    updateResultHint();
                };
                if (cancelled)
                {
                    showScoreOr(locText("Cancelled", "已取消"));
                    setGpuStatus(StatusLight::Ready, locText("Cancelled", "已取消"));
                }
                else if (failedJobs > 0 && succeededJobs == 0)
                {
                    m_lastScoreEn.clear();
                    ResultText().Text(locText("Error — see output", "出错 —— 见输出"));
                    updateResultHint();
                    setGpuStatus(StatusLight::Error, locText("Failed", "运行失败"));
                }
                else if (failedJobs > 0)
                {
                    showScoreOr(locText("Completed with errors — see output",
                                        "部分完成 —— 请查看错误输出"));
                    const auto ok = std::to_string(succeededJobs);
                    const auto failed = std::to_string(failedJobs);
                    setGpuStatus(StatusLight::Error, i18n::currentLang() == i18n::Lang::Zh
                        ? u8("完成 " + ok + " 项，失败 " + failed + " 项")
                        : u8(ok + " completed; " + failed + " failed"));
                }
                else
                {
                    showScoreOr(locText("Done — see output / History", "完成 —— 见输出/历史"));
                    if (postProcessFailed)
                        setGpuStatus(StatusLight::Error, u8("Benchmark done; " + postProcessStatus));
                    else
                        setGpuStatus(StatusLight::Ready, needCharts
                            ? locText("Done (charts & report regenerated)", "完成（已重新生成图表与报告）")
                            : locText("Done", "完成"));
                }
                endTask(ActiveTask::GpuBenchmark);
                Busy().IsActive(false);
                try { refreshHistory(); }
                catch (winrt::hresult_error const& e)
                {
                    appendGuiCrashLog("refreshHistory", winrt::to_string(e.message()).c_str());
                }
                catch (...)
                {
                    appendGuiCrashLog("refreshHistory", "unknown");
                }
            }
            catch (winrt::hresult_error const& e)
            {
                appendGuiCrashLog("launchJobs.ui", winrt::to_string(e.message()).c_str());
                endTask(ActiveTask::GpuBenchmark);
                try { Busy().IsActive(false); GpuCancelButton().IsEnabled(false); } catch (...) {}
            }
            catch (...)
            {
                appendGuiCrashLog("launchJobs.ui", "unknown");
                endTask(ActiveTask::GpuBenchmark);
                try { Busy().IsActive(false); GpuCancelButton().IsEnabled(false); } catch (...) {}
            }
        }))
        {
            appendGuiCrashLog("launchJobs", "TryEnqueue failed");
            // Best-effort unlock if the UI queue is already gone.
            m_activeTask.store(ActiveTask::None);
        }
      }
      catch (std::exception const& ex)
      {
          std::string msg = ex.what();
          appendGuiCrashLog("launchJobs.worker", msg.c_str());
          if (!disp || !disp.TryEnqueue([this, strong, msg]()
          {
              if (!uiAlive()) { endTask(ActiveTask::GpuBenchmark); return; }
              try
              {
                  OutputBox().Text(u8(std::string("[GUI] Worker exception: ") + msg));
                  ResultText().Text(locText("Error — see output", "出错 —— 见输出"));
                  setGpuStatus(StatusLight::Error, locText("Failed", "运行失败"));
                  stopGpuProgress(locText("Failed", "运行失败"), false);
                  endTask(ActiveTask::GpuBenchmark);
                  Busy().IsActive(false);
                  GpuCancelButton().IsEnabled(false);
              }
              catch (...) { endTask(ActiveTask::GpuBenchmark); }
          }))
          {
              m_activeTask.store(ActiveTask::None);
          }
      }
      catch (...)
      {
          appendGuiCrashLog("launchJobs.worker", "unknown");
          if (!disp || !disp.TryEnqueue([this, strong]()
          {
              if (!uiAlive()) { endTask(ActiveTask::GpuBenchmark); return; }
              try
              {
                  OutputBox().Text(locText("[GUI] Worker failed with an unknown exception.",
                                           "[GUI] 工作线程发生未知异常。"));
                  ResultText().Text(locText("Error — see output", "出错 —— 见输出"));
                  setGpuStatus(StatusLight::Error, locText("Failed", "运行失败"));
                  stopGpuProgress(locText("Failed", "运行失败"), false);
                  endTask(ActiveTask::GpuBenchmark);
                  Busy().IsActive(false);
                  GpuCancelButton().IsEnabled(false);
              }
              catch (...) { endTask(ActiveTask::GpuBenchmark); }
          }))
          {
              m_activeTask.store(ActiveTask::None);
          }
      }
    }).detach();
}

void MainWindow::OnRun(IInspectable const&, RoutedEventArgs const&)
{
    if (!m_gpuEnumerationComplete)
    {
        setGpuStatus(StatusLight::Running, locText(
            "Still detecting GPUs / APIs…",
            "仍在检测 GPU / API…"));
        return;
    }
    if (m_enginePath.empty())
    {
        setGpuStatus(StatusLight::Error, locText(
            "Engine exe not found (build the CMake project first).",
            "未找到引擎（请先用 CMake 构建）。"));
        return;
    }
    const int preset = PresetBox().SelectedIndex();
    if (preset != 0 && selectedApis().empty())
    {
        // Mirror rebuildApiPicker's Vulkan-only test: for the fluid workloads the
        // API panel lists Vulkan or nothing, so the generic "unsupported APIs may
        // also be selected" wording would be wrong here.
        const auto wl = selected(WorkloadBox());
        auto wlItem = WorkloadBox().SelectedItem().try_as<ComboBoxItem>();
        const bool vulkanOnly =
            wl == "cinematic_liquid" || wl == "cinematic_liquid_v1" ||
            (wlItem && (wlItem == WorkloadCinematicLiquid() ||
                        wlItem == WorkloadCinematicLiquidV1()));
        bool vulkanDetected = false;
        for (auto const& capabilities : m_gpuApiSupport)
            vulkanDetected = vulkanDetected || capabilities[0]; // {vulkan,...}
        if (vulkanOnly && !vulkanDetected)
        {
            ResultText().Text(locText(
                "This fluid workload requires Vulkan, and no Vulkan support was "
                "detected on this device.",
                "该流体测试仅支持 Vulkan，当前设备未检测到 Vulkan 支持。"));
            setGpuStatus(StatusLight::Error,
                         locText("Vulkan not available.", "Vulkan 不可用。"));
        }
        else if (vulkanOnly)
        {
            ResultText().Text(locText(
                "This fluid workload runs on Vulkan only — select Vulkan in the "
                "Graphics API list.",
                "该流体测试仅支持 Vulkan，请在图形 API 中勾选 Vulkan。"));
            setGpuStatus(StatusLight::Error,
                         locText("No graphics API selected.", "尚未选择图形 API。"));
        }
        else
        {
            ResultText().Text(locText(
                "Select at least one graphics API. Unsupported APIs may also be selected.",
                "请至少选择一个图形 API；未报告支持的 API 也可以勾选。"));
            setGpuStatus(StatusLight::Error,
                         locText("No graphics API selected.", "尚未选择图形 API。"));
        }
        return;
    }
    const bool workloadSelectable = preset == 1 || preset == 2 || preset == 3;
    const auto workload = selected(WorkloadBox());
    const bool gpuBurnWorkload = workload == "gpu_burn";
    const auto burnIterText = to_string(ExtraBox().Text());
    if (workloadSelectable && gpuBurnWorkload && !burnIterText.empty())
    {
        bool valid = false;
        try {
            size_t used = 0;
            const int value = std::stoi(burnIterText, &used);
            valid = used == burnIterText.size() && value >= 16 && value <= 32;
        } catch (...) {}
        if (!valid)
        {
            ResultText().Text(locText(
                "GPU Burn fixed steps must be an integer from 16 to 32. Leave it empty for safe per-device auto-tuning.",
                "GPU Burn 固定步数必须是 16 到 32 的整数；留空会按设备安全自动标定。"));
            setGpuStatus(StatusLight::Error, locText("GPU Burn settings need attention.",
                                                     "请检查 GPU Burn 设置。"));
            return;
        }
    }
    if (workloadSelectable && gpuBurnWorkload &&
        !isUnlimitedDuration() &&
        durationUnitTag() == "frames" &&
        burnIterText.empty())
    {
        ResultText().Text(locText(
            "Auto-tuned GPU Burn requires a timed run. Select Seconds/Minutes/Hours, or enter a fixed step count for Frames.",
            "自动标定的 GPU Burn 需要按时间运行。请选择秒/分钟/小时，或为按帧运行填写固定步数。"));
        setGpuStatus(StatusLight::Error, locText("GPU Burn settings need attention.",
                                                 "请检查 GPU Burn 设置。"));
        return;
    }
    bool needCharts = false;
    auto jobs = buildPresetJobs(needCharts);
    if (jobs.empty())
    {
        setGpuStatus(StatusLight::Error, locText("No benchmark jobs were generated.", "没有生成任何测试任务。"));
        return;
    }
    launchJobs(std::move(jobs), needCharts);
}

void MainWindow::OnPresetChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady) return;
    updateExtraLabel();
    if (m_gpuEnumerationComplete) rebuildApiPicker(true);
}

// ---- history (via gpu_bench::LoadResults, in-process) ----------------------
void MainWindow::syncHistoryCategoryFromUi()
{
    auto selected = HistoryCategoryTabs().SelectedItem();
    m_historyCategory = (selected && selected == HistoryCpuTab())
        ? HistoryCategory::Cpu : HistoryCategory::Gpu;
    m_showLegacyHistory = HistoryLegacyBox().IsChecked().Value();
    m_showHeadlessHistory = HistoryHeadlessBox().IsChecked().Value();
}

void MainWindow::updateHistoryFilterVisibility()
{
    const bool cpu = m_historyCategory == HistoryCategory::Cpu;
    ApiFilterColumn().Visibility(cpu ? Visibility::Collapsed : Visibility::Visible);
    HistoryLegacyBox().Visibility(cpu ? Visibility::Collapsed : Visibility::Visible);
    HistoryHeadlessBox().Visibility(cpu ? Visibility::Collapsed : Visibility::Visible);

    bool showParticles = false;
    bool showSteps = false;
    if (!cpu)
    {
        showParticles = historyParamFilterNeeded(
            WorkloadFilterPanel(),
            [](std::string const& key) { return workloadUsesParticles(key); },
            [this]()
            {
                for (auto const& r : m_results)
                {
                    if (isCpuHistoryResult(r)) continue;
                    if (r.particleCount > 0 && workloadUsesParticles(historyWorkloadKey(r)))
                        return true;
                }
                return false;
            });
        showSteps = historyParamFilterNeeded(
            WorkloadFilterPanel(),
            [](std::string const& key) { return workloadUsesBurnSteps(key); },
            [this]()
            {
                for (auto const& r : m_results)
                {
                    if (isCpuHistoryResult(r)) continue;
                    if (!burnStepsKey(r).empty())
                        return true;
                }
                return false;
            });
    }
    ParticleFilterColumn().Visibility(showParticles ? Visibility::Visible : Visibility::Collapsed);
    StepsFilterColumn().Visibility(showSteps ? Visibility::Visible : Visibility::Collapsed);

    if (cpu)
        GpuFilterLabel().Text(locText("CPUs", "处理器"));
    else
        GpuFilterLabel().Text(locText("GPUs", "显卡"));
}

void MainWindow::OnHistoryCategoryChanged(
    SelectorBar const&,
    SelectorBarSelectionChangedEventArgs const&)
{
    if (!m_uiReady) return;
    const auto previous = m_historyCategory;
    syncHistoryCategoryFromUi();
    if (previous == m_historyCategory) return;
    // Fresh defaults for the newly selected category (avoid cross-category residue).
    rebuildGpuFilter(false);
    rebuildHistoryFilters(false);
    updateHistoryFilterVisibility();
    applyHistoryView();
}

void MainWindow::OnHistoryLegacyChecked(IInspectable const&, RoutedEventArgs const&)
{
    if (!m_uiReady) return;
    syncHistoryCategoryFromUi();
    rebuildHistoryFilters(true);
    updateHistoryFilterVisibility();
    applyHistoryView();
}

void MainWindow::refreshHistory()
{
    if (!uiAlive()) return;
    m_results = gpu_bench::LoadResults();
    syncHistoryCategoryFromUi();
    rebuildGpuFilter(true);
    rebuildHistoryFilters(true);
    m_historyFiltersInitialized = true;
    updateHistoryFilterVisibility();
    applyHistoryView();
}

void MainWindow::rebuildHistoryFilters(bool preserveSelection)
{
    ApiFilterLabel().Text(locText("Graphics API", "图形 API"));
    WorkloadFilterLabel().Text(locText("Workload", "测试项目"));
    ParticleFilterLabel().Text(locText("Particles", "粒子数"));
    StepsFilterLabel().Text(locText("Steps", "步数"));
    SelectAllApis().Content(locContent("All", "全选"));
    ClearAllApis().Content(locContent("None", "清空"));
    SelectAllWorkloads().Content(locContent("All", "全选"));
    ClearAllWorkloads().Content(locContent("None", "清空"));
    SelectAllParticles().Content(locContent("All", "全选"));
    ClearAllParticles().Content(locContent("None", "清空"));
    SelectAllSteps().Content(locContent("All", "全选"));
    ClearAllSteps().Content(locContent("None", "清空"));
    SelectAllGpus().Content(locContent("All", "全选"));
    ClearAllGpus().Content(locContent("None", "清空"));

    auto prevApis = checkedTags(ApiFilterPanel());
    auto prevWorkloads = checkedTags(WorkloadFilterPanel());
    auto prevParticles = checkedTags(ParticleFilterPanel());
    auto prevSteps = checkedTags(StepsFilterPanel());
    ApiFilterPanel().Children().Clear();
    WorkloadFilterPanel().Children().Clear();
    ParticleFilterPanel().Children().Clear();
    StepsFilterPanel().Children().Clear();

    const bool cpuCat = m_historyCategory == HistoryCategory::Cpu;
    std::map<std::string, int> apiCounts;
    std::map<std::string, int> workloadCounts;
    std::map<std::uint32_t, int> particleCounts;
    std::map<std::string, int> stepCounts;
    for (auto const& r : m_results)
    {
        if (isCpuHistoryResult(r) != cpuCat) continue;
        if (!cpuCat && !r.graphicsApi.empty() && r.graphicsApi != "CPU")
            ++apiCounts[r.graphicsApi];
        auto historyKey = historyWorkloadKey(r);
        if (historyKey.empty()) continue;
        if (cpuCat)
        {
            if (historyKey.rfind("cpu_", 0) == 0)
                ++workloadCounts[historyKey];
        }
        else
        {
            if (historyKey.rfind("cpu_", 0) == 0) continue;
            if (!m_showLegacyHistory && isLegacyGpuWorkload(historyKey)) continue;
            if (!m_showHeadlessHistory && r.headless) continue;
            ++workloadCounts[historyKey];
            if (r.particleCount > 0 && workloadUsesParticles(historyKey))
                ++particleCounts[r.particleCount];
            if (auto steps = burnStepsKey(r); !steps.empty())
                ++stepCounts[steps];
        }
    }

    auto addStringFilters = [&](StackPanel const& panel,
                                std::map<std::string, int> const& counts,
                                std::set<std::string> const& previous,
                                std::vector<std::string> const& preferred,
                                auto labelOf)
    {
        std::string defaultKey = mostFrequentKey(counts);
        std::set<std::string> emitted;
        const bool usePrevious = preserveSelection && m_historyFiltersInitialized && !previous.empty();
        auto emit = [&](std::string const& key)
        {
            auto it = counts.find(key);
            if (it == counts.end()) return;
            bool checked = usePrevious ? previous.find(key) != previous.end()
                                       : key == defaultKey;
            appendFilterCheckBox(panel, labelOf(key), key, checked);
            emitted.insert(key);
        };
        for (auto const& key : preferred) emit(key);
        for (auto const& [key, count] : counts)
            if (emitted.find(key) == emitted.end()) emit(key);
    };

    if (!cpuCat)
    {
        addStringFilters(ApiFilterPanel(), apiCounts, prevApis,
                         { "Vulkan", "DX12", "DX11", "OpenGL" },
                         [](std::string const& key) { return apiLabel(key); });
        std::vector<std::string> preferred = { "stream", "gpu_burn", "cinematic_liquid" };
        if (m_showLegacyHistory)
        {
            for (auto const* id : { "gpu_stress", "nbody", "synthpeak", "stress", "render3d",
                                    "volumetric", "fluid", "cinematic_liquid_v1" })
                preferred.push_back(id);
        }
        addStringFilters(WorkloadFilterPanel(), workloadCounts, prevWorkloads, preferred,
                         [](std::string const& key) { return workloadLabel(key); });

        std::map<std::string, int> particleStringCounts;
        for (auto const& [n, count] : particleCounts)
            particleStringCounts[std::to_string(n)] = count;
        const std::string defaultParticle = mostFrequentKey(particleStringCounts);
        const bool useParticlePrevious =
            preserveSelection && m_historyFiltersInitialized && !prevParticles.empty();
        for (auto const& [n, count] : particleCounts)
        {
            std::string key = std::to_string(n);
            bool checked = useParticlePrevious
                ? prevParticles.find(key) != prevParticles.end()
                : key == defaultParticle;
            appendFilterCheckBox(ParticleFilterPanel(), particleLabel(n), key, checked);
        }

        std::vector<std::string> stepKeys;
        stepKeys.reserve(stepCounts.size());
        for (auto const& [key, count] : stepCounts) stepKeys.push_back(key);
        std::sort(stepKeys.begin(), stepKeys.end(), [](std::string const& a, std::string const& b)
        {
            try { return std::stoll(a) < std::stoll(b); }
            catch (...) { return a < b; }
        });
        const std::string defaultStep = mostFrequentKey(stepCounts);
        const bool useStepPrevious =
            preserveSelection && m_historyFiltersInitialized && !prevSteps.empty();
        for (auto const& key : stepKeys)
        {
            bool checked = useStepPrevious
                ? prevSteps.find(key) != prevSteps.end()
                : key == defaultStep;
            appendFilterCheckBox(StepsFilterPanel(), burnStepsLabel(key), key, checked);
        }
    }
    else
    {
        addStringFilters(WorkloadFilterPanel(), workloadCounts, prevWorkloads,
                         { "cpu_single_core", "cpu_multi_core" },
                         [](std::string const& key) { return workloadLabel(key); });
    }
}

void MainWindow::rebuildGpuFilter(bool preserveSelection)
{
    using winrt::Windows::UI::Text::FontWeights;
    auto previous = checkedTags(GpuFilterPanel());
    GpuFilterPanel().Children().Clear();
    const bool usePrevious = preserveSelection && m_historyFiltersInitialized && !previous.empty();

    if (m_historyCategory == HistoryCategory::Cpu)
    {
        std::map<std::string, int> counts;
        for (auto const& r : m_results)
        {
            if (!isCpuHistoryResult(r)) continue;
            ++counts[cpuFilterKey(r)];
        }
        std::string defaultKey = mostFrequentKey(counts);
        for (auto const& [name, count] : counts)
        {
            bool checked = usePrevious ? previous.find(name) != previous.end()
                                       : name == defaultKey;
            appendFilterCheckBox(GpuFilterPanel(), name, name, checked);
        }
        return;
    }

    std::map<std::string, std::map<std::string, std::set<std::string>>> groups;
    std::map<std::string, int> counts;
    for (auto const& r : m_results)
    {
        if (isCpuHistoryResult(r)) continue;
        GpuLeaf l = leafOf(r);
        std::string key = filterKey(l);
        groups[l.brand][l.series].insert(l.device);
        ++counts[key];
    }
    std::string defaultKey = mostFrequentKey(counts);

    for (auto const& [brand, seriesMap] : groups)
    {
        TextBlock bt; bt.Text(u8(brand));
        bt.FontWeight(FontWeights::SemiBold());
        bt.Margin(Thickness{ 0, 8, 0, 2 });
        GpuFilterPanel().Children().Append(bt);

        for (auto const& [series, devs] : seriesMap)
        {
            TextBlock st; st.Text(u8(series));
            st.Opacity(0.7); st.FontSize(12);
            st.Margin(Thickness{ 12, 2, 0, 0 });
            GpuFilterPanel().Children().Append(st);

            for (auto const& d : devs)
            {
                std::string key = filterKey({ brand, series, d });
                bool checked = usePrevious ? previous.find(key) != previous.end()
                                           : key == defaultKey;
                appendFilterCheckBox(GpuFilterPanel(), d, key, checked, Thickness{ 24, 0, 0, 0 });
            }
        }
    }
}

void MainWindow::applyHistoryView()
{
    {
        updateHistoryFilterVisibility();

        auto collect = [&](StackPanel const& panel,
                           DropDownButton const& button,
                           hstring const& allLabel,
                           hstring const& noneLabel)
        {
            std::set<std::string> allowed;
            int total = 0, checked = 0;
            hstring single;
            for (auto const& c : panel.Children())
                if (auto cb = c.try_as<CheckBox>(); cb && cb.Tag())
                {
                    ++total;
                    if (cb.IsChecked() && cb.IsChecked().Value())
                    {
                        ++checked;
                        allowed.insert(to_string(unbox_value_or<hstring>(cb.Tag(), L"")));
                        single = unbox_value_or<hstring>(cb.Content(), L"");
                    }
                }

            hstring label = (!total || checked == total) ? allLabel
                           : checked == 0               ? noneLabel
                           : checked == 1               ? single
                                                         : u8(std::to_string(checked) + " / " + std::to_string(total));
            button.Content(box_value(label));
            return allowed;
        };

        const bool cpuCat = m_historyCategory == HistoryCategory::Cpu;
        auto allowedApis = collect(ApiFilterPanel(), ApiFilterButton(),
                                   locText("All APIs", "全部 API"), locText("None", "无"));
        auto allowedWorkloads = collect(WorkloadFilterPanel(), WorkloadFilterButton(),
                                        locText("All workloads", "全部项目"), locText("None", "无"));
        auto allowedParticles = collect(ParticleFilterPanel(), ParticleFilterButton(),
                                        locText("All particle counts", "全部粒子数"), locText("None", "无"));
        auto allowedSteps = collect(StepsFilterPanel(), StepsFilterButton(),
                                    locText("All step counts", "全部步数"), locText("None", "无"));
        auto allowedDevices = collect(GpuFilterPanel(), GpuFilterButton(),
                                      cpuCat ? locText("All CPUs", "全部处理器")
                                             : locText("All GPUs", "全部 GPU"),
                                      locText("None", "无"));

        bool hasApiFilter = !cpuCat && ApiFilterPanel().Children().Size() > 0;
        bool hasWorkloadFilter = WorkloadFilterPanel().Children().Size() > 0;
        bool hasParticleFilter =
            !cpuCat &&
            ParticleFilterColumn().Visibility() == Visibility::Visible &&
            ParticleFilterPanel().Children().Size() > 0;
        bool hasStepsFilter =
            !cpuCat &&
            StepsFilterColumn().Visibility() == Visibility::Visible &&
            StepsFilterPanel().Children().Size() > 0;
        bool hasDeviceFilter = GpuFilterPanel().Children().Size() > 0;

        int rangeIdx = TimeRangeBox().SelectedIndex();
        std::string lo, hi;
        if (rangeIdx == 4) { lo = pickerDate(FromDate()); hi = pickerDate(ToDate()); }
        else               { lo = cutoffFor(rangeIdx); }

        std::vector<const gpu_bench::BenchmarkResult*> view;
        for (auto& r : m_results)
        {
            if (isCpuHistoryResult(r) != cpuCat) continue;
            const auto wlKey = historyWorkloadKey(r);
            if (!cpuCat && !m_showLegacyHistory &&
                isLegacyGpuWorkload(wlKey)) continue;
            if (!cpuCat && !m_showHeadlessHistory && r.headless) continue;
            if (hasApiFilter && allowedApis.find(r.graphicsApi) == allowedApis.end()) continue;
            if (hasWorkloadFilter && allowedWorkloads.find(wlKey) == allowedWorkloads.end()) continue;
            // Param filters only apply to workloads that use that dimension.
            if (hasParticleFilter && workloadUsesParticles(wlKey) &&
                allowedParticles.find(std::to_string(r.particleCount)) == allowedParticles.end())
                continue;
            if (hasStepsFilter && workloadUsesBurnSteps(wlKey) &&
                allowedSteps.find(burnStepsKey(r)) == allowedSteps.end())
                continue;
            if (hasDeviceFilter)
            {
                const auto deviceKey = cpuCat ? cpuFilterKey(r) : filterKey(leafOf(r));
                if (allowedDevices.find(deviceKey) == allowedDevices.end()) continue;
            }
            std::string date = resultDatePrefix(r.timestamp);
            if (!lo.empty() && date < lo) continue;
            if (!hi.empty() && date > hi) continue;
            view.push_back(&r);
        }

        auto cmpString = [](std::string const& a, std::string const& b)
        {
            if (a < b) return -1;
            if (a > b) return 1;
            return 0;
        };
        std::sort(view.begin(), view.end(),
            [&](const gpu_bench::BenchmarkResult* a, const gpu_bench::BenchmarkResult* b)
            {
                int c = 0;
                auto const& col = m_historySortColumn;
                if (col == "api")            c = cmpString(a->graphicsApi, b->graphicsApi);
                else if (col == "device")    c = cmpString(a->deviceName, b->deviceName);
                else if (col == "cpu")       c = cmpString(a->cpuName, b->cpuName);
                else if (col == "mem")       c = (a->vramMB < b->vramMB) ? -1 : (a->vramMB > b->vramMB) ? 1 : 0;
                else if (col == "workload")  c = cmpString(a->workload, b->workload);
                else if (col == "particles") c = (a->particleCount < b->particleCount) ? -1 : (a->particleCount > b->particleCount) ? 1 : 0;
                else if (col == "score")     c = (a->score < b->score) ? -1 : (a->score > b->score) ? 1 : 0;
                else if (col == "fps")       c = (a->avgFps < b->avgFps) ? -1 : (a->avgFps > b->avgFps) ? 1 : 0;
                else                          c = cmpString(a->timestamp, b->timestamp);
                if (c == 0) c = cmpString(a->timestamp, b->timestamp);
                return m_historySortAscending ? c < 0 : c > 0;
            });

        HistoryList().Items().Clear();
        HistoryHeaderPanel().Children().Clear();
        m_displayedIds.clear();

        const std::string gp = "  ";
        const std::string sentinel = "\xE2\x80\x8C";  // UTF-8 for U+200C

        auto addHeader = [&](std::string label, size_t width, char const* column)
        {
            if (m_historySortColumn == column)
                label += m_historySortAscending ? " ^" : " v";
            width = (std::max)(width, utf8DisplayWidth(label));
            TextBlock tb;
            tb.Text(u8(padDisplay(label, width) + gp + sentinel));
            tb.Tag(box_value(u8(column)));
            tb.FontFamily(Media::FontFamily(L"Consolas"));
            tb.Opacity(0.78);
            tb.VerticalAlignment(VerticalAlignment::Center);
            tb.Margin(Thickness{ 0, 0, 0, 2 });
            tb.Tapped([this](IInspectable const& sender, auto const&)
            {
                auto fe = sender.as<FrameworkElement>();
                std::string column = to_string(unbox_value_or<hstring>(fe.Tag(), L"time"));
                if (m_historySortColumn == column)
                {
                    m_historySortAscending = !m_historySortAscending;
                }
                else
                {
                    m_historySortColumn = column;
                    m_historySortAscending = !(column == "time" || column == "score" || column == "fps");
                }
                applyHistoryView();
            });
            HistoryHeaderPanel().Children().Append(tb);
        };

        // CPU history: no GPU/API/VRAM/Particles/FPS columns.
        if (cpuCat)
        {
            struct CpuRow { std::string time, cpu, wl, score; };
            std::vector<CpuRow> rows; rows.reserve(view.size());
            for (auto* r : view)
            {
                CpuRow x;
                x.time = localizeTimestamp(r->timestamp);
                x.cpu  = normalizeCpuName(r->cpuName.empty() ? r->deviceName : r->cpuName);
                x.wl   = workloadRunLabel(*r);
                x.score = r->score > 0.0
                    ? [&] { std::ostringstream o; o.setf(std::ios::fixed); o.precision(1);
                            o << r->score << ' ' << r->scoreUnit;
                            return o.str(); }()
                    : "-";
                rows.push_back(std::move(x));
            }

            size_t wTime = 4, wCpu = 3, wWl = 8, wScore = 5;
            for (auto& x : rows)
            {
                wTime  = (std::max)(wTime,  utf8DisplayWidth(x.time));
                wCpu   = (std::max)(wCpu,   utf8DisplayWidth(x.cpu));
                wWl    = (std::max)(wWl,    utf8DisplayWidth(x.wl));
                wScore = (std::max)(wScore, utf8DisplayWidth(x.score));
            }

            addHeader(to_string(locText("Time", "时间")), wTime, "time");
            addHeader(to_string(locText("CPU", "CPU")), wCpu, "cpu");
            addHeader(to_string(locText("Workload", "测试项目")), wWl, "workload");
            addHeader(to_string(locText("Score", "分数")), wScore, "score");

            for (size_t i = 0; i < rows.size(); ++i)
            {
                auto& x = rows[i];
                std::string line = padDisplay(x.time, wTime) + gp + padDisplay(x.cpu, wCpu)
                                 + gp + padDisplay(x.wl, wWl) + gp + padDisplay(x.score, wScore)
                                 + sentinel;
                TextBlock tb; tb.Text(u8(line));
                tb.FontFamily(Media::FontFamily(L"Consolas"));
                HistoryList().Items().Append(tb);
                m_displayedIds.push_back(view[i]->id);
            }
            return;
        }

        struct Row { std::string time, api, dev, cpu, mem, wl, particles, score, fps; };
        std::vector<Row> rows; rows.reserve(view.size());
        for (auto* r : view)
        {
            Row x;
            x.time = localizeTimestamp(r->timestamp);
            x.api  = r->graphicsApi;
            x.dev  = normalizeGpuName(r->deviceName);
            x.cpu  = normalizeCpuName(r->cpuName);
            x.mem  = formatVramMB(resolvedVramMB(*r, m_results));
            x.wl   = workloadRunLabel(*r);
            x.particles = particleLabel(r->particleCount);
            x.score = r->workload == "fluid"
                ? i18n::tr("Unverified legacy", "未验证旧版")
                : r->score > 0.0
                ? [&] { std::ostringstream o; o.setf(std::ios::fixed); o.precision(1);
                        o << r->score << ' ' << r->scoreUnit;
                        if (!r->precision.empty()) o << " (" << r->precision << ')';
                        return o.str(); }()
                : "-";
            x.fps  = std::to_string((int)r->avgFps);
            rows.push_back(std::move(x));
        }

        size_t wTime = 4, wApi = 3, wDev = 6, wCpu = 3, wMem = 4, wWl = 8, wParticles = 9, wScore = 5;
        for (auto& x : rows)
        {
            wTime  = (std::max)(wTime,  utf8DisplayWidth(x.time));
            wApi   = (std::max)(wApi,   utf8DisplayWidth(x.api));
            wDev   = (std::max)(wDev,   utf8DisplayWidth(x.dev));
            wCpu   = (std::max)(wCpu,   utf8DisplayWidth(x.cpu));
            wMem   = (std::max)(wMem,   utf8DisplayWidth(x.mem));
            wWl    = (std::max)(wWl,    utf8DisplayWidth(x.wl));
            wParticles = (std::max)(wParticles, utf8DisplayWidth(x.particles));
            wScore = (std::max)(wScore, utf8DisplayWidth(x.score));
        }

        addHeader(to_string(locText("Time", "时间")), wTime, "time");
        addHeader(to_string(locText("API", "API")), wApi, "api");
        addHeader(to_string(locText("GPU/Render", "GPU/渲染")), wDev, "device");
        addHeader(to_string(locText("CPU", "CPU")), wCpu, "cpu");
        addHeader(to_string(locText("VRAM", "显存")), wMem, "mem");
        addHeader(to_string(locText("Workload", "测试项目")), wWl, "workload");
        addHeader(to_string(locText("Particles", "粒子")), wParticles, "particles");
        addHeader(to_string(locText("Score", "分数")), wScore, "score");
        addHeader(to_string(locText("FPS", "FPS")), 3, "fps");

        for (size_t i = 0; i < rows.size(); ++i)
        {
            auto& x = rows[i];
            std::string line = padDisplay(x.time, wTime) + gp + padDisplay(x.api, wApi) + gp + padDisplay(x.dev, wDev)
                             + gp + padDisplay(x.cpu, wCpu) + gp + padDisplay(x.mem, wMem) + gp + padDisplay(x.wl, wWl)
                             + gp + padDisplay(x.particles, wParticles)
                             + gp + padDisplay(x.score, wScore) + gp + x.fps + sentinel;
            TextBlock tb; tb.Text(u8(line));
            tb.FontFamily(Media::FontFamily(L"Consolas"));
            HistoryList().Items().Append(tb);
            m_displayedIds.push_back(view[i]->id);
        }
        return;
    }

    // GPU filter: which device checkboxes are ticked.
    std::set<std::string> allowed;
    int total = 0, checked = 0;
    for (auto const& c : GpuFilterPanel().Children())
        if (auto cb = c.try_as<CheckBox>(); cb && cb.Tag())
        {
            ++total;
            if (cb.IsChecked() && cb.IsChecked().Value())
            { allowed.insert(to_string(unbox_value_or<hstring>(cb.Tag(), L""))); ++checked; }
        }
    bool anyBox = total > 0;
    GpuFilterButton().Content(box_value(
        (!anyBox || checked == total) ? locText("All GPUs", "全部 GPU")
        : (checked == 0)              ? locText("None", "无")
                                      : u8(std::to_string(checked) + " / " + std::to_string(total))));

    // Time range (index 4 = custom date pickers).
    int rangeIdx = TimeRangeBox().SelectedIndex();
    std::string lo, hi;
    if (rangeIdx == 4) { lo = pickerDate(FromDate()); hi = pickerDate(ToDate()); }
    else               { lo = cutoffFor(rangeIdx); }

    std::string workloadFilter = selected(WorkloadFilterBox());
    if (workloadFilter.empty()) workloadFilter = "*";
    std::uint32_t particleFilter = 0;
    try { particleFilter = static_cast<std::uint32_t>(std::stoul(selected(ParticleFilterBox()))); }
    catch (...) { particleFilter = 0; }

    std::vector<const gpu_bench::BenchmarkResult*> view;
    for (auto& r : m_results)
    {
        if (anyBox && allowed.find(filterKey(leafOf(r))) == allowed.end()) continue;
        if (workloadFilter != "*" && historyWorkloadKey(r) != workloadFilter) continue;
        if (particleFilter != 0 && r.particleCount != particleFilter) continue;
        std::string date = resultDatePrefix(r.timestamp);
        if (!lo.empty() && date < lo) continue;
        if (!hi.empty() && date > hi) continue;
        view.push_back(&r);
    }

    int s = SortBox().SelectedIndex();
    std::sort(view.begin(), view.end(),
        [s](const gpu_bench::BenchmarkResult* a, const gpu_bench::BenchmarkResult* b)
        {
            switch (s)
            {
                case 1:  return a->score > b->score;
                case 2:  return a->graphicsApi < b->graphicsApi;
                case 3:  return a->deviceName  < b->deviceName;
                case 4:  return a->workload    < b->workload;
                default: return a->timestamp   > b->timestamp;  // newest first
            }
        });

    HistoryList().Items().Clear();
    m_displayedIds.clear();

    // Flatten to strings first so columns can size to the widest value (no
    // truncation); the list scrolls horizontally if the total is wide.
    struct Row { std::string time, api, dev, cpu, mem, wl, particles, score, fps; };
    std::vector<Row> rows; rows.reserve(view.size());
    for (auto* r : view)
    {
        Row x;
        x.time = r->timestamp;
        x.api  = r->graphicsApi;
        x.dev  = r->deviceName;
        x.cpu  = r->cpuName;
        x.mem  = formatVramMB(r->vramMB);
        x.wl   = workloadRunLabel(*r);
        x.particles = particleLabel(r->particleCount);
        x.score = r->workload == "fluid"
            ? i18n::tr("Unverified legacy", "未验证旧版")
            : r->score > 0.0
            ? [&] { std::ostringstream o; o.setf(std::ios::fixed); o.precision(1);
                    o << r->score << ' ' << r->scoreUnit;
                    if (!r->precision.empty()) o << " (" << r->precision << ')';
                    return o.str(); }()
            : "-";
        x.fps  = std::to_string((int)r->avgFps);
        rows.push_back(std::move(x));
    }

    size_t wTime = 4, wApi = 3, wDev = 6, wCpu = 3, wMem = 4, wWl = 8, wParticles = 9, wScore = 5;
    for (auto& x : rows)
    {
        wTime  = (std::max)(wTime,  utf8DisplayWidth(x.time));
        wApi   = (std::max)(wApi,   utf8DisplayWidth(x.api));
        wDev   = (std::max)(wDev,   utf8DisplayWidth(x.dev));
        wCpu   = (std::max)(wCpu,   utf8DisplayWidth(x.cpu));
        wMem   = (std::max)(wMem,   utf8DisplayWidth(x.mem));
        wWl    = (std::max)(wWl,    utf8DisplayWidth(x.wl));
        wParticles = (std::max)(wParticles, utf8DisplayWidth(x.particles));
        wScore = (std::max)(wScore, utf8DisplayWidth(x.score));
    }
    const std::string gp = "  ";
    const std::string sentinel = "\xE2\x80\x8C";

    HistoryHeader().Text(u8(padDisplay("Time", wTime) + gp + padDisplay("API", wApi) + gp + padDisplay("Device", wDev)
                            + gp + padDisplay("CPU", wCpu) + gp + padDisplay("VRAM", wMem) + gp + padDisplay("Workload", wWl)
                            + gp + padDisplay("Particles", wParticles)
                            + gp + padDisplay("Score", wScore) + gp + "FPS" + sentinel));
    for (size_t i = 0; i < rows.size(); ++i)
    {
        auto& x = rows[i];
        std::string line = padDisplay(x.time, wTime) + gp + padDisplay(x.api, wApi) + gp + padDisplay(x.dev, wDev)
                         + gp + padDisplay(x.cpu, wCpu) + gp + padDisplay(x.mem, wMem) + gp + padDisplay(x.wl, wWl)
                         + gp + padDisplay(x.particles, wParticles)
                         + gp + padDisplay(x.score, wScore) + gp + x.fps + sentinel;
        TextBlock tb; tb.Text(u8(line));
        tb.FontFamily(Media::FontFamily(L"Consolas"));
        HistoryList().Items().Append(tb);
        m_displayedIds.push_back(view[i]->id);
    }
}

void MainWindow::OnRefreshHistory(IInspectable const&, RoutedEventArgs const&) { refreshHistory(); }

void MainWindow::OnHistoryViewChanged(IInspectable const&, SelectionChangedEventArgs const&)
{
    if (!m_uiReady) return;
    bool custom = TimeRangeBox().SelectedIndex() == 4;
    DateRangeRow().Visibility(custom ? Visibility::Visible : Visibility::Collapsed);
    applyHistoryView();
}

void MainWindow::OnDateRangeChanged(Controls::CalendarDatePicker const&,
                                    Controls::CalendarDatePickerDateChangedEventArgs const&)
{
    if (m_uiReady) applyHistoryView();
}

void MainWindow::OnDeleteSelected(IInspectable const&, RoutedEventArgs const&)
{
    auto sel = HistoryList().SelectedItems();
    if (sel.Size() == 0) return;
    auto items = HistoryList().Items();
    std::vector<std::string> ids;
    for (auto const& s : sel)
    {
        uint32_t idx = 0;
        if (items.IndexOf(s, idx) && idx < m_displayedIds.size())
            ids.push_back(m_displayedIds[idx]);
    }
    for (auto& id : ids) gpu_bench::DeleteResult(id);
    refreshHistory();
}

// ---- open user data folders. results.json and RenderDoc .rdc captures share
// the same PathService data root but live in separate subfolders, so History
// offers one button per folder rather than pretending they are one place.
// PathService throws when a directory cannot be created; a Click handler must
// not let that escape or the whole GUI dies. ----
static void openFolderInExplorer(std::filesystem::path (*resolve)())
{
    std::filesystem::path dir;
    try { dir = resolve(); }
    catch (std::exception const&) { return; }
    ShellExecuteW(nullptr, L"open", dir.wstring().c_str(),
                  nullptr, nullptr, SW_SHOWNORMAL);
}

void MainWindow::OnOpenResultsFolder(IInspectable const&, RoutedEventArgs const&)
{
    openFolderInExplorer(&gpu_bench::paths::ResultsDirectory);
}

void MainWindow::OnOpenCapturesFolder(IInspectable const&, RoutedEventArgs const&)
{
    openFolderInExplorer(&gpu_bench::paths::CapturesDirectory);
}

// ---- charts (run plot_workloads.py, then show the PNGs) --------------------
void MainWindow::OnGenerateCharts(IInspectable const&, RoutedEventArgs const&)
{
    if (m_enginePath.empty()) { ChartsStatus().Text(locText("Engine/repo not found.", "未找到引擎/仓库。")); return; }
    auto repo = pathFromUtf8(m_enginePath).parent_path().parent_path().parent_path();
    if (!std::filesystem::exists(repo / "scripts" / "plot_workloads.py"))
    { ChartsStatus().Text(locText("scripts/plot_workloads.py not found.", "未找到 scripts/plot_workloads.py。")); return; }
    const auto python = findPythonExecutable();
    if (python.empty())
    {
        ChartsStatus().Text(locText(
            "Charts unavailable: this package does not yet include the frozen report worker.",
            "图表不可用：当前安装包尚未包含冻结的报告工具。"));
        return;
    }

    const auto resultsPath = pathFromUtf8(gpu_bench::ResultsFilePath());
    const auto imageDir = resultsPath.parent_path().parent_path() / L"reports" / L"images";
    std::error_code ec;
    std::filesystem::create_directories(imageDir, ec);
    if (ec)
    {
        ChartsStatus().Text(locText("Could not create the reports directory.",
                                    "无法创建报告目录。"));
        return;
    }

    if (!tryBeginTask(ActiveTask::Charts))
    {
        ChartsStatus().Text(locText(
            "Another benchmark or report task is already running.",
            "另一个测试或报告任务正在运行。"));
        return;
    }
    ChartsBusy().IsActive(true);
    ChartsStatus().Text(locText("Running plot_workloads.py…", "正在运行 plot_workloads.py…"));

    auto strong = get_strong();
    auto disp = m_dispatcher;
    std::wstring repoW = repo.wstring();
    const auto pythonW = python.wstring();
    const auto resultsW = resultsPath.wstring();
    const auto imageDirW = imageDir.wstring();
    std::thread([this, strong, disp, repoW, pythonW, resultsW, imageDirW]()
    {
      try
      {
        auto result = runProcess(
            L"\"" + pythonW + L"\" \"scripts\\plot_workloads.py\" --input \""
            + resultsW + L"\" --out \"" + imageDirW + L"\"", repoW);
        disp.TryEnqueue([this, strong, imageDirW, result = std::move(result)]()
        {
            ChartsPanel().Children().Clear();
            std::filesystem::path imgDir(imageDirW);
            const char* names[] = { "workload_stream", "workload_gpu_burn", "workload_gpu_stress",
                                    "workload_nbody", "workload_stress",
                                    "workload_render3d", "workload_volumetric",
                                    "workload_cinematic_liquid", "workload_synthpeak" };
            int shown = 0;
            for (auto n : names)
            {
                auto p = imgDir / (std::string(n) + ".png");
                if (!std::filesystem::exists(p)) continue;
                Media::Imaging::BitmapImage bmp;
                bmp.CreateOptions(Media::Imaging::BitmapCreateOptions::IgnoreImageCache);
                bmp.UriSource(fileUri(p));
                Image img; img.Source(bmp); img.Stretch(Stretch::Uniform);
                img.MaxWidth(900); img.HorizontalAlignment(HorizontalAlignment::Left);
                ChartsPanel().Children().Append(img);
                ++shown;
            }
            const auto count = std::to_string(shown);
            ChartsStatus().Text(result.exitCode == 0
                ? (i18n::currentLang() == i18n::Lang::Zh
                    ? u8("完成 —— " + count + " 张图表")
                    : u8("Done — " + count + " chart(s)"))
                : (i18n::currentLang() == i18n::Lang::Zh
                    ? u8("python 退出码 " + std::to_string(result.exitCode))
                    : u8("python exited with " + std::to_string(result.exitCode))));
            endTask(ActiveTask::Charts); ChartsBusy().IsActive(false);
        });
      }
      catch (...)
      {
          disp.TryEnqueue([this, strong]()
          {
              ChartsStatus().Text(locText("Chart generation failed.", "图表生成失败"));
              endTask(ActiveTask::Charts);
              ChartsBusy().IsActive(false);
          });
      }
    }).detach();
}

}
