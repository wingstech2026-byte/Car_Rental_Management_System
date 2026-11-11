// =============================================================================
// Rental.h
// -----------------------------------------------------------------------------
// COMPOSITION: an active rental is made of a Renter, the daily rate that was
// agreed when the vehicle was rented, and the date and time the rental started.
// The agreed rate is stored separately from the vehicle's current daily rate,
// because the two may differ (discounts, or the vehicle's price changing later).
// =============================================================================
#pragma once

#include "DateTime.h"
#include "Money.h"
#include "Renter.h"

#include <optional>

class Rental
{
public:
    /// @param startDate when the rental started (may be unknown for old data).
    /// @throws ValidationException if the agreed daily rate is not positive.
    Rental(Renter renter, const Money& agreedDailyRate, std::optional<DateTime> startDate = std::nullopt);

    const Renter& getRenter() const noexcept;
    Money getAgreedDailyRate() const noexcept;
    const std::optional<DateTime>& getStartDate() const noexcept;

    /// Charge for the given number of days: agreed daily rate x days.
    /// @throws ValidationException if days is not between 1 and Validation::MaxRentalDays.
    Money calculateCharge(int days) const;

    /// Rental days from the start date until 'end' (every started 24 hours
    /// counts, at least 1, at most Validation::MaxRentalDays). 1 if the start is unknown.
    int rentalDaysUntil(const DateTime& end) const noexcept;

private:
    Renter m_renter;
    Money m_agreedDailyRate;
    std::optional<DateTime> m_startDate;
};
