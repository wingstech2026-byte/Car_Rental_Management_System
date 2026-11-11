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
//   * Role-based access control: once a user has logged in, every protected
//     use case checks the user's Permission and throws AccessDeniedException.
// =============================================================================
#pragma once

#include "Money.h"
#include "Receipt.h"
#include "RentalRecord.h"
#include "Renter.h"
#include "User.h"
#include "UserDirectory.h"
#include "Vehicle.h"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

class IDataStore;

/// Which vehicles RentalAgency::searchFleet() returns.
enum class StatusFilter
{
    All,
    Available,
    Rented
};

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

    /// Removes an AVAILABLE vehicle from the fleet. Its completed rentals stay in the history.
    /// @throws VehicleNotFoundException, VehicleNotAvailableException (vehicle is rented)
    void removeVehicle(const std::wstring& plate);

    /// All vehicles whose plate or model contains the query (case-insensitive),
    /// limited to the given status. An empty query matches every vehicle.
    std::vector<Vehicle> searchFleet(const std::wstring& query, StatusFilter filter) const;

    /// 3. Available vehicles whose model contains the query (case-insensitive).
    ///    An empty query returns all available vehicles.
    std::vector<Vehicle> searchAvailableByModel(const std::wstring& query) const;

    /// 4. Rents an available vehicle.
    /// @throws VehicleNotFoundException, VehicleNotAvailableException, ValidationException
    void rentVehicle(const std::wstring& plate, const Renter& renter, const Money& agreedDailyRate);

    /// 5. Returns a rented vehicle, adds the charge to the revenue, archives the
    ///    rental in the history and gives a receipt.
    /// @throws VehicleNotFoundException, VehicleNotRentedException, ValidationException
    Receipt returnVehicle(const std::wstring& plate, int days);

    /// Rental days suggested for returning a vehicle now, calculated from the
    /// rental start date (every started 24 hours counts, at least 1).
    /// @throws VehicleNotFoundException, VehicleNotRentedException
    int suggestedRentalDays(const std::wstring& plate) const;

    /// 6. All vehicles that are currently rented.
    std::vector<Vehicle> getRentedVehicles() const;

    /// 6. The cumulative rental revenue.
    Money getRevenue() const noexcept;

    // ---- Rental history ----------------------------------------------------------

    /// All completed rentals, oldest first.
    std::vector<RentalRecord> getHistory() const;

    /// Completed rentals whose plate, model, renter or telephone contains the query
    /// (case-insensitive). An empty query returns the whole history.
    std::vector<RentalRecord> searchHistory(const std::wstring& query) const;

    /// The most recent completed rental. @throws RentalException if the history is empty.
    const RentalRecord& getLastRecord() const;

    /// Sum of the charges of the given records.
    static Money totalCharged(const std::vector<RentalRecord>& records);

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

    /// Marks the data as not saved (used when storing a saved copy failed afterwards).
    void markUnsaved() noexcept;

    // ---- Users and access control --------------------------------------------------

    /// Logs a user in. @throws AuthenticationException
    const User& login(const std::wstring& username, const std::wstring& password);
    void logout() noexcept;

    /// The logged-in user, or nullptr.
    const User* getCurrentUser() const;

    /// True if the logged-in user may perform the operation (always true when nobody is logged in).
    bool isAllowed(Permission permission) const;

    /// @throws AccessDeniedException if the logged-in user may not perform the operation.
    void requirePermission(Permission permission) const;

    /// Manager only. @throws AccessDeniedException, ValidationException
    void addUser(const std::wstring& username, const std::wstring& password, Role role);
    void removeUser(const std::wstring& username);
    void resetPassword(const std::wstring& username, const std::wstring& newPassword);

    /// Any logged-in user. @throws ValidationException (wrong current password, invalid new password)
    void changeOwnPassword(const std::wstring& currentPassword, const std::wstring& newPassword);

    /// All accounts (salted password hashes only).
    std::vector<User> getUsers() const;

    /// True if a default account (manager/manager123, clerk/clerk123) still has its default password.
    bool usesDefaultPasswords() const;

    // ---- Persistence (through the abstract interface) --------------------------

    /// Replaces the current data with the data from the store.
    /// @return warnings about skipped entries (empty if everything was valid).
    std::vector<std::wstring> loadFrom(IDataStore& store);

    /// Writes all data to the store and clears the "unsaved changes" flag.
    void saveTo(IDataStore& store);

private:
    Vehicle& findMutableVehicle(const std::wstring& plate);
    static std::wstring makeKey(const std::wstring& plate);
    int nextReceiptNumber() const noexcept;

    std::map<std::wstring, Vehicle> m_fleet; ///< Key = upper-case plate (unique, sorted).
    Money m_revenue;                         ///< Cumulative rental revenue.
    std::vector<RentalRecord> m_history;     ///< Completed rentals, oldest first.
    UserDirectory m_users;                   ///< User accounts.
    std::optional<std::wstring> m_currentUser; ///< User name of the logged-in user.
    bool m_modified = false;                 ///< Unsaved changes?
};
