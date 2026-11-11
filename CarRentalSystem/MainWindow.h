// =============================================================================
// MainWindow.h
// -----------------------------------------------------------------------------
// The application's main window: menu, tab control with three pages, status
// bar. SEPARATION OF CONCERNS: the window contains no business rules at all -
// it only displays data obtained from RentalAgency and forwards the user's
// actions to RentalAgency (directly or through the dialogs). When the agency
// throws a RentalException, the window shows its message.
//
// Win32 calls a static window procedure; we store the C++ object pointer in
// the window's GWLP_USERDATA and forward each message to the member function
// handleMessage(), so all state lives in the object, not in global variables.
// =============================================================================
#pragma once

#include "Win32Include.h"

#include "GuiUtils.h"
#include "ReportListView.h"

#include <array>
#include <string>
#include <vector>

class IDataStore;
class RentalAgency;

class MainWindow
{
public:
    MainWindow(HINSTANCE instance, RentalAgency& agency, IDataStore& store);

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    /// Registers the window classes, creates and shows the window.
    bool create(int showCommand);

    /// Runs the message loop until the window is closed. Returns the exit code.
    int runMessageLoop();

    /// Shows one summary warning listing all lines skipped while loading.
    void showLoadWarnings(const std::vector<std::wstring>& warnings) const;

private:
    // ---- Tabs -------------------------------------------------------------------
    static constexpr int FleetTab = 0;
    static constexpr int SearchTab = 1;
    static constexpr int RentalsTab = 2;
    static constexpr int HistoryTab = 3;
    static constexpr int TabCount = 4;

    // ---- Window procedure plumbing -----------------------------------------------
    static bool registerWindowClasses(HINSTANCE instance);
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT routeMessage(UINT message, WPARAM wParam, LPARAM lParam);

    // ---- Creation and layout ------------------------------------------------------
    bool onCreate();
    void createControls();
    void createFleetPage(HWND page);
    void createSearchPage(HWND page);
    void createRentalsPage(HWND page);
    void createHistoryPage(HWND page);
    void applyFonts();
    void layout();
    void layoutFleetPage(int width, int height);
    void layoutSearchPage(int width, int height);
    void layoutRentalsPage(int width, int height);
    void layoutHistoryPage(int width, int height);
    void updateStatusBarParts();
    int scale(int value) const noexcept;

    // ---- Events -------------------------------------------------------------------
    void onCommand(int commandId, int notificationCode);
    LRESULT onNotify(NMHDR& header);
    LRESULT handleListNotification(ReportListView& list, NMHDR& header, void (MainWindow::*activate)());
    LRESULT onControlColor(HDC deviceContext, HWND control) const;
    void onDefaultAction();

    // ---- Use cases ----------------------------------------------------------------
    void onAddVehicle();
    void onEditVehicle();
    void onRentVehicle();
    void onReturnVehicle();
    void onShowHistoryReceipt();
    bool onSave();
    bool confirmSaveChanges();
    void onAbout() const;
    void onSelfTest() const;

    // ---- Refreshing the display ---------------------------------------------------
    void refreshAll();
    void refreshFleet();
    void refreshSearch();
    void refreshRentals();
    void refreshHistory();
    void refreshStatusBar();
    void updateButtons();
    void updateTitle();
    void setLastAction(const std::wstring& text);
    void reportError(const std::wstring& message);

    // ---- Tab helpers --------------------------------------------------------------
    int currentTab() const;
    void selectTab(int tab);
    void showPage(int tab);
    void focusCurrentPage();
    HWND page(int tab) const;
    void enableControl(HWND control, bool enabled, HWND focusFallback) const;

    // ---- Data -----------------------------------------------------------------------
    HINSTANCE m_instance;
    RentalAgency& m_agency; ///< The business logic (not owned).
    IDataStore& m_store;    ///< Where data is saved (not owned; used polymorphically).

    HWND m_hwnd = nullptr;
    HACCEL m_accelerators = nullptr;
    UINT m_dpi = USER_DEFAULT_SCREEN_DPI;
    HWND m_lastFocus = nullptr;

    HWND m_tab = nullptr;
    std::array<HWND, TabCount> m_pages{};
    HWND m_statusBar = nullptr;

    // Fleet tab
    HWND m_fleetHint = nullptr;
    ReportListView m_fleetList;
    HWND m_addButton = nullptr;
    HWND m_editButton = nullptr;

    // Search & Rent tab
    HWND m_searchLabel = nullptr;
    HWND m_searchEdit = nullptr;
    ReportListView m_searchList;
    HWND m_searchCount = nullptr;
    HWND m_rentButton = nullptr;

    // Rentals & Returns tab
    HWND m_revenueLabel = nullptr;
    ReportListView m_rentalsList;
    HWND m_rentalsHint = nullptr;
    HWND m_returnButton = nullptr;

    // Rental History tab
    HWND m_historySummary = nullptr;
    HWND m_historyLabel = nullptr;
    HWND m_historySearch = nullptr;
    ReportListView m_historyList;
    HWND m_historyHint = nullptr;

    Gui::FontHandle m_font;       ///< Segoe UI 9 pt, DPI-scaled.
    Gui::FontHandle m_titleFont;  ///< Large bold font for the revenue label.
    std::wstring m_lastAction = L"Ready.";
};
