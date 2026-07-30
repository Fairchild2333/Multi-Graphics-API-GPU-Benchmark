#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "uxtheme.lib")

namespace {

constexpr wchar_t kWindowClass[] = L"MangekyoNativeSetup";
constexpr wchar_t kLicenceWindowClass[] = L"MangekyoNativeSetupLicence";
constexpr char kPayloadMagic[16] = {
    'M', 'A', 'N', 'G', 'E', 'K', 'Y', 'O', '_', 'M', 'S', 'I', '_', 'V', '1', '\0'};

#pragma pack(push, 1)
struct PayloadFooter {
    char magic[16];
    std::uint64_t payloadSize;
};
#pragma pack(pop)

enum ControlId {
    IdLanguage = 100,
    IdHeading,
    IdDescription,
    IdLanguageLabel,
    IdPathLabel,
    IdPath,
    IdBrowse,
    IdNotice,
    IdLicence,
    IdViewLicence,
    IdDesktopShortcut,
    IdStartMenuShortcut,
    IdInstall,
    IdCancel,
    IdProgress,
    IdStatus,
    IdLicenceOk,
};

enum class Language { English, Chinese };

struct UiText {
    const wchar_t* title;
    const wchar_t* heading;
    const wchar_t* description;
    const wchar_t* language;
    const wchar_t* installPath;
    const wchar_t* browse;
    const wchar_t* notice;
    const wchar_t* licence;
    const wchar_t* viewLicence;
    const wchar_t* install;
    const wchar_t* cancel;
    const wchar_t* installing;
    const wchar_t* complete;
    const wchar_t* failed;
    const wchar_t* corrupt;
    const wchar_t* invalidPath;
    const wchar_t* close;
    const wchar_t* apply;
    const wchar_t* updatingShortcuts;
    const wchar_t* shortcutsUpdated;
    const wchar_t* completeHeading;
    const wchar_t* completeDescription;
    const wchar_t* desktopShortcut;
    const wchar_t* startMenuShortcut;
};

constexpr UiText kEnglish{
    L"Mangekyo Setup",
    L"Install Mangekyo",
    L"Cross-API GPU and CPU benchmark",
    L"Setup language",
    L"Install location",
    L"Browse...",
    L"Mangekyo will be installed for this computer. Windows may ask for administrator permission.",
    L"I accept the MIT licence terms",
    L"View licence",
    L"Install",
    L"Cancel",
    L"Installing Mangekyo...",
    L"Mangekyo was installed successfully.",
    L"Installation failed. Windows Installer returned code %lu.",
    L"The embedded installer is missing or damaged.",
    L"Choose a valid absolute installation path.",
    L"Close",
    L"Apply",
    L"Updating shortcut options...",
    L"Shortcut options updated.",
    L"Installation complete",
    L"Mangekyo is ready to use.",
    L"Create a desktop shortcut",
    L"Create a Start menu shortcut"};

constexpr UiText kChinese{
    L"Mangekyo 安装程序",
    L"安装 Mangekyo",
    L"跨 API GPU 与 CPU 基准测试",
    L"安装语言",
    L"安装位置",
    L"浏览……",
    L"Mangekyo 将安装到此电脑。Windows 可能会请求管理员权限。",
    L"我接受 MIT 许可证条款",
    L"查看许可证",
    L"安装",
    L"取消",
    L"正在安装 Mangekyo……",
    L"Mangekyo 安装成功。",
    L"安装失败。Windows Installer 返回代码 %lu。",
    L"内嵌安装程序缺失或已损坏。",
    L"请选择有效的绝对安装路径。",
    L"关闭",
    L"应用",
    L"正在更新快捷方式选项……",
    L"快捷方式选项已更新。",
    L"安装完成",
    L"Mangekyo 已可以使用。",
    L"创建桌面快捷方式",
    L"创建开始菜单快捷方式"};

constexpr wchar_t kLicenceText[] =
    L"MIT License\r\n\r\n"
    L"Copyright (c) 2026 Mangekyo contributors\r\n\r\n"
    L"Permission is hereby granted, free of charge, to any person obtaining a copy "
    L"of this software and associated documentation files (the \"Software\"), to deal "
    L"in the Software without restriction, including without limitation the rights "
    L"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies "
    L"of the Software, and to permit persons to whom the Software is furnished to do so, "
    L"subject to the following conditions:\r\n\r\n"
    L"The above copyright notice and this permission notice shall be included in all "
    L"copies or substantial portions of the Software.\r\n\r\n"
    L"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.";

HINSTANCE gInstance = nullptr;
HWND gWindow = nullptr;
HWND gLicenceWindow = nullptr;
HFONT gFont = nullptr;
HFONT gHeadingFont = nullptr;
HFONT gSmallFont = nullptr;
HBRUSH gBackgroundBrush = nullptr;
HBRUSH gCardBrush = nullptr;
HBRUSH gEditBrush = nullptr;
Language gLanguage = Language::English;
bool gLanguageForced = false;
bool gInstalling = false;
bool gLicenceAccepted = false;
bool gInstallComplete = false;
bool gHasRunError = false;
bool gCreateDesktopShortcut = false;
bool gCreateStartMenuShortcut = true;
bool gInstalledDesktopShortcut = false;
bool gInstalledStartMenuShortcut = true;
bool gShortcutOptionsDirty = false;
bool gUpdatingShortcuts = false;
bool gProgressVisible = false;
bool gProgressComplete = false;
bool gProgressError = false;
int gProgressPulse = 0;
ITaskbarList3* gTaskbar = nullptr;
std::wstring gExtractedMsi;
std::wstring gInstallPath;

struct ThemePalette {
    COLORREF background;
    COLORREF card;
    COLORREF input;
    COLORREF border;
    COLORREF text;
    COLORREF muted;
    COLORREF accent;
    COLORREF accentPressed;
    COLORREF disabled;
    COLORREF disabledText;
};

bool gDarkTheme = false;
ThemePalette gTheme{};

HWND Item(int id);

bool SystemUsesDarkTheme() {
    DWORD lightTheme = 1;
    DWORD size = sizeof(lightTheme);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &lightTheme, &size);
    return status == ERROR_SUCCESS && lightTheme == 0;
}

void DeleteThemeBrushes() {
    if (gBackgroundBrush) DeleteObject(gBackgroundBrush);
    if (gCardBrush) DeleteObject(gCardBrush);
    if (gEditBrush) DeleteObject(gEditBrush);
    gBackgroundBrush = nullptr;
    gCardBrush = nullptr;
    gEditBrush = nullptr;
}

void ApplyWindowChrome(HWND window) {
    if (!window) return;
    const BOOL dark = gDarkTheme ? TRUE : FALSE;
    DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
    DwmSetWindowAttribute(window, 19, &dark, sizeof(dark));
}

void ApplySystemTheme() {
    gDarkTheme = SystemUsesDarkTheme();
    gTheme = gDarkTheme
        ? ThemePalette{RGB(27, 26, 25), RGB(38, 36, 35), RGB(48, 46, 44),
                       RGB(74, 70, 67), RGB(244, 241, 239), RGB(181, 174, 169),
                       RGB(171, 132, 112), RGB(145, 108, 91), RGB(67, 64, 62),
                       RGB(139, 133, 129)}
        : ThemePalette{RGB(247, 246, 245), RGB(255, 255, 255), RGB(255, 255, 255),
                       RGB(220, 216, 213), RGB(37, 34, 32), RGB(103, 98, 94),
                       RGB(112, 85, 73), RGB(91, 68, 59), RGB(229, 225, 222),
                       RGB(145, 139, 135)};
    DeleteThemeBrushes();
    gBackgroundBrush = CreateSolidBrush(gTheme.background);
    gCardBrush = CreateSolidBrush(gTheme.card);
    gEditBrush = CreateSolidBrush(gTheme.input);
    if (gWindow) {
        ApplyWindowChrome(gWindow);
        ApplyWindowChrome(gLicenceWindow);
        if (Item(IdPath)) {
            SetWindowTheme(Item(IdPath), gDarkTheme ? L"DarkMode_Explorer" : L"Explorer", nullptr);
        }
        InvalidateRect(gWindow, nullptr, TRUE);
        RedrawWindow(gWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
        if (gLicenceWindow) {
            InvalidateRect(gLicenceWindow, nullptr, TRUE);
            RedrawWindow(gLicenceWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
    }
}

const UiText& Text() { return gLanguage == Language::Chinese ? kChinese : kEnglish; }

HWND Item(int id) { return GetDlgItem(gWindow, id); }

void SetText(int id, const wchar_t* value) { SetWindowTextW(Item(id), value); }

void ApplyLanguage() {
    const auto& text = Text();
    SetWindowTextW(gWindow, text.title);
    SetText(IdHeading, text.heading);
    SetText(IdDescription, text.description);
    SetText(IdLanguageLabel, text.language);
    SetText(IdPathLabel, text.installPath);
    SetText(IdBrowse, text.browse);
    SetText(IdNotice, text.notice);
    SetText(IdLicence, text.licence);
    SetText(IdViewLicence, text.viewLicence);
    SetText(IdDesktopShortcut, text.desktopShortcut);
    SetText(IdStartMenuShortcut, text.startMenuShortcut);
    SetText(IdInstall, gInstallComplete
                           ? (gShortcutOptionsDirty ? text.apply : text.close)
                           : text.install);
    SetText(IdCancel, text.cancel);
    SetText(IdLanguage, gLanguage == Language::Chinese ? L"简体中文" : L"English");
    if (!gInstalling) SetText(IdStatus, L"");
    InvalidateRect(Item(IdLanguage), nullptr, TRUE);
    InvalidateRect(Item(IdLicence), nullptr, TRUE);
}

void ShowLanguageMenu() {
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    MENUINFO menuInfo{sizeof(menuInfo)};
    menuInfo.fMask = MIM_BACKGROUND | MIM_STYLE;
    menuInfo.hbrBack = gCardBrush;
    menuInfo.dwStyle = MNS_CHECKORBMP | MNS_NOCHECK;
    SetMenuInfo(menu, &menuInfo);
    MENUITEMINFOW item{sizeof(item)};
    item.fMask = MIIM_FTYPE | MIIM_ID | MIIM_DATA;
    item.fType = MFT_OWNERDRAW;
    item.wID = 1;
    item.dwItemData = 1;
    InsertMenuItemW(menu, 0, TRUE, &item);
    item.wID = 2;
    item.dwItemData = 2;
    InsertMenuItemW(menu, 1, TRUE, &item);
    RECT anchor{};
    GetWindowRect(Item(IdLanguage), &anchor);
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                                        anchor.left, anchor.bottom + 4, 0, gWindow, nullptr);
    DestroyMenu(menu);
    if (command == 1 || command == 2) {
        gLanguage = command == 2 ? Language::Chinese : Language::English;
        ApplyLanguage();
    }
}

std::wstring DefaultInstallPath() {
    wchar_t path[32768]{};
    const wchar_t* suffix =
#if defined(_M_ARM64)
        L"\\Mangekyo ARM64";
#else
        L"\\Mangekyo";
#endif
    const DWORD length = ExpandEnvironmentStringsW(L"%ProgramFiles%", path, static_cast<DWORD>(std::size(path)));
    if (!length || length >= std::size(path)) return L"C:\\Program Files" + std::wstring(suffix);
    return std::wstring(path) + suffix;
}

bool SelectedInstallPath(std::wstring& path) {
    const int length = GetWindowTextLengthW(Item(IdPath));
    if (length <= 0) return false;
    std::vector<wchar_t> input(static_cast<size_t>(length) + 1);
    GetWindowTextW(Item(IdPath), input.data(), static_cast<int>(input.size()));
    std::vector<wchar_t> full(32768);
    const DWORD fullLength = GetFullPathNameW(input.data(), static_cast<DWORD>(full.size()), full.data(), nullptr);
    if (!fullLength || fullLength >= full.size() || PathIsRelativeW(full.data()) || wcschr(full.data(), L'"')) {
        return false;
    }
    path.assign(full.data(), fullLength);
    while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
    return true;
}

void BrowseForInstallPath() {
    IFileDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog)))) return;
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    std::wstring current;
    if (SelectedInstallPath(current)) {
        IShellItem* folder = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(current.c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder);
            folder->Release();
        }
    }
    if (SUCCEEDED(dialog->Show(gWindow))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR selected = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &selected))) {
                SetWindowTextW(Item(IdPath), selected);
                CoTaskMemFree(selected);
            }
            item->Release();
        }
    }
    dialog->Release();
}

bool IsChineseUiLanguage() {
    const LANGID lang = GetUserDefaultUILanguage();
    return PRIMARYLANGID(lang) == LANG_CHINESE;
}

std::wstring ExecutablePath() {
    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!length || length >= path.size()) return {};
    return std::wstring(path.data(), length);
}

bool ExtractPayload(std::wstring& destination) {
    const std::wstring sourcePath = ExecutablePath();
    if (sourcePath.empty()) return false;

    HANDLE source = CreateFileW(sourcePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (source == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER fileSize{};
    if (!GetFileSizeEx(source, &fileSize) || fileSize.QuadPart < static_cast<LONGLONG>(sizeof(PayloadFooter))) {
        CloseHandle(source);
        return false;
    }

    // Authenticode places WIN_CERTIFICATE after the signed image. When present,
    // the MSI footer is immediately before that certificate (plus at most seven
    // alignment bytes), rather than at physical end-of-file.
    LONGLONG logicalEnd = fileSize.QuadPart;
    DWORD peOffset = 0;
    DWORD certificateOffset = 0;
    DWORD read = 0;
    SetFilePointer(source, 0x3c, nullptr, FILE_BEGIN);
    ReadFile(source, &peOffset, sizeof(peOffset), &read, nullptr);
    if (read == sizeof(peOffset)) {
        WORD optionalMagic = 0;
        LARGE_INTEGER magicPosition{};
        magicPosition.QuadPart = static_cast<LONGLONG>(peOffset) + 24;
        SetFilePointerEx(source, magicPosition, nullptr, FILE_BEGIN);
        ReadFile(source, &optionalMagic, sizeof(optionalMagic), &read, nullptr);
        if (read == sizeof(optionalMagic) && (optionalMagic == 0x20b || optionalMagic == 0x10b)) {
            const LONGLONG dataDirectory = static_cast<LONGLONG>(peOffset) + 24 +
                                           (optionalMagic == 0x20b ? 112 : 96);
            LARGE_INTEGER securityDirectory{};
            securityDirectory.QuadPart = dataDirectory + 4 * 8;
            SetFilePointerEx(source, securityDirectory, nullptr, FILE_BEGIN);
            ReadFile(source, &certificateOffset, sizeof(certificateOffset), &read, nullptr);
            if (read == sizeof(certificateOffset) && certificateOffset > 0 &&
                certificateOffset <= static_cast<DWORD64>(fileSize.QuadPart)) {
                logicalEnd = certificateOffset;
            }
        }
    }

    LARGE_INTEGER footerPosition{};
    PayloadFooter footer{};
    bool footerFound = false;
    for (int padding = 0; padding < 8; ++padding) {
        footerPosition.QuadPart = logicalEnd - padding - sizeof(PayloadFooter);
        if (footerPosition.QuadPart <= 0) break;
        SetFilePointerEx(source, footerPosition, nullptr, FILE_BEGIN);
        read = 0;
        if (ReadFile(source, &footer, sizeof(footer), &read, nullptr) && read == sizeof(footer) &&
            memcmp(footer.magic, kPayloadMagic, sizeof(kPayloadMagic)) == 0 && footer.payloadSize > 0 &&
            footer.payloadSize <= static_cast<std::uint64_t>(footerPosition.QuadPart)) {
            footerFound = true;
            break;
        }
    }
    if (!footerFound) {
        CloseHandle(source);
        return false;
    }

    wchar_t tempDirectory[MAX_PATH]{};
    if (!GetTempPathW(MAX_PATH, tempDirectory)) {
        CloseHandle(source);
        return false;
    }
    destination = tempDirectory;
    destination += L"Mangekyo-Setup-" + std::to_wstring(GetCurrentProcessId()) + L".msi";

    HANDLE output = CreateFileW(destination.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        CloseHandle(source);
        return false;
    }

    LARGE_INTEGER payloadPosition{};
    payloadPosition.QuadPart = footerPosition.QuadPart - static_cast<LONGLONG>(footer.payloadSize);
    SetFilePointerEx(source, payloadPosition, nullptr, FILE_BEGIN);
    // The worker uses CreateThread's default stack (typically 1 MiB). A 1 MiB
    // local buffer overflows before execution reaches ShellExecuteEx/UAC.
    std::vector<BYTE> buffer(1024 * 1024);
    std::uint64_t remaining = footer.payloadSize;
    bool ok = true;
    while (remaining) {
        const DWORD wanted = static_cast<DWORD>(std::min<std::uint64_t>(remaining, buffer.size()));
        DWORD bytesRead = 0;
        DWORD bytesWritten = 0;
        if (!ReadFile(source, buffer.data(), wanted, &bytesRead, nullptr) || bytesRead != wanted ||
            !WriteFile(output, buffer.data(), bytesRead, &bytesWritten, nullptr) || bytesWritten != bytesRead) {
            ok = false;
            break;
        }
        remaining -= bytesRead;
    }
    CloseHandle(output);
    CloseHandle(source);
    if (!ok) {
        DeleteFileW(destination.c_str());
        destination.clear();
    }
    return ok;
}

DWORD RunInstaller(bool updateShortcuts = false) {
    if (gExtractedMsi.empty() && !ExtractPayload(gExtractedMsi)) return ERROR_BAD_FORMAT;
    if (gInstallPath.empty()) gInstallPath = DefaultInstallPath();
    std::wstring parameters = L"/i \"" + gExtractedMsi +
                              L"\" /qn /norestart INSTALL_ROOT=\"" + gInstallPath +
                              L"\" CREATE_DESKTOP_SHORTCUT=" +
                              (gCreateDesktopShortcut ? L"1" : L"0") +
                              L" CREATE_START_MENU_SHORTCUT=" +
                              (gCreateStartMenuShortcut ? L"1" : L"0");
    if (updateShortcuts) {
        parameters += L" REINSTALL=ALL REINSTALLMODE=vomus";
    }
    SHELLEXECUTEINFOW execute{};
    execute.cbSize = sizeof(execute);
    execute.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    execute.lpVerb = L"runas";
    execute.lpFile = L"msiexec.exe";
    execute.lpParameters = parameters.c_str();
    execute.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&execute)) {
        return GetLastError();
    }
    WaitForSingleObject(execute.hProcess, INFINITE);
    DWORD exitCode = ERROR_INSTALL_FAILURE;
    GetExitCodeProcess(execute.hProcess, &exitCode);
    CloseHandle(execute.hProcess);
    return exitCode;
}

DWORD WINAPI InstallThread(void*) {
    const DWORD result = RunInstaller(gUpdatingShortcuts);
    PostMessageW(gWindow, WM_APP + 1, result, 0);
    return 0;
}

void BeginInstall() {
    if (!SelectedInstallPath(gInstallPath)) {
        gHasRunError = true;
        SetText(IdStatus, Text().invalidPath);
        InvalidateRect(Item(IdStatus), nullptr, TRUE);
        SetFocus(Item(IdPath));
        return;
    }
    gInstalling = true;
    gUpdatingShortcuts = false;
    gInstallComplete = false;
    gHasRunError = false;
    EnableWindow(Item(IdLanguage), FALSE);
    EnableWindow(Item(IdPath), FALSE);
    EnableWindow(Item(IdBrowse), FALSE);
    EnableWindow(Item(IdLicence), FALSE);
    EnableWindow(Item(IdViewLicence), FALSE);
    EnableWindow(Item(IdDesktopShortcut), FALSE);
    EnableWindow(Item(IdStartMenuShortcut), FALSE);
    EnableWindow(Item(IdInstall), FALSE);
    EnableWindow(Item(IdCancel), FALSE);
    SetText(IdStatus, Text().installing);
    gProgressVisible = true;
    gProgressComplete = false;
    gProgressError = false;
    gProgressPulse = 0;
    ShowWindow(Item(IdProgress), SW_SHOW);
    SetTimer(gWindow, 1, 24, nullptr);
    InvalidateRect(Item(IdProgress), nullptr, TRUE);
    if (gTaskbar) gTaskbar->SetProgressState(gWindow, TBPF_INDETERMINATE);
    HANDLE thread = CreateThread(nullptr, 0, InstallThread, nullptr, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    } else {
        PostMessageW(gWindow, WM_APP + 1, GetLastError(), 0);
    }
}

void BeginShortcutUpdate() {
    gInstalling = true;
    gUpdatingShortcuts = true;
    gHasRunError = false;
    EnableWindow(Item(IdDesktopShortcut), FALSE);
    EnableWindow(Item(IdStartMenuShortcut), FALSE);
    EnableWindow(Item(IdInstall), FALSE);
    SetText(IdStatus, Text().updatingShortcuts);
    gProgressVisible = true;
    gProgressComplete = false;
    gProgressError = false;
    gProgressPulse = 0;
    ShowWindow(Item(IdProgress), SW_SHOW);
    SetTimer(gWindow, 1, 24, nullptr);
    InvalidateRect(Item(IdProgress), nullptr, TRUE);
    if (gTaskbar) gTaskbar->SetProgressState(gWindow, TBPF_INDETERMINATE);
    HANDLE thread = CreateThread(nullptr, 0, InstallThread, nullptr, 0, nullptr);
    if (thread) {
        CloseHandle(thread);
    } else {
        PostMessageW(gWindow, WM_APP + 1, GetLastError(), 0);
    }
}

void FinishInstall(DWORD result) {
    gInstalling = false;
    KillTimer(gWindow, 1);
    const bool shortcutUpdate = gUpdatingShortcuts;
    gUpdatingShortcuts = false;
    if (result == ERROR_SUCCESS || result == ERROR_SUCCESS_REBOOT_REQUIRED) {
        gProgressComplete = true;
        gProgressError = false;
        ShowWindow(Item(IdProgress), SW_SHOW);
        InvalidateRect(Item(IdProgress), nullptr, TRUE);
        if (gTaskbar) {
            gTaskbar->SetProgressState(gWindow, TBPF_NORMAL);
            gTaskbar->SetProgressValue(gWindow, 100, 100);
        }
        gInstallComplete = true;
        gHasRunError = false;
        gInstalledDesktopShortcut = gCreateDesktopShortcut;
        gInstalledStartMenuShortcut = gCreateStartMenuShortcut;
        gShortcutOptionsDirty = false;
        SetText(IdHeading, Text().completeHeading);
        SetText(IdDescription, Text().completeDescription);
        SetText(IdStatus, shortcutUpdate ? Text().shortcutsUpdated : Text().complete);
        ShowWindow(Item(IdCancel), SW_HIDE);
        SetText(IdInstall, Text().close);
        EnableWindow(Item(IdDesktopShortcut), TRUE);
        EnableWindow(Item(IdStartMenuShortcut), TRUE);
        EnableWindow(Item(IdInstall), TRUE);
        SetFocus(Item(IdInstall));
        return;
    }
    gHasRunError = true;
    gProgressComplete = true;
    gProgressError = true;
    ShowWindow(Item(IdProgress), SW_SHOW);
    InvalidateRect(Item(IdProgress), nullptr, TRUE);
    if (gTaskbar) {
        gTaskbar->SetProgressState(gWindow, TBPF_ERROR);
        gTaskbar->SetProgressValue(gWindow, 100, 100);
    }
    wchar_t message[256]{};
    if (result == ERROR_BAD_FORMAT) {
        lstrcpynW(message, Text().corrupt, static_cast<int>(std::size(message)));
    } else {
        wsprintfW(message, Text().failed, result);
    }
    SetText(IdStatus, message);
    InvalidateRect(Item(IdStatus), nullptr, TRUE);
    EnableWindow(Item(IdLanguage), TRUE);
    EnableWindow(Item(IdPath), TRUE);
    EnableWindow(Item(IdBrowse), TRUE);
    EnableWindow(Item(IdLicence), TRUE);
    EnableWindow(Item(IdViewLicence), TRUE);
    EnableWindow(Item(IdDesktopShortcut), TRUE);
    EnableWindow(Item(IdStartMenuShortcut), TRUE);
    EnableWindow(Item(IdCancel), TRUE);
    SetText(IdInstall, gInstallComplete ? Text().apply : Text().install);
    EnableWindow(Item(IdInstall), gInstallComplete || gLicenceAccepted);
}

void UpdateShortcutOptionState() {
    if (!gInstallComplete) return;
    gShortcutOptionsDirty =
        gCreateDesktopShortcut != gInstalledDesktopShortcut ||
        gCreateStartMenuShortcut != gInstalledStartMenuShortcut;
    SetText(IdInstall, gShortcutOptionsDirty ? Text().apply : Text().close);
    EnableWindow(Item(IdInstall), TRUE);
}

HWND AddControl(const wchar_t* className, const wchar_t* text, DWORD style,
                int x, int y, int width, int height, int id, HFONT font = nullptr,
                DWORD extendedStyle = 0) {
    HWND control = CreateWindowExW(extendedStyle, className, text, WS_CHILD | WS_VISIBLE | style,
                                   x, y, width, height, gWindow,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), gInstance, nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font ? font : gFont), TRUE);
    return control;
}

void DrawOwnerButton(const DRAWITEMSTRUCT* item) {
    const bool primary = item->CtlID == IdInstall || item->CtlID == IdLicenceOk;
    const bool checkbox = item->CtlID == IdLicence ||
                          item->CtlID == IdDesktopShortcut ||
                          item->CtlID == IdStartMenuShortcut;
    const bool dropdown = item->CtlID == IdLanguage;
    const bool checked = item->CtlID == IdLicence ? gLicenceAccepted
                         : item->CtlID == IdDesktopShortcut ? gCreateDesktopShortcut
                         : item->CtlID == IdStartMenuShortcut ? gCreateStartMenuShortcut
                         : false;
    const bool enabled = IsWindowEnabled(item->hwndItem) != FALSE;
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    COLORREF fill = primary ? gTheme.accent : (checkbox ? gTheme.card : gTheme.input);
    COLORREF border = primary ? gTheme.accent : gTheme.border;
    COLORREF text = primary ? RGB(255, 255, 255) : gTheme.text;
    if (!enabled) {
        fill = gTheme.disabled;
        border = gTheme.border;
        text = gTheme.disabledText;
    } else if (pressed) {
        fill = primary ? gTheme.accentPressed
                       : (gDarkTheme ? RGB(58, 55, 53) : RGB(239, 236, 234));
    }

    wchar_t label[128]{};
    GetWindowTextW(item->hwndItem, label, static_cast<int>(std::size(label)));
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, text);
    SelectObject(item->hDC, gFont);

    if (checkbox) {
        FillRect(item->hDC, &item->rcItem, gCardBrush);
        RECT box{item->rcItem.left + 2, item->rcItem.top + 4,
                 item->rcItem.left + 22, item->rcItem.top + 24};
        HBRUSH boxBrush = CreateSolidBrush(checked ? gTheme.accent : gTheme.input);
        HPEN boxPen = CreatePen(PS_SOLID, 1, checked ? gTheme.accent : gTheme.border);
        HGDIOBJ oldBrush = SelectObject(item->hDC, boxBrush);
        HGDIOBJ oldPen = SelectObject(item->hDC, boxPen);
        RoundRect(item->hDC, box.left, box.top, box.right, box.bottom, 6, 6);
        if (checked) {
            HPEN checkPen = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            SelectObject(item->hDC, checkPen);
            MoveToEx(item->hDC, box.left + 5, box.top + 10, nullptr);
            LineTo(item->hDC, box.left + 9, box.top + 14);
            LineTo(item->hDC, box.left + 16, box.top + 6);
            SelectObject(item->hDC, oldPen);
            DeleteObject(checkPen);
        } else {
            SelectObject(item->hDC, oldPen);
        }
        SelectObject(item->hDC, oldBrush);
        DeleteObject(boxPen);
        DeleteObject(boxBrush);
        RECT textRect = item->rcItem;
        textRect.left += 32;
        DrawTextW(item->hDC, label, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        return;
    }

    HBRUSH brush = CreateSolidBrush(fill);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HGDIOBJ oldBrush = SelectObject(item->hDC, brush);
    HGDIOBJ oldPen = SelectObject(item->hDC, pen);
    RoundRect(item->hDC, item->rcItem.left, item->rcItem.top, item->rcItem.right,
              item->rcItem.bottom, 10, 10);
    SelectObject(item->hDC, oldPen);
    SelectObject(item->hDC, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
    RECT textRect = item->rcItem;
    if (pressed) OffsetRect(&textRect, 0, 1);
    if (dropdown) {
        textRect.left += 14;
        textRect.right -= 42;
        DrawTextW(item->hDC, label, -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        const int centreX = item->rcItem.right - 22;
        const int centreY = (item->rcItem.top + item->rcItem.bottom) / 2;
        HPEN chevron = CreatePen(PS_SOLID, 2, text);
        HGDIOBJ oldChevron = SelectObject(item->hDC, chevron);
        MoveToEx(item->hDC, centreX - 5, centreY - 2, nullptr);
        LineTo(item->hDC, centreX, centreY + 3);
        LineTo(item->hDC, centreX + 5, centreY - 2);
        SelectObject(item->hDC, oldChevron);
        DeleteObject(chevron);
    } else {
        DrawTextW(item->hDC, label, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void DrawModernProgress(const DRAWITEMSTRUCT* item) {
    const int saved = SaveDC(item->hDC);
    const RECT bounds = item->rcItem;
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    HBRUSH trackBrush = CreateSolidBrush(gDarkTheme ? RGB(61, 58, 56) : RGB(224, 220, 217));
    HPEN trackPen = CreatePen(PS_NULL, 0, gTheme.border);
    HGDIOBJ oldBrush = SelectObject(item->hDC, trackBrush);
    HGDIOBJ oldPen = SelectObject(item->hDC, trackPen);
    RoundRect(item->hDC, bounds.left, bounds.top, bounds.right, bounds.bottom,
              height, height);
    SelectObject(item->hDC, oldPen);
    SelectObject(item->hDC, oldBrush);
    DeleteObject(trackPen);
    DeleteObject(trackBrush);

    HRGN clip = CreateRoundRectRgn(bounds.left, bounds.top, bounds.right + 1,
                                   bounds.bottom + 1, height, height);
    SelectClipRgn(item->hDC, clip);
    const COLORREF progressColour = gProgressError
                                        ? (gDarkTheme ? RGB(255, 99, 86) : RGB(196, 43, 28))
                                        : gTheme.accent;
    HBRUSH progressBrush = CreateSolidBrush(progressColour);
    HPEN progressPen = CreatePen(PS_NULL, 0, progressColour);
    oldBrush = SelectObject(item->hDC, progressBrush);
    oldPen = SelectObject(item->hDC, progressPen);
    if (gProgressComplete) {
        RoundRect(item->hDC, bounds.left, bounds.top, bounds.right, bounds.bottom,
                  height, height);
    } else if (gProgressVisible) {
        const int segmentWidth = (std::max)(88, width / 4);
        const int travel = width + segmentWidth;
        const int left = bounds.left + (gProgressPulse % travel) - segmentWidth;
        RoundRect(item->hDC, left, bounds.top, left + segmentWidth, bounds.bottom,
                  height, height);
    }
    SelectObject(item->hDC, oldPen);
    SelectObject(item->hDC, oldBrush);
    DeleteObject(progressPen);
    DeleteObject(progressBrush);
    DeleteObject(clip);
    RestoreDC(item->hDC, saved);
}

void DrawLanguageMenuItem(const DRAWITEMSTRUCT* item) {
    const bool selected = (item->itemState & ODS_SELECTED) != 0;
    const bool checked = (item->itemData == 1 && gLanguage == Language::English) ||
                         (item->itemData == 2 && gLanguage == Language::Chinese);
    const COLORREF fill = selected ? gTheme.input : gTheme.card;
    HBRUSH brush = CreateSolidBrush(fill);
    FillRect(item->hDC, &item->rcItem, brush);
    DeleteObject(brush);

    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, gTheme.text);
    SelectObject(item->hDC, gFont);
    RECT textRect = item->rcItem;
    textRect.left += 46;
    DrawTextW(item->hDC, item->itemData == 2 ? L"简体中文" : L"English", -1,
              &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    if (checked) {
        HPEN checkPen = CreatePen(PS_SOLID, 2, gTheme.accent);
        HGDIOBJ oldPen = SelectObject(item->hDC, checkPen);
        const int x = item->rcItem.left + 18;
        const int y = (item->rcItem.top + item->rcItem.bottom) / 2;
        MoveToEx(item->hDC, x, y, nullptr);
        LineTo(item->hDC, x + 4, y + 4);
        LineTo(item->hDC, x + 11, y - 5);
        SelectObject(item->hDC, oldPen);
        DeleteObject(checkPen);
    }
    if (item->itemData == 1) {
        HPEN separator = CreatePen(PS_SOLID, 1, gTheme.border);
        HGDIOBJ oldPen = SelectObject(item->hDC, separator);
        MoveToEx(item->hDC, item->rcItem.left + 10, item->rcItem.bottom - 1, nullptr);
        LineTo(item->hDC, item->rcItem.right - 10, item->rcItem.bottom - 1);
        SelectObject(item->hDC, oldPen);
        DeleteObject(separator);
    }
}

LRESULT CALLBACK LicenceWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        gLicenceWindow = window;
        ApplyWindowChrome(window);
        HWND button = CreateWindowExW(
            0, L"BUTTON", gLanguage == Language::Chinese ? L"确定" : L"OK",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | BS_DEFPUSHBUTTON,
            514, 548, 102, 42, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IdLicenceOk)), gInstance, nullptr);
        SendMessageW(button, WM_SETFONT, reinterpret_cast<WPARAM>(gFont), TRUE);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IdLicenceOk) {
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_DRAWITEM:
        DrawOwnerButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        return TRUE;
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(gBackgroundBrush);
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        FillRect(dc, &client, gBackgroundBrush);

        HBRUSH iconBrush = CreateSolidBrush(gTheme.accent);
        HPEN iconPen = CreatePen(PS_SOLID, 1, gTheme.accent);
        HGDIOBJ oldBrush = SelectObject(dc, iconBrush);
        HGDIOBJ oldPen = SelectObject(dc, iconPen);
        Ellipse(dc, 32, 31, 74, 73);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(iconPen);
        DeleteObject(iconBrush);

        HPEN informationPen = CreatePen(PS_SOLID, 3, RGB(255, 255, 255));
        oldPen = SelectObject(dc, informationPen);
        MoveToEx(dc, 53, 48, nullptr);
        LineTo(dc, 53, 63);
        SelectObject(dc, oldPen);
        DeleteObject(informationPen);
        HBRUSH dotBrush = CreateSolidBrush(RGB(255, 255, 255));
        oldBrush = SelectObject(dc, dotBrush);
        Ellipse(dc, 51, 39, 55, 43);
        SelectObject(dc, oldBrush);
        DeleteObject(dotBrush);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, gTheme.text);
        SelectObject(dc, gHeadingFont);
        RECT headingRect{92, 32, client.right - 32, 76};
        DrawTextW(dc, gLanguage == Language::Chinese ? L"MIT 许可证" : L"MIT Licence", -1,
                  &headingRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

        const wchar_t* licenceBody = wcsstr(kLicenceText, L"\r\n\r\n");
        licenceBody = licenceBody ? licenceBody + 4 : kLicenceText;
        SelectObject(dc, gFont);
        RECT bodyRect{92, 94, client.right - 38, 520};
        DrawTextW(dc, licenceBody, -1, &bodyRect, DT_LEFT | DT_TOP | DT_WORDBREAK);

        HPEN separator = CreatePen(PS_SOLID, 1, gTheme.border);
        oldPen = SelectObject(dc, separator);
        MoveToEx(dc, 28, 534, nullptr);
        LineTo(dc, client.right - 28, 534);
        SelectObject(dc, oldPen);
        DeleteObject(separator);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        ApplySystemTheme();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        gLicenceWindow = nullptr;
        EnableWindow(gWindow, TRUE);
        SetForegroundWindow(gWindow);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowLicenceWindow() {
    if (gLicenceWindow) {
        SetForegroundWindow(gLicenceWindow);
        return;
    }
    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    RECT bounds{0, 0, 650, 620};
    AdjustWindowRectEx(&bounds, style, FALSE, WS_EX_DLGMODALFRAME);
    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    RECT owner{};
    GetWindowRect(gWindow, &owner);
    const int x = owner.left + ((owner.right - owner.left) - width) / 2;
    const int y = owner.top + ((owner.bottom - owner.top) - height) / 2;
    EnableWindow(gWindow, FALSE);
    HWND dialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME, kLicenceWindowClass,
        gLanguage == Language::Chinese ? L"MIT 许可证" : L"MIT Licence",
        style, x, y, width, height, gWindow, nullptr, gInstance, nullptr);
    if (!dialog) {
        EnableWindow(gWindow, TRUE);
        return;
    }
    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
        gWindow = window;
        if (SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(&gTaskbar)))) {
            if (FAILED(gTaskbar->HrInit())) {
                gTaskbar->Release();
                gTaskbar = nullptr;
            }
        }
        NONCLIENTMETRICSW metrics{sizeof(metrics)};
        SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
        gFont = CreateFontIndirectW(&metrics.lfMessageFont);
        LOGFONTW heading = metrics.lfMessageFont;
        heading.lfHeight = -30;
        heading.lfWeight = FW_SEMIBOLD;
        gHeadingFont = CreateFontIndirectW(&heading);
        LOGFONTW smallFontDescription = metrics.lfMessageFont;
        smallFontDescription.lfHeight = -14;
        gSmallFont = CreateFontIndirectW(&smallFontDescription);
        ApplySystemTheme();

        HWND icon = AddControl(L"STATIC", L"", SS_ICON | SS_CENTERIMAGE, 32, 26, 52, 52, -1);
        HICON appIcon = reinterpret_cast<HICON>(LoadImageW(
            gInstance, MAKEINTRESOURCEW(101), IMAGE_ICON, 48, 48, LR_DEFAULTCOLOR));
        if (appIcon) SendMessageW(icon, STM_SETICON, reinterpret_cast<WPARAM>(appIcon), 0);
        AddControl(L"STATIC", L"", SS_LEFT, 102, 25, 560, 40, IdHeading, gHeadingFont);
        AddControl(L"STATIC", L"", SS_LEFT, 104, 65, 555, 24, IdDescription);

        AddControl(L"STATIC", L"", SS_LEFT, 50, 137, 180, 24, IdLanguageLabel);
        AddControl(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP,
                   50, 162, 592, 38, IdLanguage);
        if (!gLanguageForced) {
            gLanguage = IsChineseUiLanguage() ? Language::Chinese : Language::English;
        }
        AddControl(L"STATIC", L"", SS_LEFT, 50, 210, 180, 24, IdPathLabel);
        HWND path = AddControl(L"EDIT", DefaultInstallPath().c_str(), ES_AUTOHSCROLL | WS_TABSTOP,
                               60, 243, 456, 22, IdPath);
        SendMessageW(path, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(0, 0));
        AddControl(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 538, 235, 104, 38, IdBrowse);
        AddControl(L"STATIC", L"", SS_LEFT, 50, 284, 592, 36, IdNotice, gSmallFont);
        AddControl(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 50, 326, 340, 28, IdLicence);
        AddControl(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 478, 321, 164, 36, IdViewLicence);
        AddControl(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP,
                   50, 368, 260, 28, IdDesktopShortcut);
        AddControl(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP,
                   320, 368, 330, 28, IdStartMenuShortcut);
        HWND progress = AddControl(L"STATIC", L"", SS_OWNERDRAW, 32, 420, 636, 8, IdProgress);
        ShowWindow(progress, SW_HIDE);
        AddControl(L"STATIC", L"", SS_LEFT, 32, 434, 636, 20, IdStatus, gSmallFont);
        AddControl(L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 454, 467, 102, 42, IdCancel);
        AddControl(L"BUTTON", L"", BS_OWNERDRAW | BS_DEFPUSHBUTTON | WS_TABSTOP,
                   566, 467, 102, 42, IdInstall);
        EnableWindow(Item(IdInstall), FALSE);
        ApplyLanguage();
        ApplySystemTheme();
        return 0;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IdLanguage:
            ShowLanguageMenu();
            return 0;
        case IdLicence:
            gLicenceAccepted = !gLicenceAccepted;
            InvalidateRect(Item(IdLicence), nullptr, TRUE);
            EnableWindow(Item(IdInstall), gLicenceAccepted && !gInstalling);
            return 0;
        case IdDesktopShortcut:
            gCreateDesktopShortcut = !gCreateDesktopShortcut;
            InvalidateRect(Item(IdDesktopShortcut), nullptr, TRUE);
            UpdateShortcutOptionState();
            return 0;
        case IdStartMenuShortcut:
            gCreateStartMenuShortcut = !gCreateStartMenuShortcut;
            InvalidateRect(Item(IdStartMenuShortcut), nullptr, TRUE);
            UpdateShortcutOptionState();
            return 0;
        case IdViewLicence:
            ShowLicenceWindow();
            return 0;
        case IdBrowse:
            BrowseForInstallPath();
            return 0;
        case IdInstall:
            if (gInstallComplete) {
                if (gShortcutOptionsDirty) {
                    BeginShortcutUpdate();
                } else {
                    DestroyWindow(window);
                }
            } else {
                BeginInstall();
            }
            return 0;
        case IdCancel:
            DestroyWindow(window);
            return 0;
        }
        break;
    case WM_DRAWITEM:
        if (wParam == 0) {
            DrawLanguageMenuItem(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        } else if (wParam == IdProgress) {
            DrawModernProgress(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        } else {
            DrawOwnerButton(reinterpret_cast<const DRAWITEMSTRUCT*>(lParam));
        }
        return TRUE;
    case WM_MEASUREITEM:
        if (wParam == 0) {
            auto* item = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
            item->itemWidth = 206;
            item->itemHeight = 38;
            return TRUE;
        }
        break;
    case WM_CTLCOLORSTATIC: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        const int id = GetDlgCtrlID(reinterpret_cast<HWND>(lParam));
        SetBkMode(dc, TRANSPARENT);
        const COLORREF colour = id == IdStatus && gHasRunError
                                    ? (gDarkTheme ? RGB(255, 153, 143) : RGB(176, 0, 32))
                                    : ((id == IdDescription || id == IdNotice || id == IdStatus)
                                           ? gTheme.muted : gTheme.text);
        SetTextColor(dc, colour);
        return reinterpret_cast<LRESULT>((id >= IdLanguageLabel && id <= IdStartMenuShortcut)
                                             ? gCardBrush : gBackgroundBrush);
    }
    case WM_CTLCOLOREDIT: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkColor(dc, gTheme.input);
        SetTextColor(dc, gTheme.text);
        return reinterpret_cast<LRESULT>(gEditBrush);
    }
    case WM_CTLCOLORBTN: {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(gCardBrush);
    }
    case WM_PAINT: {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        FillRect(dc, &client, gBackgroundBrush);
        HPEN border = CreatePen(PS_SOLID, 1, gTheme.border);
        HGDIOBJ oldPen = SelectObject(dc, border);
        HGDIOBJ oldBrush = SelectObject(dc, gCardBrush);
        RoundRect(dc, 26, 116, client.right - 26, 410, 16, 16);
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(border);

        HBRUSH inputBrush = CreateSolidBrush(gTheme.input);
        HPEN inputBorder = CreatePen(PS_SOLID, 1, gTheme.border);
        oldBrush = SelectObject(dc, inputBrush);
        oldPen = SelectObject(dc, inputBorder);
        RoundRect(dc, 50, 236, 526, 272, 9, 9);
        SelectObject(dc, oldPen);
        SelectObject(dc, oldBrush);
        DeleteObject(inputBorder);
        DeleteObject(inputBrush);

        HPEN separator = CreatePen(PS_SOLID, 1, gTheme.border);
        oldPen = SelectObject(dc, separator);
        MoveToEx(dc, 26, 455, nullptr);
        LineTo(dc, client.right - 26, 455);
        SelectObject(dc, oldPen);
        DeleteObject(separator);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_APP + 1:
        FinishInstall(static_cast<DWORD>(wParam));
        return 0;
    case WM_TIMER:
        if (wParam == 1 && gInstalling) {
            gProgressPulse += 7;
            InvalidateRect(Item(IdProgress), nullptr, FALSE);
            return 0;
        }
        break;
    case WM_SETTINGCHANGE:
    case WM_THEMECHANGED:
        ApplySystemTheme();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_CLOSE:
        if (!gInstalling) DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        if (!gExtractedMsi.empty()) DeleteFileW(gExtractedMsi.c_str());
        if (gTaskbar) {
            gTaskbar->SetProgressState(window, TBPF_NOPROGRESS);
            gTaskbar->Release();
            gTaskbar = nullptr;
        }
        if (gHeadingFont) DeleteObject(gHeadingFont);
        if (gSmallFont) DeleteObject(gSmallFont);
        if (gFont) DeleteObject(gFont);
        DeleteThemeBrushes();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

int RunQuiet() {
    const DWORD result = RunInstaller(false);
    if (!gExtractedMsi.empty()) {
        DeleteFileW(gExtractedMsi.c_str());
        gExtractedMsi.clear();
    }
    return (result == ERROR_SUCCESS || result == ERROR_SUCCESS_REBOOT_REQUIRED) ? 0 : static_cast<int>(result);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int showCommand) {
    gInstance = instance;
    const std::wstring arguments = commandLine ? commandLine : L"";
    if (arguments.find(L"--lang zh") != std::wstring::npos ||
        arguments.find(L"--lang=zh") != std::wstring::npos) {
        gLanguage = Language::Chinese;
        gLanguageForced = true;
    } else if (arguments.find(L"--lang en") != std::wstring::npos ||
               arguments.find(L"--lang=en") != std::wstring::npos) {
        gLanguage = Language::English;
        gLanguageForced = true;
    }
    if (arguments.find(L"--quiet") != std::wstring::npos) return RunQuiet();

    SetProcessDPIAware();
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    WNDCLASSEXW windowClass{sizeof(windowClass)};
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = reinterpret_cast<HICON>(LoadImageW(
        instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED));
    if (!windowClass.hIcon) windowClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    windowClass.lpszClassName = kWindowClass;
    windowClass.hIconSm = windowClass.hIcon;
    if (!RegisterClassExW(&windowClass)) return 1;

    WNDCLASSEXW licenceWindowClass = windowClass;
    licenceWindowClass.lpfnWndProc = LicenceWindowProc;
    licenceWindowClass.lpszClassName = kLicenceWindowClass;
    if (!RegisterClassExW(&licenceWindowClass)) return 1;

    constexpr int clientWidth = 720;
    constexpr int clientHeight = 531;
    constexpr DWORD windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    RECT windowBounds{0, 0, clientWidth, clientHeight};
    AdjustWindowRectEx(&windowBounds, windowStyle, FALSE, WS_EX_APPWINDOW);
    const int width = windowBounds.right - windowBounds.left;
    const int height = windowBounds.bottom - windowBounds.top;
    RECT desktop{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &desktop, 0);
    const int x = desktop.left + ((desktop.right - desktop.left) - width) / 2;
    const int y = desktop.top + ((desktop.bottom - desktop.top) - height) / 2;
    HWND window = CreateWindowExW(WS_EX_APPWINDOW, kWindowClass, L"Mangekyo Setup",
                                  windowStyle,
                                  x, y, width, height, nullptr, nullptr, instance, nullptr);
    if (!window) return 1;
    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (SUCCEEDED(comResult)) CoUninitialize();
    return static_cast<int>(message.wParam);
}
