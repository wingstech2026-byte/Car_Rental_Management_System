// =============================================================================
// AddEditVehicleDialog.cpp
// =============================================================================
#include "AddEditVehicleDialog.h"

#include "RentalAgency.h"
#include "Resource.h"
#include "Validation.h"

AddEditVehicleDialog::AddEditVehicleDialog(HINSTANCE instance, RentalAgency& agency)
    : ModalDialog(instance, IDD_VEHICLE), m_agency(agency)
{
}

AddEditVehicleDialog::AddEditVehicleDialog(HINSTANCE instance, RentalAgency& agency, const Vehicle& vehicleToEdit)
    : ModalDialog(instance, IDD_VEHICLE), m_agency(agency), m_vehicleToEdit(vehicleToEdit)
{
}

const std::wstring& AddEditVehicleDialog::savedPlate() const noexcept
{
    return m_savedPlate;
}

bool AddEditVehicleDialog::isEditMode() const noexcept
{
    return m_vehicleToEdit.has_value();
}

bool AddEditVehicleDialog::onInitDialog()
{
    limitText(IDC_PLATE_EDIT, Validation::MaxPlateLength);
    limitText(IDC_MODEL_EDIT, Validation::MaxModelLength);
    limitText(IDC_RATE_EDIT, 16);

    if (!isEditMode())
    {
        SetWindowTextW(dialogHandle(), L"Add Vehicle");
        return true; // focus goes to the first field (plate)
    }

    SetWindowTextW(dialogHandle(), L"Edit Vehicle");
    setControlText(IDC_PLATE_EDIT, m_vehicleToEdit->getPlate());
    SendDlgItemMessageW(dialogHandle(), IDC_PLATE_EDIT, EM_SETREADONLY, TRUE, 0);
    setControlText(IDC_PLATE_HINT, L"The license plate cannot be changed.");
    setControlText(IDC_MODEL_EDIT, m_vehicleToEdit->getModel());
    setControlText(IDC_RATE_EDIT, m_vehicleToEdit->getDailyRate().toString());
    focusControl(IDC_MODEL_EDIT);
    return false;
}

bool AddEditVehicleDialog::onOk()
{
    // The fields are checked in screen order so the first bad field gets the focus.
    // Any exception thrown here is caught by ModalDialog::handleOk().
    if (isEditMode())
    {
        const std::wstring model = Validation::validateModel(getControlText(IDC_MODEL_EDIT));
        const Money rate = Validation::parseRate(getControlText(IDC_RATE_EDIT), InputField::DailyRate);
        m_agency.editVehicle(m_vehicleToEdit->getPlate(), model, rate);
        m_savedPlate = m_vehicleToEdit->getPlate();
    }
    else
    {
        const std::wstring plate = Validation::normalizePlate(getControlText(IDC_PLATE_EDIT));
        const std::wstring model = Validation::validateModel(getControlText(IDC_MODEL_EDIT));
        const Money rate = Validation::parseRate(getControlText(IDC_RATE_EDIT), InputField::DailyRate);
        m_savedPlate = m_agency.addVehicle(plate, model, rate).getPlate();
    }
    return true;
}

int AddEditVehicleDialog::controlForField(InputField field) const
{
    switch (field)
    {
    case InputField::Plate:
        return isEditMode() ? 0 : IDC_PLATE_EDIT;
    case InputField::Model:
        return IDC_MODEL_EDIT;
    case InputField::DailyRate:
        return IDC_RATE_EDIT;
    default:
        return 0;
    }
}
