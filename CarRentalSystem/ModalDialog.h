// =============================================================================
// ModalDialog.h
// -----------------------------------------------------------------------------
// INHERITANCE + POLYMORPHISM in the GUI (Template Method pattern).
//
// ModalDialog is an abstract base class that contains everything all dialogs
// have in common: showing the dialog from a resource template, routing Win32
// messages to the C++ object, handling OK/Cancel, and - most importantly -
// catching business exceptions when OK is pressed:
//
//   * the derived class implements onOk() and simply calls the business logic,
//   * if the logic throws ValidationException, the base class shows the message,
//     keeps the dialog open and focuses the field returned by the virtual
//     function controlForField() of the derived class.
//
// AddEditVehicleDialog, RentDialog and ReturnDialog override the pure virtual
// functions onInitDialog() and onOk(); the static dialogProc calls them through
// a ModalDialog pointer - dynamic dispatch decides which version runs.
// =============================================================================
#pragma once

#include "Win32Include.h"

#include "RentalExceptions.h"

#include <string>

class ModalDialog
{
public:
    virtual ~ModalDialog() = default;

    ModalDialog(const ModalDialog&) = delete;
    ModalDialog& operator=(const ModalDialog&) = delete;

    /// Shows the dialog modally. Returns true if the user confirmed with OK
    /// and the operation succeeded.
    bool showModal(HWND owner);

protected:
    ModalDialog(HINSTANCE instance, int templateId) noexcept;

    /// Fills the controls. Return true to let Windows focus the first control,
    /// or false after setting the focus yourself.
    virtual bool onInitDialog() = 0;

    /// Validates the input and performs the operation. Return true to close the
    /// dialog. Exceptions are caught and reported by the base class.
    virtual bool onOk() = 0;

    /// Called for WM_COMMAND messages other than OK/Cancel (e.g. EN_CHANGE).
    virtual void onCommand(int controlId, int notificationCode);

    /// Maps a field from a ValidationException to the control ID to focus (0 = none).
    virtual int controlForField(InputField field) const;

    // ---- Helpers for derived classes ---------------------------------------------
    HWND dialogHandle() const noexcept;
    std::wstring getControlText(int controlId) const;
    void setControlText(int controlId, const std::wstring& text) const;
    void limitText(int controlId, std::size_t maxLength) const;
    void focusControl(int controlId) const;
    void showError(const std::wstring& message, int controlId) const;

private:
    static INT_PTR CALLBACK dialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam);
    INT_PTR handleMessage(UINT message, WPARAM wParam);
    void handleOk();

    HINSTANCE m_instance;
    int m_templateId;
    HWND m_dialog = nullptr;
};
