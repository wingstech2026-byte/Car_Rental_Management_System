// =============================================================================
// RentalAgency.h
// -----------------------------------------------------------------------------
// The central business class ("model" / "service" layer). It OWNS the fleet
// and the cumulative revenue, and offers one method per use case.
//
//   * Contains NO GUI code - it could be reused by a console program or tests.
//   * All rules are enforced here (or in the Vehicle/Rental/Renter classes it
//     uses), never only in the GUI.
//   * Read-only queries return copies (std::vector<Vehicle>), so callers can
//     never modify the fleet behind the agency's back (encapsulation).
//   * Persistence is done through the IDataStore interface (polymorphism).
// =============================================================================
#pragma once

#include "Money.h"
#include "Receipt.h"
#include "Renter.h"
#include "Vehicle.h"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

class IDataStore;

class RentalAgency
{
public:
    // ---- Use cases ------------------------------------------------------------

    /// 1. Adds a new available vehicle.
    /// @throws ValidationException, DuplicatePlateException
    const Vehicle& addVehicle(const std::wstring& plate, const std::wstring& model, const Money& dailyRate);

    /// 2. Changes the model and daily rate of an AVAILABLE vehicle.
    /// @throws VehicleNotFoundException, VehicleNotAvailableException, ValidationException
    void editVehicle(const std::wstring& plate, const std::wstring& model, const Money& dailyRate);

    /// 3. Available vehicles whose model contains the query (case-insensitive).
    ///    An empty query returns all available vehicles.
    std::vector<Vehicle> searchAvailableByModel(const std::wstring& query) const;

    /// 4. Rents an available vehicle.
    /// @throws VehicleNotFoundException, VehicleNotAvailableException, ValidationException
    void rentVehicle(const std::wstring& plate, const Renter& renter, const Money& agreedDailyRate);

    /// 5. Returns a rented vehicle, adds the charge to the revenue and gives a receipt.
    /// @throws VehicleNotFoundException, VehicleNotRentedException, ValidationException
    Receipt returnVehicle(const std::wstring& plate, int days);

    /// 6. All vehicles that are currently rented.
    std::vector<Vehicle> getRentedVehicles() const;

    /// 6. The cumulative rental revenue.
    Money getRevenue() const noexcept;

    // ---- Queries ----------------------------------------------------------------

    /// All vehicles, sorted by plate.
    std::vector<Vehicle> getAllVehicles() const;

    /// @throws VehicleNotFoundException
    const Vehicle& findVehicle(const std::wstring& plate) const;

    /// Like findVehicle(), but @throws VehicleNotAvailableException if the vehicle is rented.
    const Vehicle& findEditableVehicle(const std::wstring& plate) const;

    /// Like findVehicle(), but @throws VehicleNotAvailableException if the vehicle is rented.
    const Vehicle& findRentableVehicle(const std::wstring& plate) const;

    /// Like findVehicle(), but @throws VehicleNotRentedException if it is not rented.
    const Vehicle& findRentedVehicle(const std::wstring& plate) const;

    std::size_t getVehicleCount() const noexcept;
    std::size_t getAvailableCount() const;
    std::size_t getRentedCount() const;

    /// True if something changed since the last load/save.
    bool hasUnsavedChanges() const noexcept;

    // ---- Persistence (through the abstract interface) --------------------------

    /// Replaces the current data with the data from the store.
    /// @return warnings about skipped entries (empty if everything was valid).
    std::vector<std::wstring> loadFrom(IDataStore& store);

    /// Writes all data to the store and clears the "unsaved changes" flag.
    void saveTo(IDataStore& store);

private:
    Vehicle& findMutableVehicle(const std::wstring& plate);
    static std::wstring makeKey(const std::wstring& plate);

    std::map<std::wstring, Vehicle> m_fleet; ///< Key = upper-case plate (unique, sorted).
    Money m_revenue;                         ///< Cumulative rental revenue.
    bool m_modified = false;                 ///< Unsaved changes?
};
