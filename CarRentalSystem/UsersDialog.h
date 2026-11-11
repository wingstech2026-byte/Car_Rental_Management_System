// =============================================================================
// UsersDialog.h
// -----------------------------------------------------------------------------
// Managers only: lists the user accounts and lets the Manager add accounts,
// delete accounts and reset passwords. Every operation is a call to
// RentalAgency, which enforces the rules (Manager permission, unique names,
// password length, never delete yourself or the last Manager).
// =============================================================================
#pragma once

#include "ModalDialog.h"

#include <string>

class RentalAgency;

class UsersDialog final : public ModalDialog
{
public:
    UsersDialog(HINSTANCE instance, RentalAgency& agency);

    /// True if an account was added, deleted or changed.
    bool changed() const noexcept;

private:
    bool onInitDialog() override;
    bool onOk() override;
    void onCommand(int controlId, int notificationCode) override;
    int controlForField(InputField field) const override;

    void refreshList();
    std::wstring selectedUsername() const;
    void runAction(void (UsersDialog::*action)());
    void addAccount();
    void removeAccount();
    void resetAccountPassword();

    RentalAgency& m_agency;
    bool m_changed = false;
};
