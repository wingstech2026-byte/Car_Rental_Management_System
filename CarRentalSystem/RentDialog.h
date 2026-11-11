// =============================================================================
// RentDialog.h
// -----------------------------------------------------------------------------
// Dialog for renting an available vehicle. Shows plate, model and current rate
// read-only and asks for renter name, telephone and agreed daily rate (pre-filled
// with the current daily rate, but editable).
// =============================================================================
#pragma once

#include "ModalDialog.h"
#include "Vehicle.h"

#include <string>

class RentalAgency;

class RentDialog final : public ModalDialog
{
public:
    RentDialog(HINSTANCE instance, RentalAgency& agency, const Vehicle& vehicle);

    /// Name of the renter (valid after OK).
    const std::wstring& renterName() const noexcept;

private:
    bool onInitDialog() override;
    bool onOk() override;
    int controlForField(InputField field) const override;

    RentalAgency& m_agency;
    Vehicle m_vehicle; ///< Copy, used only for display and for the plate.
    std::wstring m_renterName;
};
