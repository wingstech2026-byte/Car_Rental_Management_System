// =============================================================================
// RentalStatistics.h
// -----------------------------------------------------------------------------
// Reports and statistics about the fleet and the rental history.
//
// A RentalStatistics object is a read-only SNAPSHOT: the constructor reads the
// RentalAgency once (through its public, const interface only - it cannot change
// anything) and calculates every figure. The getters then just return them.
// This keeps reporting logic out of RentalAgency (single responsibility) and
// out of the GUI (separation of logic and interface).
// =============================================================================
#pragma once

#include "Money.h"

#include <cstddef>
#include <string>
#include <vector>

class RentalAgency;

/// Totals for one vehicle model, based on the rental history.
struct ModelSummary
{
    std::wstring model;
    int rentals = 0;   ///< Number of completed rentals.
    int days = 0;      ///< Total rental days.
    Money revenue;     ///< Total charged.
};

/// Totals for one vehicle (license plate), based on the rental history.
struct VehicleSummary
{
    std::wstring plate;
    std::wstring model;
    int rentals = 0;
    Money revenue;
};

class RentalStatistics
{
public:
    /// Calculates all figures from the agency's current fleet and history.
    explicit RentalStatistics(const RentalAgency& agency);

    // ---- Fleet --------------------------------------------------------------
    std::size_t getVehicleCount() const noexcept;
    std::size_t getAvailableCount() const noexcept;
    std::size_t getRentedCount() const noexcept;

    /// Share of the fleet that is rented right now, in percent (0 - 100).
    int getUtilizationPercent() const noexcept;

    /// Sum of the agreed daily rates of all active rentals (income per day while they last).
    Money getActiveDailyIncome() const noexcept;

    /// Cumulative rental revenue (as stored by the agency).
    Money getRevenue() const noexcept;

    // ---- Rental history -------------------------------------------------------
    std::size_t getCompletedRentals() const noexcept;
    Money getTotalCharged() const noexcept;

    /// Average charge per completed rental (0.00 if there are none), rounded to the cent.
    Money getAverageCharge() const noexcept;

    /// Average rental length in tenths of a day (e.g. 42 = 4.2 days), 0 if there are none.
    int getAverageDaysTimesTen() const noexcept;

    /// Average rental length as text with one decimal, e.g. "4.2".
    std::wstring getAverageDaysText() const;

    /// Revenue per model, highest revenue first.
    const std::vector<ModelSummary>& getRevenueByModel() const noexcept;

    /// Vehicles by number of rentals (then revenue), most rented first.
    const std::vector<VehicleSummary>& getTopVehicles() const noexcept;

private:
    std::size_t m_vehicleCount = 0;
    std::size_t m_availableCount = 0;
    std::size_t m_rentedCount = 0;
    Money m_activeDailyIncome;
    Money m_revenue;
    std::size_t m_completedRentals = 0;
    Money m_totalCharged;
    long long m_totalDays = 0;
    std::vector<ModelSummary> m_byModel;
    std::vector<VehicleSummary> m_topVehicles;
};
