// =============================================================================
// main.cpp - program entry point.
// -----------------------------------------------------------------------------
// Wires the layers together:
//   data store (persistence)  ->  RentalAgency (business logic)  ->  MainWindow (GUI)
//
// POLYMORPHISM: the data store is chosen at run time -
//   * RemoteDataStore   (shared online database) if "server.txt" next to the
//                       .exe contains the web address of the site, otherwise
//   * TextFileDataStore (text files next to the .exe).
// The agency and the window only see it through an IDataStore& reference.
//
// The user must log in before the main window opens.
// =============================================================================
#include "Win32Include.h"

#include <commctrl.h>

#include "IDataStore.h"
#include "LoginDialog.h"
#include "MainWindow.h"
#include "RentalAgency.h"
#include "RemoteDataStore.h"
#include "RentalExceptions.h"
#include "TextFileDataStore.h"
#include "TextUtils.h"

#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
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

    /// The web address in "server.txt" next to the .exe, or an empty text if there is none.
    std::wstring readServerAddress(const std::filesystem::path& directory)
    {
        std::ifstream input(directory / L"server.txt", std::ios::binary);
        if (!input)
        {
            return std::wstring();
        }
        const std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        const std::wstring text = TextUtils::trim(TextUtils::fromUtf8Lenient(bytes));
        return text.rfind(L"http", 0) == 0 ? text.substr(0, text.find_first_of(L" \r\n\t")) : std::wstring();
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
        // All objects are owned by RAII objects and destroyed automatically.
        const std::filesystem::path folder = executableDirectory();
        TextFileDataStore textFileStore(folder);
        const std::wstring serverAddress = readServerAddress(folder);
        std::unique_ptr<RemoteDataStore> remoteStore;
        if (!serverAddress.empty())
        {
            remoteStore = std::make_unique<RemoteDataStore>(serverAddress);
        }
        IDataStore& store = remoteStore ? static_cast<IDataStore&>(*remoteStore) : textFileStore; // only the interface from here on
        RentalAgency agency;

        std::vector<std::wstring> warnings;
        LoginDialog::LoginAction loginAction;
        std::wstring loginInfo;
        if (remoteStore)
        {
            // Shared online database: the server checks the account before it sends any data.
            loginAction = [&](const std::wstring& username, const std::wstring& password) {
                remoteStore->setCredentials(username, password);
                warnings = agency.loadFrom(store); // throws AuthenticationException for a wrong password
                if (remoteStore->wasEmpty())
                {
                    // Nothing online yet: start from the local data; the first save uploads it.
                    warnings = agency.loadFrom(textFileStore);
                    agency.markUnsaved();
                }
                agency.login(username, password);
            };
            loginInfo = L"Data: shared online database\n" + serverAddress +
                        L"\n\nFirst start: manager / manager123 or clerk / clerk123.";
        }
        else
        {
            try
            {
                warnings = agency.loadFrom(store); // missing files = empty data and the default accounts
            }
            catch (const RentalException& error)
            {
                warnings.push_back(L"The data files could not be read: " + error.message());
            }
            loginAction = [&](const std::wstring& username, const std::wstring& password) { agency.login(username, password); };
            loginInfo = L"Data: files in " + folder.wstring() +
                        (agency.usesDefaultPasswords() ? L"\n\nDefault accounts: manager / manager123 (Manager) and clerk / clerk123 (Clerk)."
                                                       : L"");
        }

        LoginDialog login(instance, loginAction, loginInfo);
        if (!login.showModal(nullptr))
        {
            return 0; // "Exit" in the login dialog
        }
        if (agency.usesDefaultPasswords() && agency.isAllowed(Permission::ManageUsers))
        {
            MessageBoxW(nullptr,
                        L"The default accounts still use their default passwords (manager123 / clerk123).\n\n"
                        L"Please change them in Users > Manage Users and save.",
                        L"Security reminder", MB_OK | MB_ICONWARNING);
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
