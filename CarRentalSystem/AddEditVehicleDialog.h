// =============================================================================
// AddEditVehicleDialog.h
// -----------------------------------------------------------------------------
// Dialog for adding a new vehicle or editing an existing one (same template,
// two modes). In edit mode the plate is shown read-only, because a vehicle's
// plate can never change. The dialog only collects input; RentalAgency does
// the work and enforces the rules.
// =============================================================================
#pragma once

#include "ModalDialog.h"
#include "Vehicle.h"

#include <optional>
#include <string>

class RentalAgency;

class AddEditVehicleDialog final : public ModalDialog
{
public:
    /// Add mode.
    AddEditVehicleDialog(HINSTANCE instance, RentalAgency& agency);

    /// Edit mode for the given (available) vehicle.
    AddEditVehicleDialog(HINSTANCE instance, RentalAgency& agency, const Vehicle& vehicleToEdit);

    /// Plate of the vehicle that was added/edited (valid after OK).
    const std::wstring& savedPlate() const noexcept;

private:
    bool onInitDialog() override;
    bool onOk() override;
    int controlForField(InputField field) const override;

    bool isEditMode() const noexcept;

    RentalAgency& m_agency;
    std::optional<Vehicle> m_vehicleToEdit;
    std::wstring m_savedPlate;
};
