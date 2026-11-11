// =============================================================================
// ChangePasswordDialog.cpp
// =============================================================================
#include "ChangePasswordDialog.h"

#include "RentalAgency.h"
#include "RentalExceptions.h"
#include "Resource.h"
#include "Validation.h"

ChangePasswordDialog::ChangePasswordDialog(HINSTANCE instance, RentalAgency& agency)
    : ModalDialog(instance, IDD_CHANGE_PASSWORD), m_agency(agency)
{
}

bool ChangePasswordDialog::onInitDialog()
{
    limitText(IDC_PW_CURRENT, Validation::MaxPasswordLength);
    limitText(IDC_PW_NEW, Validation::MaxPasswordLength);
    limitText(IDC_PW_CONFIRM, Validation::MaxPasswordLength);
    focusControl(IDC_PW_CURRENT);
    return false;
}

bool ChangePasswordDialog::onOk()
{
    const std::wstring newPassword = getControlText(IDC_PW_NEW);
    if (newPassword != getControlText(IDC_PW_CONFIRM))
    {
        throw ValidationException(InputField::None, L"The new password and its confirmation are different.");
    }
    m_agency.changeOwnPassword(getControlText(IDC_PW_CURRENT), newPassword);
    return true;
}

int ChangePasswordDialog::controlForField(InputField field) const
{
    switch (field)
    {
    case InputField::Password:
        return IDC_PW_CURRENT;
    case InputField::None:
        return IDC_PW_CONFIRM;
    default:
        return IDC_PW_NEW;
    }
}
