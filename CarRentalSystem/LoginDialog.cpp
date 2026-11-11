// =============================================================================
// LoginDialog.cpp
// =============================================================================
#include "LoginDialog.h"

#include "Resource.h"
#include "Validation.h"

#include <utility>

LoginDialog::LoginDialog(HINSTANCE instance, LoginAction action, std::wstring info)
    : ModalDialog(instance, IDD_LOGIN), m_action(std::move(action)), m_info(std::move(info))
{
}

bool LoginDialog::onInitDialog()
{
    limitText(IDC_LOGIN_USERNAME, Validation::MaxUsernameLength);
    limitText(IDC_LOGIN_PASSWORD, Validation::MaxPasswordLength);
    setControlText(IDC_LOGIN_INFO, m_info);
    focusControl(IDC_LOGIN_USERNAME);
    return false;
}

bool LoginDialog::onOk()
{
    // Throws AuthenticationException / DataStoreException; ModalDialog shows the message.
    m_action(getControlText(IDC_LOGIN_USERNAME), getControlText(IDC_LOGIN_PASSWORD));
    return true;
}

int LoginDialog::controlForField(InputField field) const
{
    return field == InputField::Password ? IDC_LOGIN_PASSWORD : IDC_LOGIN_USERNAME;
}
