// =============================================================================
// ChangePasswordDialog.h - lets the logged-in user change their own password.
// =============================================================================
#pragma once

#include "ModalDialog.h"

class RentalAgency;

class ChangePasswordDialog final : public ModalDialog
{
public:
    ChangePasswordDialog(HINSTANCE instance, RentalAgency& agency);

private:
    bool onInitDialog() override;
    bool onOk() override;
    int controlForField(InputField field) const override;

    RentalAgency& m_agency;
};
