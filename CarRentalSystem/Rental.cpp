// =============================================================================
// Rental.cpp
// =============================================================================
#include "Rental.h"

#include "Validation.h"

#include <utility>

Rental::Rental(Renter renter, const Money& agreedDailyRate)
    : m_renter(std::move(renter)), m_agreedDailyRate(agreedDailyRate)
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

Money Rental::calculateCharge(int days) const
{
    Validation::requireValidDays(days);
    return m_agreedDailyRate * days; // exact integer arithmetic in cents
}
