// =============================================================================
// Rental.h
// -----------------------------------------------------------------------------
// COMPOSITION: an active rental is made of a Renter and the daily rate that was
// agreed when the vehicle was rented. The agreed rate is stored separately from
// the vehicle's current daily rate, because the two may differ (discounts, or
// the vehicle's price changing later).
// =============================================================================
#pragma once

#include "Money.h"
#include "Renter.h"

class Rental
{
public:
    /// @throws ValidationException if the agreed daily rate is not positive.
    Rental(Renter renter, const Money& agreedDailyRate);

    const Renter& getRenter() const noexcept;
    Money getAgreedDailyRate() const noexcept;

    /// Charge for the given number of days: agreed daily rate x days.
    /// @throws ValidationException if days is not between 1 and Validation::MaxRentalDays.
    Money calculateCharge(int days) const;

private:
    Renter m_renter;
    Money m_agreedDailyRate;
};
