// =============================================================================
// Rental.cpp
// =============================================================================
#include "Rental.h"

#include "Validation.h"

#include <algorithm>
#include <utility>

Rental::Rental(Renter renter, const Money& agreedDailyRate, std::optional<DateTime> startDate)
    : m_renter(std::move(renter)), m_agreedDailyRate(agreedDailyRate), m_startDate(startDate)
{
    Validation::requireValidRate(m_agreedDailyRate, InputField::AgreedRate);
}

const Renter& Rental::getRenter() const noexcept
{
    return m_renter;
}

Money Rental::getAgreedDailyRate() const noexcept
{
    return m_agreedDailyRate;
}

const std::optional<DateTime>& Rental::getStartDate() const noexcept
{
    return m_startDate;
}

Money Rental::calculateCharge(int days) const
{
    Validation::requireValidDays(days);
    return m_agreedDailyRate * days; // exact integer arithmetic in cents
}

int Rental::rentalDaysUntil(const DateTime& end) const noexcept
{
    if (!m_startDate)
    {
        return 1;
    }
    return std::min(m_startDate->rentalDaysUntil(end), Validation::MaxRentalDays);
}
