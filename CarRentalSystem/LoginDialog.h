// =============================================================================
// LoginDialog.h
// -----------------------------------------------------------------------------
// Asks for the user name and password. The actual check is done by a function
// passed in by the caller (for example RentalAgency::login(), or loading the
// shared online data with these credentials first); if it throws, the dialog
// shows the message and stays open.
// =============================================================================
#pragma once

#include "ModalDialog.h"

#include <functional>
#include <string>

class LoginDialog final : public ModalDialog
{
public:
    using LoginAction = std::function<void(const std::wstring& username, const std::wstring& password)>;

    /// @param info text shown under the fields (data location, hints).
    LoginDialog(HINSTANCE instance, LoginAction action, std::wstring info);

private:
    bool onInitDialog() override;
    bool onOk() override;
    int controlForField(InputField field) const override;

    LoginAction m_action;
    std::wstring m_info;
};
