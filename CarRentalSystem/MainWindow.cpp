// =============================================================================
// MainWindow.cpp
// =============================================================================
#include "MainWindow.h"

#include "AddEditVehicleDialog.h"
#include "BusinessRuleSelfTest.h"
#include "IDataStore.h"
#include "RentDialog.h"
#include "RentalAgency.h"
#include "RentalExceptions.h"
#include "RentalStatistics.h"
#include "Resource.h"
#include "ReturnDialog.h"
#include "ChangePasswordDialog.h"
#include "LoginDialog.h"
#include "UsersDialog.h"
#include "TextUtils.h"

#include <commctrl.h>

#include <algorithm>
#include <utility>

namespace
{
    constexpr wchar_t MainWindowClassName[] = L"CarRentalSystem.MainWindow";
    constexpr wchar_t PageClassName[] = L"CarRentalSystem.TabPage";
    constexpr wchar_t ApplicationTitle[] = L"Car Rental Management System";

    constexpr int MinimumWidth = 1000;  // at 100% scaling
    constexpr int MinimumHeight = 650;
    constexpr int InitialWidth = 1120;
    constexpr int InitialHeight = 720;

    constexpr COLORREF AvailableRowColor = RGB(220, 245, 220); // light green
    constexpr COLORREF RentedRowColor = RGB(255, 226, 190);    // light orange
    constexpr COLORREF RevenueTextColor = RGB(0, 100, 50);
    constexpr COLORREF HintTextColor = RGB(90, 90, 90);
    constexpr wchar_t BulletCharacter = static_cast<wchar_t>(0x2022);

    /// Window procedure of the tab pages: a page is only a container, so it
    /// forwards the notifications of its child controls to the main window.
    LRESULT CALLBACK tabPageProc(HWND page, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_COMMAND:
        case WM_NOTIFY:
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN:
            return SendMessageW(GetParent(page), message, wParam, lParam);
        default:
            return DefWindowProcW(page, message, wParam, lParam);
        }
    }

    /// EnumChildWindows callback: applies the font to every control.
    BOOL CALLBACK applyFontToChild(HWND child, LPARAM font)
    {
        SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
        return TRUE;
    }

    std::wstring countText(std::size_t count, const wchar_t* singular, const wchar_t* plural)
    {
        return std::to_wstring(count) + L" " + (count == 1 ? singular : plural);
    }

    /// Status bar text must be a single line.
    std::wstring singleLine(std::wstring text)
    {
        std::replace(text.begin(), text.end(), L'\n', L' ');
        std::replace(text.begin(), text.end(), L'\r', L' ');
        return text;
    }

    ReportListView::Column makeColumn(const wchar_t* title, int weight, bool money)
    {
        ReportListView::Column column;
        column.title = title;
        column.weight = weight;
        column.rightAligned = money;
        column.numericSort = money;
        return column;
    }
}

// =============================================================================
// Construction and window procedure
// =============================================================================

MainWindow::MainWindow(HINSTANCE instance, RentalAgency& agency, IDataStore& store)
    : m_instance(instance), m_agency(agency), m_store(store)
{
}

bool MainWindow::registerWindowClasses(HINSTANCE instance)
{
    WNDCLASSEXW mainClass{};
    mainClass.cbSize = sizeof(mainClass);
    mainClass.lpfnWndProc = &MainWindow::windowProc;
    mainClass.hInstance = instance;
    mainClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    mainClass.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    mainClass.hIconSm = LoadIconW(nullptr, IDI_APPLICATION);
    mainClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE);
    mainClass.lpszClassName = MainWindowClassName;
    if (RegisterClassExW(&mainClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return false;
    }

    WNDCLASSEXW pageClass{};
    pageClass.cbSize = sizeof(pageClass);
    pageClass.lpfnWndProc = &tabPageProc;
    pageClass.hInstance = instance;
    pageClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    pageClass.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    pageClass.lpszClassName = PageClassName;
    if (RegisterClassExW(&pageClass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return false;
    }
    return true;
}

bool MainWindow::create(int showCommand)
{
    if (!registerWindowClasses(m_instance))
    {
        return false;
    }

    const UINT systemDpi = GetDpiForSystem();
    const HMENU menu = LoadMenuW(m_instance, MAKEINTRESOURCEW(IDR_MAINMENU));
    const HWND window = CreateWindowExW(0, MainWindowClassName, ApplicationTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                        CW_USEDEFAULT, CW_USEDEFAULT,
                                        Gui::scaleForDpi(InitialWidth, systemDpi),
                                        Gui::scaleForDpi(InitialHeight, systemDpi),
                                        nullptr, menu, m_instance, this);
    if (window == nullptr)
    {
        if (menu != nullptr)
        {
            DestroyMenu(menu);
        }
        return false;
    }

    m_accelerators = LoadAcceleratorsW(m_instance, MAKEINTRESOURCEW(IDR_ACCELERATORS));
    ShowWindow(window, showCommand);
    UpdateWindow(window);
    return true;
}

int MainWindow::runMessageLoop()
{
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        // 1) keyboard shortcuts (Ctrl+S, Ctrl+F, Ctrl+Tab ...)
        if (m_hwnd != nullptr && m_accelerators != nullptr &&
            TranslateAcceleratorW(m_hwnd, m_accelerators, &message) != 0)
        {
            continue;
        }
        // 2) dialog-style keyboard navigation: Tab, Shift+Tab, Enter, Alt+mnemonics
        if (m_hwnd != nullptr && IsDialogMessageW(m_hwnd, &message) != 0)
        {
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void MainWindow::showLoadWarnings(const std::vector<std::wstring>& warnings) const
{
    if (warnings.empty())
    {
        return;
    }
    constexpr std::size_t MaxListed = 20;
    std::wstring text = countText(warnings.size(), L"entry", L"entries") +
                        L" in the data files could not be loaded and " +
                        (warnings.size() == 1 ? L"was" : L"were") + L" skipped:\n\n";
    for (std::size_t index = 0; index < warnings.size() && index < MaxListed; ++index)
    {
        text += std::wstring(1, BulletCharacter) + L" " + warnings[index] + L"\n";
    }
    if (warnings.size() > MaxListed)
    {
        text += L"... and " + std::to_wstring(warnings.size() - MaxListed) + L" more.\n";
    }
    text += L"\nAll other data was loaded normally. Saving will rewrite the files without the skipped lines.";
    Gui::showMessage(m_hwnd, text, L"Some data was skipped", MB_ICONWARNING);
}

LRESULT CALLBACK MainWindow::windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    MainWindow* self = nullptr;
    if (message == WM_NCCREATE)
    {
        // The object pointer arrives in CREATESTRUCT::lpCreateParams; remember it.
        const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(creation->lpCreateParams);
        self->m_hwnd = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    }

    if (self == nullptr)
    {
        return DefWindowProcW(window, message, wParam, lParam);
    }
    return self->handleMessage(message, wParam, lParam);
}

LRESULT MainWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    // Central exception barrier: a C++ exception must never travel into Windows.
    // Every business-rule violation ends up here and is shown to the user.
    try
    {
        return routeMessage(message, wParam, lParam);
    }
    catch (const RentalException& error)
    {
        reportError(error.message());
    }
    catch (const std::exception& error)
    {
        reportError(TextUtils::fromUtf8Lenient(error.what()));
    }
    return 0;
}

LRESULT MainWindow::routeMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
        return onCreate() ? 0 : -1;

    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            layout();
        }
        return 0;

    case WM_GETMINMAXINFO:
    {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize.x = scale(MinimumWidth);
        info->ptMinTrackSize.y = scale(MinimumHeight);
        return 0;
    }

    case WM_DPICHANGED:
    {
        // The window moved to a monitor with a different scale factor:
        // recreate the fonts and use the size Windows suggests.
        m_dpi = HIWORD(wParam);
        applyFonts();
        const RECT* suggested = reinterpret_cast<const RECT*>(lParam);
        SetWindowPos(m_hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        layout();
        return 0;
    }

    case WM_ACTIVATE:
        // Remember / restore the focused control when switching applications.
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            m_lastFocus = GetFocus();
            return 0;
        }
        if (m_lastFocus != nullptr && IsWindow(m_lastFocus) && IsWindowEnabled(m_lastFocus) &&
            IsWindowVisible(m_lastFocus))
        {
            SetFocus(m_lastFocus);
        }
        else
        {
            focusCurrentPage(); // first activation: focus the main control of the visible tab
        }
        return 0;

    case WM_COMMAND:
        onCommand(LOWORD(wParam), HIWORD(wParam));
        return 0;

    case WM_NOTIFY:
        return onNotify(*reinterpret_cast<NMHDR*>(lParam));

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        return onControlColor(reinterpret_cast<HDC>(wParam), reinterpret_cast<HWND>(lParam));

    case WM_CLOSE:
        if (confirmSaveChanges())
        {
            DestroyWindow(m_hwnd);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_NCDESTROY:
    {
        const HWND window = m_hwnd;
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        m_hwnd = nullptr;
        return DefWindowProcW(window, message, wParam, lParam);
    }

    default:
        return DefWindowProcW(m_hwnd, message, wParam, lParam);
    }
}

// =============================================================================
// Creation and layout
// =============================================================================

bool MainWindow::onCreate()
{
    m_dpi = GetDpiForWindow(m_hwnd);
    createControls();
    applyFonts();
    refreshAll();
    setLastAction(L"Loaded " + countText(m_agency.getVehicleCount(), L"vehicle", L"vehicles") + L" from " +
                  m_store.describeLocation());
    showPage(FleetTab);
    layout();
    return m_tab != nullptr && m_statusBar != nullptr;
}

void MainWindow::createControls()
{
    // Tab control. WS_CLIPSIBLINGS stops it from painting over the pages.
    m_tab = Gui::createChild(m_hwnd, m_instance, WC_TABCONTROLW, L"",
                             WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP, 0, IDC_TAB);
    const std::array<const wchar_t*, TabCount> titles{ L"Fleet", L"Search && Rent", L"Rentals && Returns",
                                                       L"Rental History", L"Reports" };
    for (int index = 0; index < TabCount; ++index)
    {
        std::wstring title = titles[static_cast<std::size_t>(index)];
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = title.data();
        SendMessageW(m_tab, TCM_INSERTITEMW, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(&item));
    }

    // One container window per tab. WS_EX_CONTROLPARENT lets Tab/Shift+Tab
    // navigate into the controls of the visible page.
    for (int index = 0; index < TabCount; ++index)
    {
        m_pages[static_cast<std::size_t>(index)] =
            Gui::createChild(m_hwnd, m_instance, PageClassName, L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                             WS_EX_CONTROLPARENT, IDC_PAGE_FIRST + index);
    }
    createFleetPage(page(FleetTab));
    createSearchPage(page(SearchTab));
    createRentalsPage(page(RentalsTab));
    createHistoryPage(page(HistoryTab));
    createReportsPage(page(ReportsTab));

    m_statusBar = Gui::createChild(m_hwnd, m_instance, STATUSCLASSNAMEW, L"",
                                   WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, IDC_STATUS_BAR);

    // Pages must be above the tab control in the Z order.
    SetWindowPos(m_tab, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void MainWindow::createFleetPage(HWND fleetPage)
{
    // Search box + status filter
    m_fleetSearchLabel = Gui::createChild(fleetPage, m_instance, L"STATIC", L"Find &vehicle:",
                                          WS_CHILD | WS_VISIBLE | SS_LEFT, 0, IDC_FLEET_SEARCH_LABEL);
    m_fleetSearch = Gui::createChild(fleetPage, m_instance, L"EDIT", L"",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE,
                                     IDC_FLEET_SEARCH);
    SendMessageW(m_fleetSearch, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Plate or model..."));
    SendMessageW(m_fleetSearch, EM_LIMITTEXT, 60, 0);
    m_fleetFilter = Gui::createChild(fleetPage, m_instance, WC_COMBOBOXW, L"",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 0,
                                     IDC_FLEET_FILTER);
    for (const wchar_t* option : { L"All vehicles", L"Available only", L"Rented only" })
    {
        SendMessageW(m_fleetFilter, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(option));
    }
    SendMessageW(m_fleetFilter, CB_SETCURSEL, 0, 0);

    m_fleetHint = Gui::createChild(fleetPage, m_instance, L"STATIC",
        L"Green = Available, orange = Rented. Double-click (or Enter) edits, Delete removes an available vehicle.",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, 0, IDC_FLEET_HINT);

    std::vector<ReportListView::Column> columns;
    columns.push_back(makeColumn(L"Plate", 2, false));
    columns.push_back(makeColumn(L"Model", 5, false));
    columns.push_back(makeColumn(L"Daily Rate", 2, true));
    columns.push_back(makeColumn(L"Status", 2, false));
    m_fleetList.create(fleetPage, IDC_FLEET_LIST, m_instance, columns);

    m_addButton = Gui::createChild(fleetPage, m_instance, L"BUTTON", L"&Add Vehicle...",
                                   WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_ADD_BUTTON);
    m_editButton = Gui::createChild(fleetPage, m_instance, L"BUTTON", L"&Edit Selected...",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_EDIT_BUTTON);
    m_removeButton = Gui::createChild(fleetPage, m_instance, L"BUTTON", L"Re&move Selected...",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_REMOVE_BUTTON);
}

void MainWindow::createReportsPage(HWND reportsPage)
{
    m_reportsSummary = Gui::createChild(reportsPage, m_instance, L"STATIC", L"",
                                        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, IDC_REPORTS_SUMMARY);

    m_modelLabel = Gui::createChild(reportsPage, m_instance, L"STATIC", L"Revenue by model",
                                    WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, IDC_MODEL_LABEL);
    std::vector<ReportListView::Column> modelColumns;
    modelColumns.push_back(makeColumn(L"Model", 4, false));
    modelColumns.push_back(makeColumn(L"Rentals", 2, true));
    modelColumns.push_back(makeColumn(L"Days", 2, true));
    modelColumns.push_back(makeColumn(L"Revenue", 3, true));
    m_modelList.create(reportsPage, IDC_MODEL_LIST, m_instance, modelColumns);

    m_topVehicleLabel = Gui::createChild(reportsPage, m_instance, L"STATIC", L"Most rented vehicles",
                                         WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX, 0, IDC_TOPVEHICLE_LABEL);
    std::vector<ReportListView::Column> vehicleColumns;
    vehicleColumns.push_back(makeColumn(L"Plate", 2, false));
    vehicleColumns.push_back(makeColumn(L"Model", 4, false));
    vehicleColumns.push_back(makeColumn(L"Rentals", 2, true));
    vehicleColumns.push_back(makeColumn(L"Revenue", 3, true));
    m_topVehicleList.create(reportsPage, IDC_TOPVEHICLE_LIST, m_instance, vehicleColumns);
}

void MainWindow::createSearchPage(HWND searchPage)
{
    m_searchLabel = Gui::createChild(searchPage, m_instance, L"STATIC", L"Search &model:",
                                     WS_CHILD | WS_VISIBLE | SS_LEFT, 0, IDC_SEARCH_LABEL);
    m_searchEdit = Gui::createChild(searchPage, m_instance, L"EDIT", L"",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE,
                                    IDC_SEARCH_EDIT);
    SendMessageW(m_searchEdit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Search model..."));
    SendMessageW(m_searchEdit, EM_LIMITTEXT, 60, 0);

    std::vector<ReportListView::Column> columns;
    columns.push_back(makeColumn(L"Plate", 2, false));
    columns.push_back(makeColumn(L"Model", 6, false));
    columns.push_back(makeColumn(L"Daily Rate", 2, true));
    m_searchList.create(searchPage, IDC_SEARCH_LIST, m_instance, columns);

    m_searchCount = Gui::createChild(searchPage, m_instance, L"STATIC", L"",
                                     WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, 0,
                                     IDC_SEARCH_COUNT);
    m_rentButton = Gui::createChild(searchPage, m_instance, L"BUTTON", L"&Rent Selected...",
                                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_RENT_BUTTON);
}

void MainWindow::createRentalsPage(HWND rentalsPage)
{
    m_revenueLabel = Gui::createChild(rentalsPage, m_instance, L"STATIC", L"",
                                      WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_CENTERIMAGE, 0,
                                      IDC_REVENUE_LABEL);

    std::vector<ReportListView::Column> columns;
    columns.push_back(makeColumn(L"Plate", 2, false));
    columns.push_back(makeColumn(L"Model", 3, false));
    columns.push_back(makeColumn(L"Renter", 3, false));
    columns.push_back(makeColumn(L"Telephone", 3, false));
    columns.push_back(makeColumn(L"Rented Since", 3, false));
    columns.push_back(makeColumn(L"Agreed Rate", 2, true));
    m_rentalsList.create(rentalsPage, IDC_RENTALS_LIST, m_instance, columns);

    m_rentalsHint = Gui::createChild(rentalsPage, m_instance, L"STATIC",
        L"Select a rented vehicle and click Return Selected... to enter the rental days and print the receipt.",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, 0, IDC_RENTALS_HINT);
    m_returnButton = Gui::createChild(rentalsPage, m_instance, L"BUTTON", L"Re&turn Selected...",
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON, 0, IDC_RETURN_BUTTON);
}

void MainWindow::createHistoryPage(HWND historyPage)
{
    m_historySummary = Gui::createChild(historyPage, m_instance, L"STATIC", L"",
                                        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_CENTERIMAGE, 0,
                                        IDC_HISTORY_SUMMARY);
    m_historyLabel = Gui::createChild(historyPage, m_instance, L"STATIC", L"Searc&h history:",
                                      WS_CHILD | WS_VISIBLE | SS_LEFT, 0, IDC_HISTORY_LABEL);
    m_historySearch = Gui::createChild(historyPage, m_instance, L"EDIT", L"",
                                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE,
                                       IDC_HISTORY_SEARCH);
    SendMessageW(m_historySearch, EM_SETCUEBANNER, TRUE,
                 reinterpret_cast<LPARAM>(L"Plate, model, renter or telephone..."));
    SendMessageW(m_historySearch, EM_LIMITTEXT, 80, 0);

    std::vector<ReportListView::Column> columns;
    columns.push_back(makeColumn(L"No.", 1, true));
    columns.push_back(makeColumn(L"Returned", 3, false));
    columns.push_back(makeColumn(L"Plate", 2, false));
    columns.push_back(makeColumn(L"Model", 3, false));
    columns.push_back(makeColumn(L"Renter", 3, false));
    columns.push_back(makeColumn(L"Telephone", 3, false));
    columns.push_back(makeColumn(L"Rate", 2, true));
    columns.push_back(makeColumn(L"Days", 1, true));
    columns.push_back(makeColumn(L"Charge", 2, true));
    m_historyList.create(historyPage, IDC_HISTORY_LIST, m_instance, columns);

    m_historyHint = Gui::createChild(historyPage, m_instance, L"STATIC",
        L"Every return is archived here and saved to history.txt. Double-click a record (or press Enter) to show its receipt again.",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX | SS_ENDELLIPSIS, 0, IDC_HISTORY_HINT);
}

void MainWindow::applyFonts()
{
    // Create the new fonts first, apply them, and only then release the old ones
    // (the controls must never refer to a deleted font).
    Gui::FontHandle font = Gui::createUiFont(9, false, m_dpi);
    Gui::FontHandle titleFont = Gui::createUiFont(14, true, m_dpi);

    EnumChildWindows(m_hwnd, &applyFontToChild, reinterpret_cast<LPARAM>(font.get()));
    SendMessageW(m_revenueLabel, WM_SETFONT, reinterpret_cast<WPARAM>(titleFont.get()), TRUE);

    m_font = std::move(font);
    m_titleFont = std::move(titleFont);
}

int MainWindow::scale(int value) const noexcept
{
    return Gui::scaleForDpi(value, m_dpi);
}

void MainWindow::layout()
{
    if (m_tab == nullptr || m_statusBar == nullptr)
    {
        return;
    }

    RECT client{};
    GetClientRect(m_hwnd, &client);

    // The status bar positions itself at the bottom when it receives WM_SIZE.
    SendMessageW(m_statusBar, WM_SIZE, 0, 0);
    updateStatusBarParts();
    RECT statusRect{};
    GetWindowRect(m_statusBar, &statusRect);
    const int statusHeight = statusRect.bottom - statusRect.top;

    const int margin = scale(8);
    RECT tabRect{ margin, margin, client.right - margin, client.bottom - statusHeight - margin };
    if (tabRect.right <= tabRect.left || tabRect.bottom <= tabRect.top)
    {
        return;
    }
    MoveWindow(m_tab, tabRect.left, tabRect.top, tabRect.right - tabRect.left, tabRect.bottom - tabRect.top, TRUE);

    // Ask the tab control for its display area (the rectangle below the tab headers).
    RECT pageRect = tabRect;
    SendMessageW(m_tab, TCM_ADJUSTRECT, FALSE, reinterpret_cast<LPARAM>(&pageRect));
    const int pageWidth = std::max(0, static_cast<int>(pageRect.right - pageRect.left));
    const int pageHeight = std::max(0, static_cast<int>(pageRect.bottom - pageRect.top));
    for (const HWND tabPage : m_pages)
    {
        MoveWindow(tabPage, pageRect.left, pageRect.top, pageWidth, pageHeight, TRUE);
    }

    layoutFleetPage(pageWidth, pageHeight);
    layoutSearchPage(pageWidth, pageHeight);
    layoutRentalsPage(pageWidth, pageHeight);
    layoutHistoryPage(pageWidth, pageHeight);
    layoutReportsPage(pageWidth, pageHeight);
}

void MainWindow::layoutFleetPage(int width, int height)
{
    const int padding = scale(10);
    const int gap = scale(8);
    const int labelWidth = scale(90);
    const int labelHeight = scale(20);
    const int editHeight = scale(26);
    const int buttonWidth = scale(150);
    const int buttonHeight = scale(30);

    // Row 1: Find vehicle [ search ] [ filter v ]
    MoveWindow(m_fleetSearchLabel, padding, padding + (editHeight - labelHeight) / 2, labelWidth, labelHeight, TRUE);
    const int editLeft = padding + labelWidth + gap;
    const int editWidth = std::max(0, std::min(scale(300), width - editLeft - scale(170) - 2 * padding));
    MoveWindow(m_fleetSearch, editLeft, padding, editWidth, editHeight, TRUE);
    // A drop-down combo box's height includes its open list.
    MoveWindow(m_fleetFilter, editLeft + editWidth + gap, padding, scale(160), scale(200), TRUE);

    // Row 2: the list
    const int listTop = padding + editHeight + gap;
    const int buttonTop = height - padding - buttonHeight;
    MoveWindow(m_fleetList.handle(), padding, listTop, std::max(0, width - 2 * padding),
               std::max(0, buttonTop - gap - listTop), TRUE);
    m_fleetList.fitColumnsToWidth();

    // Row 3: buttons, then the hint
    MoveWindow(m_addButton, padding, buttonTop, buttonWidth, buttonHeight, TRUE);
    MoveWindow(m_editButton, padding + buttonWidth + gap, buttonTop, buttonWidth, buttonHeight, TRUE);
    MoveWindow(m_removeButton, padding + 2 * (buttonWidth + gap), buttonTop, buttonWidth, buttonHeight, TRUE);
    const int hintLeft = padding + 3 * (buttonWidth + gap) + gap;
    MoveWindow(m_fleetHint, hintLeft, buttonTop + (buttonHeight - labelHeight) / 2,
               std::max(0, width - hintLeft - padding), labelHeight, TRUE);
}

void MainWindow::layoutReportsPage(int width, int height)
{
    const int padding = scale(10);
    const int gap = scale(12);
    const int summaryHeight = scale(96);
    const int labelHeight = scale(22);

    MoveWindow(m_reportsSummary, padding, padding, std::max(0, width - 2 * padding), summaryHeight, TRUE);

    const int top = padding + summaryHeight + gap;
    const int columnWidth = std::max(0, (width - 2 * padding - gap) / 2);
    const int listTop = top + labelHeight;
    const int listHeight = std::max(0, height - padding - listTop);

    MoveWindow(m_modelLabel, padding, top, columnWidth, labelHeight, TRUE);
    MoveWindow(m_modelList.handle(), padding, listTop, columnWidth, listHeight, TRUE);
    m_modelList.fitColumnsToWidth();

    const int rightLeft = padding + columnWidth + gap;
    MoveWindow(m_topVehicleLabel, rightLeft, top, columnWidth, labelHeight, TRUE);
    MoveWindow(m_topVehicleList.handle(), rightLeft, listTop, columnWidth, listHeight, TRUE);
    m_topVehicleList.fitColumnsToWidth();
}

void MainWindow::layoutSearchPage(int width, int height)
{
    const int padding = scale(10);
    const int gap = scale(8);
    const int labelWidth = scale(100);
    const int labelHeight = scale(20);
    const int editHeight = scale(26);
    const int buttonWidth = scale(150);
    const int buttonHeight = scale(30);

    MoveWindow(m_searchLabel, padding, padding + (editHeight - labelHeight) / 2, labelWidth, labelHeight, TRUE);
    const int editLeft = padding + labelWidth + gap;
    MoveWindow(m_searchEdit, editLeft, padding, std::max(0, std::min(scale(380), width - editLeft - padding)),
               editHeight, TRUE);

    const int listTop = padding + editHeight + gap;
    const int buttonTop = height - padding - buttonHeight;
    MoveWindow(m_searchList.handle(), padding, listTop, std::max(0, width - 2 * padding),
               std::max(0, buttonTop - gap - listTop), TRUE);
    m_searchList.fitColumnsToWidth();

    MoveWindow(m_searchCount, padding, buttonTop + (buttonHeight - labelHeight) / 2,
               std::max(0, width - 3 * padding - buttonWidth), labelHeight, TRUE);
    MoveWindow(m_rentButton, width - padding - buttonWidth, buttonTop, buttonWidth, buttonHeight, TRUE);
}

void MainWindow::layoutRentalsPage(int width, int height)
{
    const int padding = scale(10);
    const int gap = scale(8);
    const int revenueHeight = scale(36);
    const int labelHeight = scale(20);
    const int buttonWidth = scale(150);
    const int buttonHeight = scale(30);

    MoveWindow(m_revenueLabel, padding, padding, std::max(0, width - 2 * padding), revenueHeight, TRUE);

    const int listTop = padding + revenueHeight + gap;
    const int buttonTop = height - padding - buttonHeight;
    MoveWindow(m_rentalsList.handle(), padding, listTop, std::max(0, width - 2 * padding),
               std::max(0, buttonTop - gap - listTop), TRUE);
    m_rentalsList.fitColumnsToWidth();

    MoveWindow(m_rentalsHint, padding, buttonTop + (buttonHeight - labelHeight) / 2,
               std::max(0, width - 3 * padding - buttonWidth), labelHeight, TRUE);
    MoveWindow(m_returnButton, width - padding - buttonWidth, buttonTop, buttonWidth, buttonHeight, TRUE);
}

void MainWindow::layoutHistoryPage(int width, int height)
{
    const int padding = scale(10);
    const int gap = scale(8);
    const int summaryHeight = scale(24);
    const int labelWidth = scale(100);
    const int labelHeight = scale(20);
    const int editHeight = scale(26);

    MoveWindow(m_historySummary, padding, padding, std::max(0, width - 2 * padding), summaryHeight, TRUE);

    const int searchTop = padding + summaryHeight + gap;
    MoveWindow(m_historyLabel, padding, searchTop + (editHeight - labelHeight) / 2, labelWidth, labelHeight, TRUE);
    const int editLeft = padding + labelWidth + gap;
    MoveWindow(m_historySearch, editLeft, searchTop, std::max(0, std::min(scale(380), width - editLeft - padding)),
               editHeight, TRUE);

    const int listTop = searchTop + editHeight + gap;
    const int hintTop = height - padding - labelHeight;
    MoveWindow(m_historyList.handle(), padding, listTop, std::max(0, width - 2 * padding),
               std::max(0, hintTop - gap - listTop), TRUE);
    m_historyList.fitColumnsToWidth();

    MoveWindow(m_historyHint, padding, hintTop, std::max(0, width - 2 * padding), labelHeight, TRUE);
}

void MainWindow::updateStatusBarParts()
{
    // Right edges of the parts; -1 = extend to the right border.
    int edges[5] = {};
    int x = 0;
    x += scale(150); edges[0] = x; // total vehicles
    x += scale(120); edges[1] = x; // available
    x += scale(110); edges[2] = x; // rented
    x += scale(230); edges[3] = x; // revenue
    edges[4] = -1;                 // last action
    SendMessageW(m_statusBar, SB_SETPARTS, 5, reinterpret_cast<LPARAM>(edges));
}

// =============================================================================
// Events
// =============================================================================

void MainWindow::onCommand(int commandId, int notificationCode)
{
    switch (commandId)
    {
    case ID_FILE_SAVE:
        onSave();
        break;
    case ID_FILE_EXIT:
        SendMessageW(m_hwnd, WM_CLOSE, 0, 0);
        break;
    case ID_HELP_ABOUT:
        onAbout();
        break;
    case ID_FILE_RELOAD:
        onReload();
        break;
    case ID_FILE_LOGOUT:
        onLogout();
        break;
    case ID_USERS_MANAGE:
        onManageUsers();
        break;
    case ID_USERS_PASSWORD:
        onChangePassword();
        break;
    case ID_HELP_SELFTEST:
        onSelfTest();
        break;
    case ID_NEXT_TAB:
        selectTab((currentTab() + 1) % TabCount);
        break;
    case ID_PREV_TAB:
        selectTab((currentTab() + TabCount - 1) % TabCount);
        break;
    case ID_FIND:
        selectTab(SearchTab);
        SendMessageW(m_searchEdit, EM_SETSEL, 0, -1);
        break;
    case IDOK: // Enter pressed (sent by IsDialogMessage)
        onDefaultAction();
        break;
    case IDC_ADD_BUTTON:
        if (notificationCode == BN_CLICKED)
        {
            onAddVehicle();
        }
        break;
    case IDC_EDIT_BUTTON:
        if (notificationCode == BN_CLICKED)
        {
            onEditVehicle();
        }
        break;
    case IDC_REMOVE_BUTTON:
        if (notificationCode == BN_CLICKED)
        {
            onRemoveVehicle();
        }
        break;
    case IDC_FLEET_SEARCH:
        if (notificationCode == EN_CHANGE)
        {
            refreshFleet();
            updateButtons();
        }
        break;
    case IDC_FLEET_FILTER:
        if (notificationCode == CBN_SELCHANGE)
        {
            refreshFleet();
            updateButtons();
        }
        break;
    case IDC_RENT_BUTTON:
        if (notificationCode == BN_CLICKED)
        {
            onRentVehicle();
        }
        break;
    case IDC_RETURN_BUTTON:
        if (notificationCode == BN_CLICKED)
        {
            onReturnVehicle();
        }
        break;
    case IDC_SEARCH_EDIT:
        if (notificationCode == EN_CHANGE)
        {
            refreshSearch(); // filter as you type
            updateButtons();
        }
        break;
    case IDC_HISTORY_SEARCH:
        if (notificationCode == EN_CHANGE)
        {
            refreshHistory();
        }
        break;
    default:
        break;
    }
}

LRESULT MainWindow::onNotify(NMHDR& header)
{
    switch (header.idFrom)
    {
    case IDC_TAB:
        if (header.code == TCN_SELCHANGE)
        {
            showPage(currentTab());
        }
        return 0;
    case IDC_FLEET_LIST:
        if (header.code == LVN_KEYDOWN && reinterpret_cast<const NMLVKEYDOWN&>(header).wVKey == VK_DELETE)
        {
            onRemoveVehicle(); // Delete key removes the selected vehicle
            return 0;
        }
        return handleListNotification(m_fleetList, header, &MainWindow::onEditVehicle);
    case IDC_MODEL_LIST:
        return handleListNotification(m_modelList, header, nullptr);
    case IDC_TOPVEHICLE_LIST:
        return handleListNotification(m_topVehicleList, header, nullptr);
    case IDC_SEARCH_LIST:
        return handleListNotification(m_searchList, header, &MainWindow::onRentVehicle);
    case IDC_RENTALS_LIST:
        return handleListNotification(m_rentalsList, header, &MainWindow::onReturnVehicle);
    case IDC_HISTORY_LIST:
        return handleListNotification(m_historyList, header, &MainWindow::onShowHistoryReceipt);
    default:
        return 0;
    }
}

LRESULT MainWindow::handleListNotification(ReportListView& list, NMHDR& header, void (MainWindow::*activate)())
{
    switch (header.code)
    {
    case NM_CUSTOMDRAW:
        return list.handleCustomDraw(reinterpret_cast<NMLVCUSTOMDRAW&>(header));
    case LVN_ITEMCHANGED:
        updateButtons();
        return 0;
    case LVN_COLUMNCLICK:
        list.sortByColumn(reinterpret_cast<const NMLISTVIEW&>(header).iSubItem);
        return 0;
    case NM_DBLCLK:
        if (activate != nullptr && list.selectedRow() != nullptr)
        {
            (this->*activate)();
        }
        return 0;
    default:
        return 0;
    }
}

LRESULT MainWindow::onControlColor(HDC deviceContext, HWND control) const
{
    // Labels and buttons on the white tab pages get a white background.
    SetBkColor(deviceContext, GetSysColor(COLOR_WINDOW));
    if (control == m_revenueLabel)
    {
        SetTextColor(deviceContext, RevenueTextColor);
    }
    else if (control == m_fleetHint || control == m_rentalsHint || control == m_searchCount ||
             control == m_historyHint)
    {
        SetTextColor(deviceContext, HintTextColor);
    }
    else if (control == m_modelLabel || control == m_topVehicleLabel)
    {
        SetTextColor(deviceContext, RevenueTextColor);
    }
    else
    {
        SetTextColor(deviceContext, GetSysColor(COLOR_WINDOWTEXT));
    }
    return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
}

void MainWindow::onDefaultAction()
{
    switch (currentTab())
    {
    case FleetTab:
        onEditVehicle();
        break;
    case SearchTab:
        onRentVehicle();
        break;
    case RentalsTab:
        onReturnVehicle();
        break;
    case HistoryTab:
        onShowHistoryReceipt();
        break;
    default:
        break;
    }
}

// =============================================================================
// Use cases - each one calls the dialog / RentalAgency and refreshes the view.
// Exceptions thrown by RentalAgency are caught in handleMessage().
// =============================================================================

void MainWindow::onAddVehicle()
{
    m_agency.requirePermission(Permission::ManageFleet); // checked before any dialog opens
    AddEditVehicleDialog dialog(m_instance, m_agency);
    if (dialog.showModal(m_hwnd))
    {
        const std::wstring plate = dialog.savedPlate();
        refreshAll();
        selectTab(FleetTab);
        m_fleetList.selectKey(plate);
        setLastAction(L"Added vehicle " + plate + L".");
    }
}

void MainWindow::onEditVehicle()
{
    m_agency.requirePermission(Permission::ManageFleet); // checked before any dialog opens
    const ReportListView::Row* row = m_fleetList.selectedRow();
    if (row == nullptr)
    {
        setLastAction(L"Select a vehicle on the Fleet tab first.");
        return;
    }
    const std::wstring plate = row->key;

    // Throws VehicleNotAvailableException for a rented vehicle (e.g. on double-click).
    const Vehicle& vehicle = m_agency.findEditableVehicle(plate);
    AddEditVehicleDialog dialog(m_instance, m_agency, vehicle);
    if (dialog.showModal(m_hwnd))
    {
        refreshAll();
        m_fleetList.selectKey(plate);
        setLastAction(L"Updated vehicle " + plate + L".");
    }
}

void MainWindow::onRemoveVehicle()
{
    m_agency.requirePermission(Permission::ManageFleet); // checked before any dialog opens
    const ReportListView::Row* row = m_fleetList.selectedRow();
    if (row == nullptr)
    {
        setLastAction(L"Select a vehicle on the Fleet tab first.");
        return;
    }
    const std::wstring plate = row->key;
    const Vehicle& vehicle = m_agency.findVehicle(plate);
    if (!vehicle.isAvailable())
    {
        m_agency.removeVehicle(plate); // throws VehicleNotAvailableException with the business message
    }

    const std::wstring question = L"Remove vehicle " + plate + L" (" + vehicle.getModel() +
                                  L") from the fleet?\n\nIts completed rentals stay in the rental history.";
    if (MessageBoxW(m_hwnd, question.c_str(), L"Remove vehicle", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
    {
        return;
    }
    m_agency.removeVehicle(plate);
    refreshAll();
    setLastAction(L"Removed vehicle " + plate + L".");
}

void MainWindow::onRentVehicle()
{
    m_agency.requirePermission(Permission::RentAndReturn); // checked before any dialog opens
    const ReportListView::Row* row = m_searchList.selectedRow();
    if (row == nullptr)
    {
        setLastAction(L"Select an available vehicle on the Search & Rent tab first.");
        return;
    }
    const std::wstring plate = row->key;

    const Vehicle& vehicle = m_agency.findRentableVehicle(plate); // throws if already rented
    RentDialog dialog(m_instance, m_agency, vehicle);
    if (dialog.showModal(m_hwnd))
    {
        refreshAll();
        setLastAction(L"Rented " + plate + L" to " + dialog.renterName() + L".");
    }
}

void MainWindow::onReturnVehicle()
{
    m_agency.requirePermission(Permission::RentAndReturn); // checked before any dialog opens
    const ReportListView::Row* row = m_rentalsList.selectedRow();
    if (row == nullptr)
    {
        setLastAction(L"Select a rented vehicle on the Rentals & Returns tab first.");
        return;
    }
    const std::wstring plate = row->key;

    const Vehicle& vehicle = m_agency.findRentedVehicle(plate); // throws if not rented
    ReturnDialog dialog(m_instance, m_agency, vehicle, m_agency.suggestedRentalDays(plate));
    if (dialog.showModal(m_hwnd) && dialog.receipt().has_value())
    {
        const Receipt& receipt = *dialog.receipt();
        const RentalRecord& record = m_agency.getLastRecord(); // the return was archived
        refreshAll();
        setLastAction(L"Returned " + plate + L" - charged " + receipt.getTotalCharge().toDisplayString() +
                      L" (receipt no. " + std::to_wstring(record.getNumber()) + L").");
        Gui::showMessage(m_hwnd,
                         L"Receipt no. " + std::to_wstring(record.getNumber()) + L"   -   " + record.getReturnedOn() +
                             L"\n\n" + receipt.toDisplayText(),
                         L"Return Receipt", MB_ICONINFORMATION);
    }
}

void MainWindow::onShowHistoryReceipt()
{
    const ReportListView::Row* row = m_historyList.selectedRow();
    if (row == nullptr)
    {
        setLastAction(L"Select a record on the Rental History tab first.");
        return;
    }
    const std::wstring number = row->key;
    for (const RentalRecord& record : m_agency.getHistory())
    {
        if (std::to_wstring(record.getNumber()) == number)
        {
            Gui::showMessage(m_hwnd,
                             L"Receipt no. " + number + L"   -   " + record.getReturnedOn() + L"\n\n" +
                                 record.getReceipt().toDisplayText(),
                             L"Receipt (copy)", MB_ICONINFORMATION);
            return;
        }
    }
}

bool MainWindow::onSave()
{
    try
    {
        m_agency.saveTo(m_store); // polymorphic: works with any IDataStore
    }
    catch (const RentalException& error)
    {
        reportError(L"The data could not be saved.\n\n" + error.message());
        return false;
    }
    updateTitle();
    setLastAction(L"Saved " + countText(m_agency.getVehicleCount(), L"vehicle", L"vehicles") +
                  L" and revenue " + m_agency.getRevenue().toDisplayString() + L".");
    return true;
}

void MainWindow::onReload()
{
    if (m_agency.hasUnsavedChanges() &&
        MessageBoxW(m_hwnd, L"Reloading discards your unsaved changes.\n\nReload anyway?", L"Reload",
                    MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
    {
        return;
    }
    const std::vector<std::wstring> warnings = m_agency.loadFrom(m_store); // polymorphic: files or online database
    refreshAll();
    setLastAction(L"Reloaded " + countText(m_agency.getVehicleCount(), L"vehicle", L"vehicles") + L" from " +
                  m_store.describeLocation() + L".");
    showLoadWarnings(warnings);
    if (m_agency.getCurrentUser() == nullptr && !showLoginDialog())
    {
        DestroyWindow(m_hwnd); // the account was deleted by somebody else and nobody logged in
    }
}

void MainWindow::onLogout()
{
    if (!confirmSaveChanges())
    {
        return;
    }
    m_agency.logout();
    refreshAll();
    if (!showLoginDialog())
    {
        DestroyWindow(m_hwnd);
        return;
    }
    const User* user = m_agency.getCurrentUser();
    setLastAction(L"Logged in as " + (user != nullptr ? user->getUsername() : std::wstring()) + L".");
}

bool MainWindow::showLoginDialog()
{
    LoginDialog dialog(m_instance,
                       [this](const std::wstring& username, const std::wstring& password) { m_agency.login(username, password); },
                       L"Log in to continue working with the data in " + m_store.describeLocation() + L".");
    const bool loggedIn = dialog.showModal(m_hwnd);
    refreshAll();
    return loggedIn;
}

void MainWindow::onManageUsers()
{
    m_agency.requirePermission(Permission::ManageUsers); // Clerks get "Access denied"
    UsersDialog dialog(m_instance, m_agency);
    dialog.showModal(m_hwnd);
    if (dialog.changed())
    {
        refreshAll();
        setLastAction(L"User accounts changed. Save (Ctrl+S) to keep the changes.");
    }
}

void MainWindow::onChangePassword()
{
    ChangePasswordDialog dialog(m_instance, m_agency);
    if (dialog.showModal(m_hwnd))
    {
        refreshAll();
        setLastAction(L"Your password was changed. Save (Ctrl+S) to keep the change.");
    }
}

bool MainWindow::confirmSaveChanges()
{
    if (!m_agency.hasUnsavedChanges())
    {
        return true;
    }
    const int answer = MessageBoxW(m_hwnd, L"You have unsaved changes.\n\nSave changes before exiting?",
                                   L"Save changes?", MB_YESNOCANCEL | MB_ICONQUESTION);
    switch (answer)
    {
    case IDYES:
        return onSave(); // stay open if saving fails
    case IDNO:
        return true;
    default:
        return false; // Cancel
    }
}

void MainWindow::onAbout() const
{
    const std::wstring text =
        L"Car Rental Management System\nVersion 1.0\n\n"
        L"Object-oriented C++17 / Win32 course project.\n\n"
        L"Data folder:\n" + m_store.describeLocation() + L"\n\n"
        L"Keyboard shortcuts:\n"
        L"Ctrl+S\tSave\n"
        L"Ctrl+F\tSearch models\n"
        L"Ctrl+Tab\tNext tab (Ctrl+Shift+Tab: previous)\n"
        L"Enter\tEdit / Rent / Return the selected vehicle";
    Gui::showMessage(m_hwnd, text, L"About", MB_ICONINFORMATION);
}

void MainWindow::onSelfTest() const
{
    const BusinessRuleSelfTest selfTest{};
    const std::vector<SelfTestResult> results = selfTest.run();

    std::size_t passed = 0;
    std::wstring lines;
    for (const SelfTestResult& result : results)
    {
        if (result.passed)
        {
            ++passed;
            lines += L"[PASS]  " + result.name + L"\n";
        }
        else
        {
            lines += L"[FAIL]  " + result.name + L"  ->  " + result.detail + L"\n";
        }
    }
    const std::wstring text = std::to_wstring(passed) + L" of " + std::to_wstring(results.size()) +
                              L" business-rule checks passed.\n(A temporary agency was used - your data was not changed.)\n\n" +
                              lines;
    Gui::showMessage(m_hwnd, text, L"Business Rule Self-Test",
                     passed == results.size() ? MB_ICONINFORMATION : MB_ICONWARNING);
}

// =============================================================================
// Refreshing the display
// =============================================================================

void MainWindow::refreshAll()
{
    refreshFleet();
    refreshSearch();
    refreshRentals();
    refreshHistory();
    refreshReports();
    refreshStatusBar();
    updateButtons();
    updateTitle();
}

void MainWindow::refreshFleet()
{
    const std::wstring query = TextUtils::trim(Gui::getWindowText(m_fleetSearch));
    const LRESULT selection = SendMessageW(m_fleetFilter, CB_GETCURSEL, 0, 0);
    const StatusFilter filter = selection == 1 ? StatusFilter::Available
                              : selection == 2 ? StatusFilter::Rented
                                               : StatusFilter::All;

    std::vector<ReportListView::Row> rows;
    for (const Vehicle& vehicle : m_agency.searchFleet(query, filter))
    {
        ReportListView::Row row;
        row.key = vehicle.getPlate();
        row.cells.push_back({ vehicle.getPlate(), 0 });
        row.cells.push_back({ vehicle.getModel(), 0 });
        row.cells.push_back({ vehicle.getDailyRate().toDisplayString(), vehicle.getDailyRate().cents() });
        row.cells.push_back({ toString(vehicle.getStatus()), static_cast<long long>(vehicle.getStatus()) });
        row.actionable = vehicle.isAvailable(); // only available vehicles may be edited
        row.background = vehicle.isAvailable() ? AvailableRowColor : RentedRowColor;
        rows.push_back(std::move(row));
    }
    m_fleetList.setRows(std::move(rows));
}

void MainWindow::refreshSearch()
{
    const std::wstring query = TextUtils::trim(Gui::getWindowText(m_searchEdit));
    const std::vector<Vehicle> matches = m_agency.searchAvailableByModel(query);

    std::vector<ReportListView::Row> rows;
    for (const Vehicle& vehicle : matches)
    {
        ReportListView::Row row;
        row.key = vehicle.getPlate();
        row.cells.push_back({ vehicle.getPlate(), 0 });
        row.cells.push_back({ vehicle.getModel(), 0 });
        row.cells.push_back({ vehicle.getDailyRate().toDisplayString(), vehicle.getDailyRate().cents() });
        rows.push_back(std::move(row));
    }
    m_searchList.setRows(std::move(rows));

    const std::wstring summary = query.empty()
        ? countText(matches.size(), L"available vehicle", L"available vehicles") + L" (type to filter by model)."
        : countText(matches.size(), L"available vehicle matches", L"available vehicles match") +
              L" \"" + query + L"\".";
    SetWindowTextW(m_searchCount, summary.c_str());
}

void MainWindow::refreshRentals()
{
    std::vector<ReportListView::Row> rows;
    for (const Vehicle& vehicle : m_agency.getRentedVehicles())
    {
        const std::optional<Rental>& rental = vehicle.getRental();
        if (!rental)
        {
            continue;
        }
        ReportListView::Row row;
        row.key = vehicle.getPlate();
        row.cells.push_back({ vehicle.getPlate(), 0 });
        row.cells.push_back({ vehicle.getModel(), 0 });
        row.cells.push_back({ rental->getRenter().getName(), 0 });
        row.cells.push_back({ rental->getRenter().getPhone(), 0 });
        row.cells.push_back({ rental->getStartDate() ? rental->getStartDate()->toString() : std::wstring(L"-"), 0 });
        row.cells.push_back({ rental->getAgreedDailyRate().toDisplayString(), rental->getAgreedDailyRate().cents() });
        rows.push_back(std::move(row));
    }
    m_rentalsList.setRows(std::move(rows));

    const std::wstring revenueText = L"Cumulative Rental Revenue: " + m_agency.getRevenue().toDisplayString();
    SetWindowTextW(m_revenueLabel, revenueText.c_str());
}

void MainWindow::refreshHistory()
{
    const std::wstring query = TextUtils::trim(Gui::getWindowText(m_historySearch));
    const std::vector<RentalRecord> records = m_agency.searchHistory(query);

    std::vector<ReportListView::Row> rows;
    for (const RentalRecord& record : records)
    {
        const Receipt& receipt = record.getReceipt();
        ReportListView::Row row;
        row.key = std::to_wstring(record.getNumber());
        row.cells.push_back({ std::to_wstring(record.getNumber()), record.getNumber() });
        row.cells.push_back({ record.getReturnedOn(), 0 });
        row.cells.push_back({ receipt.getPlate(), 0 });
        row.cells.push_back({ receipt.getModel(), 0 });
        row.cells.push_back({ receipt.getRenterName(), 0 });
        row.cells.push_back({ receipt.getPhone(), 0 });
        row.cells.push_back({ receipt.getAgreedDailyRate().toDisplayString(), receipt.getAgreedDailyRate().cents() });
        row.cells.push_back({ std::to_wstring(receipt.getDays()), receipt.getDays() });
        row.cells.push_back({ receipt.getTotalCharge().toDisplayString(), receipt.getTotalCharge().cents() });
        rows.push_back(std::move(row));
    }
    m_historyList.setRows(std::move(rows));

    const std::wstring summary =
        (query.empty() ? countText(records.size(), L"completed rental", L"completed rentals")
                       : countText(records.size(), L"record matches", L"records match") + L" \"" + query + L"\"") +
        L"   |   Total charged: " + RentalAgency::totalCharged(records).toDisplayString();
    SetWindowTextW(m_historySummary, summary.c_str());
}

void MainWindow::refreshReports()
{
    if (!m_agency.isAllowed(Permission::ViewReports))
    {
        SetWindowTextW(m_reportsSummary, L"Reports and statistics are available to Managers only.");
        m_modelList.setRows({});
        m_topVehicleList.setRows({});
        return;
    }
    const RentalStatistics stats(m_agency); // all figures are calculated by the business layer

    const std::wstring summary =
        L"FLEET:   " + countText(stats.getVehicleCount(), L"vehicle", L"vehicles") + L"  -  " +
        std::to_wstring(stats.getAvailableCount()) + L" available, " + std::to_wstring(stats.getRentedCount()) +
        L" rented  (" + std::to_wstring(stats.getUtilizationPercent()) + L"% of the fleet is rented)\n" +
        L"ACTIVE RENTALS:   earn " + stats.getActiveDailyIncome().toDisplayString() + L" per day\n" +
        L"REVENUE:   " + stats.getRevenue().toDisplayString() + L" cumulative rental revenue\n" +
        L"COMPLETED RENTALS:   " + std::to_wstring(stats.getCompletedRentals()) + L"  -  total charged " +
        stats.getTotalCharged().toDisplayString() + L", average " + stats.getAverageCharge().toDisplayString() +
        L" per rental, average length " + stats.getAverageDaysText() + L" days";
    SetWindowTextW(m_reportsSummary, summary.c_str());

    std::vector<ReportListView::Row> modelRows;
    for (const ModelSummary& model : stats.getRevenueByModel())
    {
        ReportListView::Row row;
        row.key = model.model;
        row.cells.push_back({ model.model, 0 });
        row.cells.push_back({ std::to_wstring(model.rentals), model.rentals });
        row.cells.push_back({ std::to_wstring(model.days), model.days });
        row.cells.push_back({ model.revenue.toDisplayString(), model.revenue.cents() });
        modelRows.push_back(std::move(row));
    }
    m_modelList.setRows(std::move(modelRows));

    std::vector<ReportListView::Row> vehicleRows;
    for (const VehicleSummary& vehicle : stats.getTopVehicles())
    {
        ReportListView::Row row;
        row.key = vehicle.plate;
        row.cells.push_back({ vehicle.plate, 0 });
        row.cells.push_back({ vehicle.model, 0 });
        row.cells.push_back({ std::to_wstring(vehicle.rentals), vehicle.rentals });
        row.cells.push_back({ vehicle.revenue.toDisplayString(), vehicle.revenue.cents() });
        vehicleRows.push_back(std::move(row));
    }
    m_topVehicleList.setRows(std::move(vehicleRows));
}

void MainWindow::refreshStatusBar()
{
    const std::wstring parts[5] = {
        L"Total vehicles: " + std::to_wstring(m_agency.getVehicleCount()),
        L"Available: " + std::to_wstring(m_agency.getAvailableCount()),
        L"Rented: " + std::to_wstring(m_agency.getRentedCount()),
        L"Revenue: " + m_agency.getRevenue().toDisplayString(),
        m_lastAction,
    };
    for (int index = 0; index < 5; ++index)
    {
        SendMessageW(m_statusBar, SB_SETTEXTW, static_cast<WPARAM>(index),
                     reinterpret_cast<LPARAM>(parts[index].c_str()));
    }
}

void MainWindow::updateButtons()
{
    // Buttons follow both the selection and the role of the logged-in user.
    const bool canManageFleet = m_agency.isAllowed(Permission::ManageFleet);
    const bool canRent = m_agency.isAllowed(Permission::RentAndReturn);
    const ReportListView::Row* fleetRow = m_fleetList.selectedRow();
    enableControl(m_addButton, canManageFleet, m_fleetList.handle());
    enableControl(m_editButton, canManageFleet && fleetRow != nullptr && fleetRow->actionable, m_fleetList.handle());
    enableControl(m_removeButton, canManageFleet && fleetRow != nullptr && fleetRow->actionable, m_fleetList.handle());
    enableControl(m_rentButton, canRent && m_searchList.selectedRow() != nullptr, m_searchList.handle());
    enableControl(m_returnButton, canRent && m_rentalsList.selectedRow() != nullptr, m_rentalsList.handle());

    if (const HMENU menu = GetMenu(m_hwnd))
    {
        const UINT usersState = m_agency.isAllowed(Permission::ManageUsers) ? MF_ENABLED : MF_GRAYED;
        EnableMenuItem(menu, ID_USERS_MANAGE, MF_BYCOMMAND | usersState);
    }
}

void MainWindow::updateTitle()
{
    std::wstring title = ApplicationTitle;
    if (const User* user = m_agency.getCurrentUser())
    {
        title += L"  -  " + user->getUsername() + L" (" + toString(user->getRole()) + L")";
    }
    if (m_agency.hasUnsavedChanges())
    {
        title += L" *";
    }
    SetWindowTextW(m_hwnd, title.c_str());
}

void MainWindow::setLastAction(const std::wstring& text)
{
    m_lastAction = singleLine(text);
    refreshStatusBar();
}

void MainWindow::reportError(const std::wstring& message)
{
    setLastAction(L"Rejected: " + message);
    Gui::showMessage(m_hwnd, message, ApplicationTitle, MB_ICONWARNING);
}

// =============================================================================
// Tab helpers
// =============================================================================

int MainWindow::currentTab() const
{
    const int selection = static_cast<int>(SendMessageW(m_tab, TCM_GETCURSEL, 0, 0));
    return (selection >= 0 && selection < TabCount) ? selection : FleetTab;
}

void MainWindow::selectTab(int tab)
{
    SendMessageW(m_tab, TCM_SETCURSEL, static_cast<WPARAM>(tab), 0); // does not send TCN_SELCHANGE
    showPage(tab);
}

void MainWindow::showPage(int tab)
{
    for (int index = 0; index < TabCount; ++index)
    {
        ShowWindow(page(index), index == tab ? SW_SHOW : SW_HIDE);
    }
    focusCurrentPage();
}

void MainWindow::focusCurrentPage()
{
    switch (currentTab())
    {
    case FleetTab:
        SetFocus(m_fleetList.handle());
        break;
    case SearchTab:
        SetFocus(m_searchEdit);
        break;
    case RentalsTab:
        SetFocus(m_rentalsList.handle());
        break;
    case HistoryTab:
        SetFocus(m_historySearch);
        break;
    case ReportsTab:
        SetFocus(m_modelList.handle());
        break;
    default:
        break;
    }
}

HWND MainWindow::page(int tab) const
{
    return m_pages[static_cast<std::size_t>(tab)];
}

void MainWindow::enableControl(HWND control, bool enabled, HWND focusFallback) const
{
    // Disabling the focused control would leave the keyboard focus nowhere.
    if (!enabled && GetFocus() == control)
    {
        SetFocus(focusFallback);
    }
    EnableWindow(control, enabled ? TRUE : FALSE);
}
