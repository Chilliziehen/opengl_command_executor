/**
 * @file NativeDialog_win32.cpp
 * @brief Win32 COM folder-selection dialog (IFileOpenDialog with FOS_PICKFOLDERS).
 */

#include "platform/NativeDialog.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shobjidl.h>   // IFileOpenDialog, IShellItem
#include <wrl/client.h> // Microsoft::WRL::ComPtr (available in Windows SDK)
#include <string>

namespace platform {

bool OpenFolderDialog(std::string &outPath) {
    // COM must already be initialised by the caller (GLFW does this on Windows).
    // We use COINIT_APARTMENTTHREADED which is what GLFW uses internally; if
    // we weren't already initialised the call below would fail harmlessly.

    using Microsoft::WRL::ComPtr;

    ComPtr<IFileOpenDialog> dlg;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dlg));
    if (FAILED(hr))
        return false;

    // FOS_PICKFOLDERS: pick a directory instead of files.
    DWORD flags = 0;
    dlg->GetOptions(&flags);
    dlg->SetOptions(flags | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);

    // Set a meaningful title.
    dlg->SetTitle(L"Select Capture Directory");

    // Default to "example/" relative to the project root, or the user's
    // Documents folder, whichever exists.
    // We just let Windows pick its default (usually Libraries/Documents).
    hr = dlg->Show(nullptr); // nullptr = no owner window (modeless not needed)
    if (FAILED(hr))
        return false; // user cancelled or error

    ComPtr<IShellItem> item;
    hr = dlg->GetResult(&item);
    if (FAILED(hr))
        return false;

    PWSTR rawPath = nullptr;
    hr = item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath);
    if (FAILED(hr))
        return false;

    // Convert wide string → UTF-8
    int len = ::WideCharToMultiByte(CP_UTF8, 0, rawPath, -1, nullptr, 0,
                                    nullptr, nullptr);
    if (len > 0) {
        std::string utf8(static_cast<size_t>(len - 1), '\0');
        ::WideCharToMultiByte(CP_UTF8, 0, rawPath, -1, utf8.data(), len,
                              nullptr, nullptr);
        outPath = utf8;
    }
    ::CoTaskMemFree(rawPath);
    return !outPath.empty();
}

} // namespace platform
