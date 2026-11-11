// =============================================================================
// RentalStatistics.cpp
// =============================================================================
#include "RentalStatistics.h"

#include "RentalAgency.h"
#include "TextUtils.h"

#include <algorithm>
#include <map>

RentalStatistics::RentalStatistics(const RentalAgency& agency)
    : m_vehicleCount(agency.getVehicleCount()),
      m_availableCount(agency.getAvailableCount()),
      m_rentedCount(agency.getRentedCount()),
      m_revenue(agency.getRevenue())
{
    // Expected daily income from the vehicles that are rented right now.
    for (const Vehicle& vehicle : agency.getRentedVehicles())
    {
        if (const std::optional<Rental>& rental = vehicle.getRental())
        {
            m_activeDailyIncome += rental->getAgreedDailyRate();
        }
    }

    // Totals from the rental history, grouped by model and by plate.
    std::map<std::wstring, ModelSummary> byModel;      // key: lower-case model (case-insensitive grouping)
    std::map<std::wstring, VehicleSummary> byVehicle;  // key: plate
    for (const RentalRecord& record : agency.getHistory())
    {
        const Receipt& receipt = record.getReceipt();
        ++m_completedRentals;
        m_totalCharged += receipt.getTotalCharge();
        m_totalDays += receipt.getDays();

        ModelSummary& model = byModel[TextUtils::toLower(receipt.getModel())];
        if (model.model.empty())
        {
            model.model = receipt.getModel();
        }
        ++model.rentals;
        model.days += receipt.getDays();
        model.revenue += receipt.getTotalCharge();

        VehicleSummary& vehicle = byVehicle[receipt.getPlate()];
        if (vehicle.plate.empty())
        {
            vehicle.plate = receipt.getPlate();
            vehicle.model = receipt.getModel();
        }
        ++vehicle.rentals;
        vehicle.revenue += receipt.getTotalCharge();
    }

    for (const auto& entry : byModel)
    {
        m_byModel.push_back(entry.second);
    }
    std::stable_sort(m_byModel.begin(), m_byModel.end(), [](const ModelSummary& a, const ModelSummary& b) {
        return a.revenue > b.revenue;
    });

    for (const auto& entry : byVehicle)
    {
        m_topVehicles.push_back(entry.second);
    }
    std::stable_sort(m_topVehicles.begin(), m_topVehicles.end(), [](const VehicleSummary& a, const VehicleSummary& b) {
        if (a.rentals != b.rentals)
        {
            return a.rentals > b.rentals;
        }
        return a.revenue > b.revenue;
    });
}

std::size_t RentalStatistics::getVehicleCount() const noexcept { return m_vehicleCount; }
std::size_t RentalStatistics::getAvailableCount() const noexcept { return m_availableCount; }
std::size_t RentalStatistics::getRentedCount() const noexcept { return m_rentedCount; }
Money RentalStatistics::getActiveDailyIncome() const noexcept { return m_activeDailyIncome; }
Money RentalStatistics::getRevenue() const noexcept { return m_revenue; }
std::size_t RentalStatistics::getCompletedRentals() const noexcept { return m_completedRentals; }
Money RentalStatistics::getTotalCharged() const noexcept { return m_totalCharged; }

int RentalStatistics::getUtilizationPercent() const noexcept
{
    if (m_vehicleCount == 0)
    {
        return 0;
    }
    // Integer arithmetic, rounded to the nearest percent.
    return static_cast<int>((m_rentedCount * 100 + m_vehicleCount / 2) / m_vehicleCount);
}

Money RentalStatistics::getAverageCharge() const noexcept
{
    if (m_completedRentals == 0)
    {
        return Money();
    }
    const long long count = static_cast<long long>(m_completedRentals);
    return Money::fromCents((m_totalCharged.cents() + count / 2) / count); // rounded to the cent
}

int RentalStatistics::getAverageDaysTimesTen() const noexcept
{
    if (m_completedRentals == 0)
    {
        return 0;
    }
    const long long count = static_cast<long long>(m_completedRentals);
    return static_cast<int>((m_totalDays * 10 + count / 2) / count);
}

std::wstring RentalStatistics::getAverageDaysText() const
{
    const int tenths = getAverageDaysTimesTen();
    return std::to_wstring(tenths / 10) + L"." + std::to_wstring(tenths % 10);
}

const std::vector<ModelSummary>& RentalStatistics::getRevenueByModel() const noexcept
{
    return m_byModel;
}

const std::vector<VehicleSummary>& RentalStatistics::getTopVehicles() const noexcept
{
    return m_topVehicles;
}
