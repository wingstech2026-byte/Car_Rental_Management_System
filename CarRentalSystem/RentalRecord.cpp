// =============================================================================
// RentalRecord.cpp
// =============================================================================
#include "RentalRecord.h"

#include "DateTime.h"
#include "RentalExceptions.h"
#include "TextUtils.h"

#include <utility>

RentalRecord::RentalRecord(int number, std::wstring returnedOn, Receipt receipt)
    : m_number(number), m_returnedOn(TextUtils::trim(returnedOn)), m_receipt(std::move(receipt))
{
    if (m_number < 1)
    {
        throw ValidationException(InputField::None, L"Receipt number must be a positive whole number.");
    }
    if (!isValidTimestamp(m_returnedOn))
    {
        throw ValidationException(InputField::None, L"Return date \"" + m_returnedOn +
                                                    L"\" is invalid (expected YYYY-MM-DD HH:MM).");
    }
}

int RentalRecord::getNumber() const noexcept
{
    return m_number;
}

const std::wstring& RentalRecord::getReturnedOn() const noexcept
{
    return m_returnedOn;
}

const Receipt& RentalRecord::getReceipt() const noexcept
{
    return m_receipt;
}

bool RentalRecord::matches(const std::wstring& query) const
{
    const std::wstring trimmed = TextUtils::trim(query);
    return trimmed.empty() ||
           TextUtils::containsIgnoreCase(m_receipt.getPlate(), trimmed) ||
           TextUtils::containsIgnoreCase(m_receipt.getModel(), trimmed) ||
           TextUtils::containsIgnoreCase(m_receipt.getRenterName(), trimmed) ||
           TextUtils::containsIgnoreCase(m_receipt.getPhone(), trimmed);
}

std::wstring RentalRecord::currentTimestamp()
{
    return DateTime::now().toString();
}

bool RentalRecord::isValidTimestamp(const std::wstring& text)
{
    return DateTime::tryParse(text).has_value(); // full calendar check (month lengths, leap years)
}
