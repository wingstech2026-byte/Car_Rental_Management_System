// =============================================================================
// UsersDialog.cpp
// =============================================================================
#include "UsersDialog.h"

#include "RentalAgency.h"
#include "RentalExceptions.h"
#include "Resource.h"
#include "TextUtils.h"
#include "Validation.h"

UsersDialog::UsersDialog(HINSTANCE instance, RentalAgency& agency)
    : ModalDialog(instance, IDD_USERS), m_agency(agency)
{
}

bool UsersDialog::changed() const noexcept
{
    return m_changed;
}

bool UsersDialog::onInitDialog()
{
    limitText(IDC_USERS_NAME, Validation::MaxUsernameLength);
    limitText(IDC_USERS_PASSWORD, Validation::MaxPasswordLength);
    SendDlgItemMessageW(dialogHandle(), IDC_USERS_ROLE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Clerk"));
    SendDlgItemMessageW(dialogHandle(), IDC_USERS_ROLE, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Manager"));
    SendDlgItemMessageW(dialogHandle(), IDC_USERS_ROLE, CB_SETCURSEL, 0, 0);
    refreshList();
    focusControl(IDC_USERS_NAME);
    return false;
}

bool UsersDialog::onOk()
{
    return true; // "Close"
}

void UsersDialog::onCommand(int controlId, int notificationCode)
{
    if (notificationCode != BN_CLICKED)
    {
        return;
    }
    switch (controlId)
    {
    case IDC_USERS_ADD:
        runAction(&UsersDialog::addAccount);
        break;
    case IDC_USERS_REMOVE:
        runAction(&UsersDialog::removeAccount);
        break;
    case IDC_USERS_RESET:
        runAction(&UsersDialog::resetAccountPassword);
        break;
    default:
        break;
    }
}

int UsersDialog::controlForField(InputField field) const
{
    switch (field)
    {
    case InputField::Username:
        return IDC_USERS_NAME;
    case InputField::Password:
        return IDC_USERS_PASSWORD;
    default:
        return 0;
    }
}

void UsersDialog::runAction(void (UsersDialog::*action)())
{
    try
    {
        (this->*action)();
        refreshList();
    }
    catch (const ValidationException& error)
    {
        showError(error.message(), controlForField(error.field()));
    }
    catch (const RentalException& error)
    {
        showError(error.message(), 0);
    }
}

void UsersDialog::addAccount()
{
    const LRESULT roleIndex = SendDlgItemMessageW(dialogHandle(), IDC_USERS_ROLE, CB_GETCURSEL, 0, 0);
    const Role role = roleIndex == 1 ? Role::Manager : Role::Clerk;
    const std::wstring name = getControlText(IDC_USERS_NAME);
    m_agency.addUser(name, getControlText(IDC_USERS_PASSWORD), role);
    m_changed = true;
    setControlText(IDC_USERS_NAME, L"");
    setControlText(IDC_USERS_PASSWORD, L"");
    MessageBoxW(dialogHandle(), (L"Account \"" + TextUtils::toLower(TextUtils::trim(name)) + L"\" added.").c_str(),
                L"Manage Users", MB_OK | MB_ICONINFORMATION);
}

void UsersDialog::removeAccount()
{
    const std::wstring username = selectedUsername();
    if (username.empty())
    {
        throw RentalException(L"Select an account in the list first.");
    }
    const std::wstring question = L"Delete the account \"" + username + L"\"?";
    if (MessageBoxW(dialogHandle(), question.c_str(), L"Manage Users", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
    {
        return;
    }
    m_agency.removeUser(username);
    m_changed = true;
}

void UsersDialog::resetAccountPassword()
{
    const std::wstring username = selectedUsername();
    if (username.empty())
    {
        throw RentalException(L"Select an account in the list first, then type the new password.");
    }
    m_agency.resetPassword(username, getControlText(IDC_USERS_PASSWORD));
    m_changed = true;
    setControlText(IDC_USERS_PASSWORD, L"");
    MessageBoxW(dialogHandle(), (L"The password of \"" + username + L"\" was changed.").c_str(), L"Manage Users",
                MB_OK | MB_ICONINFORMATION);
}

void UsersDialog::refreshList()
{
    const HWND list = GetDlgItem(dialogHandle(), IDC_USERS_LIST);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const User& user : m_agency.getUsers())
    {
        const std::wstring line = user.getUsername() + L"   (" + toString(user.getRole()) + L")";
        SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(line.c_str()));
    }
}

std::wstring UsersDialog::selectedUsername() const
{
    const HWND list = GetDlgItem(dialogHandle(), IDC_USERS_LIST);
    const LRESULT index = SendMessageW(list, LB_GETCURSEL, 0, 0);
    if (index == LB_ERR)
    {
        return std::wstring();
    }
    const LRESULT length = SendMessageW(list, LB_GETTEXTLEN, static_cast<WPARAM>(index), 0);
    if (length <= 0)
    {
        return std::wstring();
    }
    std::wstring line(static_cast<std::size_t>(length) + 1, L'\0');
    SendMessageW(list, LB_GETTEXT, static_cast<WPARAM>(index), reinterpret_cast<LPARAM>(line.data()));
    return line.substr(0, line.find(L' ')); // "name   (Role)" -> "name"
}
