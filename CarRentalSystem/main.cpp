// =============================================================================
// main.cpp - program entry point.
// -----------------------------------------------------------------------------
// Wires the layers together:
//   TextFileDataStore (persistence)  ->  RentalAgency (business logic)  ->  MainWindow (GUI)
// The agency and the window only see the data store through an IDataStore&
// reference, so the storage technique can be replaced without changing them.
// =============================================================================
#include "Win32Include.h"

#include <commctrl.h>

#include "IDataStore.h"
#include "MainWindow.h"
#include "RentalAgency.h"
#include "RentalExceptions.h"
#include "TextFileDataStore.h"
#include "TextUtils.h"

#include <exception>
#include <filesystem>
#include <string>
#include <vector>

// Enable Visual Styles: use version 6 of the Common Controls (modern themed look).
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")

namespace
{
    /// Folder that contains the running .exe (the data files are stored there).
    std::filesystem::path executableDirectory()
    {
        std::wstring buffer(MAX_PATH, L'\0');
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
            {
                return std::filesystem::current_path();
            }
            if (length < buffer.size())
            {
                buffer.resize(length);
                break;
            }
            buffer.resize(buffer.size() * 2); // path longer than the buffer: try again
        }
        return std::filesystem::path(buffer).parent_path();
    }
}

int WINAPI wWinMain(_In_ HINSTANCE instance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int showCommand)
{
    // Crisp text on high-DPI monitors, re-scaled when moved between monitors.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES | ICC_LISTVIEW_CLASSES | ICC_TAB_CLASSES | ICC_BAR_CLASSES |
                     ICC_UPDOWN_CLASS;
    InitCommonControlsEx(&controls);

    try
    {
        // All objects live on the stack and are destroyed automatically (RAII).
        TextFileDataStore textFileStore(executableDirectory());
        IDataStore& store = textFileStore; // from here on only the interface is used
        RentalAgency agency;

        std::vector<std::wstring> warnings;
        try
        {
            warnings = agency.loadFrom(store); // missing files = empty data
        }
        catch (const RentalException& error)
        {
            warnings.push_back(L"The data files could not be read: " + error.message());
        }

        MainWindow window(instance, agency, store);
        if (!window.create(showCommand))
        {
            MessageBoxW(nullptr, L"The main window could not be created.", L"Car Rental Management System",
                        MB_OK | MB_ICONERROR);
            return 1;
        }
        window.showLoadWarnings(warnings);
        return window.runMessageLoop();
    }
    catch (const std::exception& error)
    {
        MessageBoxW(nullptr, TextUtils::fromUtf8Lenient(error.what()).c_str(), L"Unexpected error",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
}
