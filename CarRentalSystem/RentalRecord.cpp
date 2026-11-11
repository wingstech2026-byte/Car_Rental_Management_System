// =============================================================================
// RentalRecord.cpp
// =============================================================================
#include "RentalRecord.h"

#include "RentalExceptions.h"
#include "TextUtils.h"

#include <ctime>
#include <utility>

namespace
{
    /// Reads the digits text[start .. start+count) as a number, or -1 if not all digits.
    int readNumber(const std::wstring& text, std::size_t start, std::size_t count)
    {
        int value = 0;
        for (std::size_t index = start; index < start + count; ++index)
        {
            if (!TextUtils::isAsciiDigit(text[index]))
            {
                return -1;
            }
            value = value * 10 + (text[index] - L'0');
        }
        return value;
    }
}

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
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &local);

    std::wstring result;
    for (const char* character = buffer; *character != '\0'; ++character)
    {
        result += static_cast<wchar_t>(*character); // the format contains ASCII only
    }
    return result;
}

bool RentalRecord::isValidTimestamp(const std::wstring& text)
{
    // Expected layout: YYYY-MM-DD HH:MM  (16 characters)
    if (text.size() != 16 || text[4] != L'-' || text[7] != L'-' || text[10] != L' ' || text[13] != L':')
    {
        return false;
    }
    const int year = readNumber(text, 0, 4);
    const int month = readNumber(text, 5, 2);
    const int day = readNumber(text, 8, 2);
    const int hour = readNumber(text, 11, 2);
    const int minute = readNumber(text, 14, 2);
    return year >= 1900 && month >= 1 && month <= 12 && day >= 1 && day <= 31 &&
           hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
}
