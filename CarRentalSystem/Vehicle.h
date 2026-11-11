// =============================================================================
// Vehicle.h
// -----------------------------------------------------------------------------
// ENCAPSULATION: Vehicle protects its own rules.
//   * All data members are private; there are no setters.
//   * The constructor validates plate, model and daily rate.
//   * edit()  throws VehicleNotAvailableException if the vehicle is rented.
//   * rent()  throws VehicleNotAvailableException if the vehicle is already rented.
//   * returnVehicle() throws VehicleNotRentedException if it is not rented.
// Invariant: status == Rented  <=>  m_rental contains a value.
// Nobody outside the class can break this, because only these methods can
// change the status and they always change both together.
// =============================================================================
#pragma once

#include "Money.h"
#include "Receipt.h"
#include "Rental.h"
#include "Renter.h"

#include <optional>
#include <string>

/// The two possible states of a vehicle.
enum class VehicleStatus
{
    Available,
    Rented
};

/// "Available" / "Rented" - used for display and in the data file.
std::wstring toString(VehicleStatus status);

/// Parses "Available" / "Rented" (case-insensitive). Returns false if unknown.
bool tryParseVehicleStatus(const std::wstring& text, VehicleStatus& status);

class Vehicle
{
public:
    /// Creates an AVAILABLE vehicle. The plate is stored in upper case.
    /// @throws ValidationException if any value is invalid.
    Vehicle(const std::wstring& plate, const std::wstring& model, const Money& dailyRate);

    const std::wstring& getPlate() const noexcept;
    const std::wstring& getModel() const noexcept;
    Money getDailyRate() const noexcept;
    VehicleStatus getStatus() const noexcept;
    bool isAvailable() const noexcept;
    bool isRented() const noexcept;

    /// The active rental (empty when the vehicle is available).
    const std::optional<Rental>& getRental() const noexcept;

    /// Changes model and daily rate. The plate can never change.
    /// @throws VehicleNotAvailableException if the vehicle is rented.
    /// @throws ValidationException if a value is invalid (vehicle stays unchanged).
    void edit(const std::wstring& model, const Money& dailyRate);

    /// Rents the vehicle to the renter at the agreed daily rate.
    /// @throws VehicleNotAvailableException if already rented.
    /// @throws ValidationException if the agreed rate is invalid.
    void rent(const Renter& renter, const Money& agreedDailyRate);

    /// Ends the rental, calculates the charge and returns a receipt.
    /// @throws VehicleNotRentedException if the vehicle is not rented.
    /// @throws ValidationException if days is invalid (vehicle stays rented).
    Receipt returnVehicle(int days);

private:
    std::wstring m_plate;
    std::wstring m_model;
    Money m_dailyRate;
    VehicleStatus m_status = VehicleStatus::Available;
    std::optional<Rental> m_rental; ///< Only present while the vehicle is rented.
};
