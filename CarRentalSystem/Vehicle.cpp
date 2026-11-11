// =============================================================================
// Vehicle.cpp
// =============================================================================
#include "Vehicle.h"

#include "RentalExceptions.h"
#include "TextUtils.h"
#include "Validation.h"

#include <utility>

std::wstring toString(VehicleStatus status)
{
    return status == VehicleStatus::Rented ? L"Rented" : L"Available";
}

bool tryParseVehicleStatus(const std::wstring& text, VehicleStatus& status)
{
    const std::wstring trimmed = TextUtils::trim(text);
    if (TextUtils::equalsIgnoreCase(trimmed, L"Available"))
    {
        status = VehicleStatus::Available;
        return true;
    }
    if (TextUtils::equalsIgnoreCase(trimmed, L"Rented"))
    {
        status = VehicleStatus::Rented;
        return true;
    }
    return false;
}

Vehicle::Vehicle(const std::wstring& plate, const std::wstring& model, const Money& dailyRate)
    : m_plate(Validation::normalizePlate(plate)),
      m_model(Validation::validateModel(model)),
      m_dailyRate(dailyRate)
{
    Validation::requireValidRate(m_dailyRate, InputField::DailyRate);
}

const std::wstring& Vehicle::getPlate() const noexcept { return m_plate; }
const std::wstring& Vehicle::getModel() const noexcept { return m_model; }
Money Vehicle::getDailyRate() const noexcept { return m_dailyRate; }
VehicleStatus Vehicle::getStatus() const noexcept { return m_status; }
bool Vehicle::isAvailable() const noexcept { return m_status == VehicleStatus::Available; }
bool Vehicle::isRented() const noexcept { return m_status == VehicleStatus::Rented; }
const std::optional<Rental>& Vehicle::getRental() const noexcept { return m_rental; }

void Vehicle::edit(const std::wstring& model, const Money& dailyRate)
{
    if (!isAvailable())
    {
        throw VehicleNotAvailableException(m_plate, L"edited");
    }
    // Validate everything BEFORE changing anything (strong exception guarantee):
    // if a value is invalid, the vehicle keeps its old data.
    std::wstring newModel = Validation::validateModel(model);
    Validation::requireValidRate(dailyRate, InputField::DailyRate);

    m_model = std::move(newModel);
    m_dailyRate = dailyRate;
}

void Vehicle::rent(const Renter& renter, const Money& agreedDailyRate, std::optional<DateTime> startDate)
{
    if (!isAvailable())
    {
        throw VehicleNotAvailableException(m_plate, L"rented again");
    }
    m_rental.emplace(renter, agreedDailyRate, startDate); // Rental's constructor validates the rate
    m_status = VehicleStatus::Rented;
}

Receipt Vehicle::returnVehicle(int days)
{
    if (!isRented() || !m_rental)
    {
        throw VehicleNotRentedException(m_plate);
    }

    // Calculate first: if days is invalid this throws and the vehicle stays rented.
    const Money charge = m_rental->calculateCharge(days);
    const std::optional<DateTime>& start = m_rental->getStartDate();
    Receipt receipt(m_plate, m_model, m_rental->getRenter().getName(), m_rental->getRenter().getPhone(),
                    m_rental->getAgreedDailyRate(), days, charge, start ? start->toString() : std::wstring());

    m_rental.reset(); // clear the rental details
    m_status = VehicleStatus::Available;
    return receipt;
}
