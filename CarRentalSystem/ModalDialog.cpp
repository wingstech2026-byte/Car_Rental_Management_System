// =============================================================================
// ModalDialog.cpp
// =============================================================================
#include "ModalDialog.h"

#include "GuiUtils.h"
#include "TextUtils.h"

ModalDialog::ModalDialog(HINSTANCE instance, int templateId) noexcept
    : m_instance(instance), m_templateId(templateId)
{
}

bool ModalDialog::showModal(HWND owner)
{
    // "this" travels to WM_INITDIALOG through the last parameter.
    const INT_PTR result = DialogBoxParamW(m_instance, MAKEINTRESOURCEW(m_templateId), owner,
                                           &ModalDialog::dialogProc, reinterpret_cast<LPARAM>(this));
    return result == IDOK;
}

void ModalDialog::onCommand(int, int)
{
    // Default: nothing to do. Derived classes override this when needed.
}

int ModalDialog::controlForField(InputField) const
{
    return 0;
}

HWND ModalDialog::dialogHandle() const noexcept
{
    return m_dialog;
}

std::wstring ModalDialog::getControlText(int controlId) const
{
    return Gui::getWindowText(GetDlgItem(m_dialog, controlId));
}

void ModalDialog::setControlText(int controlId, const std::wstring& text) const
{
    SetDlgItemTextW(m_dialog, controlId, text.c_str());
}

void ModalDialog::limitText(int controlId, std::size_t maxLength) const
{
    SendDlgItemMessageW(m_dialog, controlId, EM_LIMITTEXT, static_cast<WPARAM>(maxLength), 0);
}

void ModalDialog::focusControl(int controlId) const
{
    const HWND control = GetDlgItem(m_dialog, controlId);
    if (control == nullptr)
    {
        return;
    }
    // WM_NEXTDLGCTL is the correct way to move the focus inside a dialog
    // (it also updates the default push button and selects the edit text).
    SendMessageW(m_dialog, WM_NEXTDLGCTL, reinterpret_cast<WPARAM>(control), TRUE);
    SendMessageW(control, EM_SETSEL, 0, -1);
}

void ModalDialog::showError(const std::wstring& message, int controlId) const
{
    MessageBoxW(m_dialog, message.c_str(), L"Please check your input", MB_OK | MB_ICONWARNING);
    if (controlId != 0)
    {
        focusControl(controlId);
    }
}

INT_PTR CALLBACK ModalDialog::dialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    ModalDialog* self = nullptr;
    if (message == WM_INITDIALOG)
    {
        self = reinterpret_cast<ModalDialog*>(lParam);
        self->m_dialog = dialog;
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<ModalDialog*>(GetWindowLongPtrW(dialog, DWLP_USER));
    }
    if (self == nullptr)
    {
        return FALSE; // messages that arrive before WM_INITDIALOG
    }
    return self->handleMessage(message, wParam);
}

INT_PTR ModalDialog::handleMessage(UINT message, WPARAM wParam)
{
    // Exceptions must never escape into Windows code, so every call into the
    // derived class is wrapped in try/catch.
    switch (message)
    {
    case WM_INITDIALOG:
        try
        {
            return onInitDialog() ? TRUE : FALSE; // virtual call
        }
        catch (const std::exception& error)
        {
            showError(TextUtils::fromUtf8Lenient(error.what()), 0);
            EndDialog(m_dialog, IDCANCEL);
            return TRUE;
        }

    case WM_COMMAND:
    {
        const int controlId = LOWORD(wParam);
        const int notificationCode = HIWORD(wParam);
        if (controlId == IDOK)
        {
            handleOk();
        }
        else if (controlId == IDCANCEL)
        {
            EndDialog(m_dialog, IDCANCEL);
        }
        else
        {
            try
            {
                onCommand(controlId, notificationCode); // virtual call
            }
            catch (const std::exception& error)
            {
                showError(TextUtils::fromUtf8Lenient(error.what()), 0);
            }
        }
        return TRUE;
    }

    default:
        return FALSE;
    }
}

void ModalDialog::handleOk()
{
    try
    {
        if (onOk()) // virtual call into the derived dialog
        {
            EndDialog(m_dialog, IDOK);
        }
    }
    catch (const ValidationException& error)
    {
        showError(error.message(), controlForField(error.field()));
    }
    catch (const DuplicatePlateException& error)
    {
        showError(error.message(), controlForField(InputField::Plate));
    }
    catch (const RentalException& error)
    {
        showError(error.message(), 0);
    }
    catch (const std::exception& error)
    {
        showError(TextUtils::fromUtf8Lenient(error.what()), 0);
    }
}
