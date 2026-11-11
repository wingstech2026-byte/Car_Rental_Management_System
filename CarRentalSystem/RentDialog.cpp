// =============================================================================
// RentDialog.cpp
// =============================================================================
#include "RentDialog.h"

#include "RentalAgency.h"
#include "Renter.h"
#include "Resource.h"
#include "Validation.h"

RentDialog::RentDialog(HINSTANCE instance, RentalAgency& agency, const Vehicle& vehicle)
    : ModalDialog(instance, IDD_RENT), m_agency(agency), m_vehicle(vehicle)
{
}

const std::wstring& RentDialog::renterName() const noexcept
{
    return m_renterName;
}

bool RentDialog::onInitDialog()
{
    setControlText(IDC_RENT_PLATE, m_vehicle.getPlate());
    setControlText(IDC_RENT_MODEL, m_vehicle.getModel());
    setControlText(IDC_RENT_CURRENT_RATE, m_vehicle.getDailyRate().toDisplayString());
    setControlText(IDC_AGREED_RATE, m_vehicle.getDailyRate().toString()); // pre-filled, editable

    limitText(IDC_RENTER_NAME, Validation::MaxRenterNameLength);
    limitText(IDC_RENTER_PHONE, Validation::MaxPhoneLength);
    limitText(IDC_AGREED_RATE, 16);

    focusControl(IDC_RENTER_NAME);
    return false;
}

bool RentDialog::onOk()
{
    // Renter's constructor validates name and telephone; parseRate validates the rate;
    // RentalAgency/Vehicle refuse if the vehicle is not available.
    const Renter renter(getControlText(IDC_RENTER_NAME), getControlText(IDC_RENTER_PHONE));
    const Money agreedRate = Validation::parseRate(getControlText(IDC_AGREED_RATE), InputField::AgreedRate);
    m_agency.rentVehicle(m_vehicle.getPlate(), renter, agreedRate);
    m_renterName = renter.getName();
    return true;
}

int RentDialog::controlForField(InputField field) const
{
    switch (field)
    {
    case InputField::RenterName:
        return IDC_RENTER_NAME;
    case InputField::Phone:
        return IDC_RENTER_PHONE;
    case InputField::AgreedRate:
        return IDC_AGREED_RATE;
    default:
        return 0;
    }
}
