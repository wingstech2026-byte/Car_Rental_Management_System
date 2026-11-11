// =============================================================================
// Money.cpp - parsing, formatting and arithmetic for the Money value class.
// =============================================================================
#include "Money.h"

#include "RentalExceptions.h"
#include "TextUtils.h"

#include <limits>
#include <ostream>
#include <stdexcept>

namespace
{
    /// Largest number of digits accepted before the decimal point (999,999,999,999).
    constexpr std::size_t MaxIntegerDigits = 12;

    /// Inserts ',' every three digits from the right: "1234567" -> "1,234,567".
    std::wstring groupThousands(const std::wstring& digits)
    {
        std::wstring result;
        const std::size_t length = digits.size();
        for (std::size_t index = 0; index < length; ++index)
        {
            if (index > 0 && (length - index) % 3 == 0)
            {
                result += L',';
            }
            result += digits[index];
        }
        return result;
    }

    /// Formats an amount in cents with exactly two decimals.
    std::wstring formatCents(long long cents, bool grouped)
    {
        const bool negative = cents < 0;
        // Work with the unsigned magnitude so that even the smallest long long is safe.
        const unsigned long long magnitude = negative
            ? 0ULL - static_cast<unsigned long long>(cents)
            : static_cast<unsigned long long>(cents);

        std::wstring whole = std::to_wstring(magnitude / 100ULL);
        if (grouped)
        {
            whole = groupThousands(whole);
        }
        const unsigned long long fraction = magnitude % 100ULL;

        std::wstring result;
        if (negative)
        {
            result += L'-';
        }
        result += whole;
        result += L'.';
        if (fraction < 10ULL)
        {
            result += L'0';
        }
        result += std::to_wstring(fraction);
        return result;
    }
}

Money Money::fromCents(long long amountInCents) noexcept
{
    return Money(amountInCents);
}

std::optional<Money> Money::tryParse(const std::wstring& rawText)
{
    const std::wstring text = TextUtils::trim(rawText);
    std::size_t position = 0;

    // Integer part: at least one digit, at most MaxIntegerDigits.
    long long whole = 0;
    std::size_t integerDigits = 0;
    while (position < text.size() && TextUtils::isAsciiDigit(text[position]))
    {
        if (++integerDigits > MaxIntegerDigits)
        {
            return std::nullopt;
        }
        whole = whole * 10 + (text[position] - L'0');
        ++position;
    }
    if (integerDigits == 0)
    {
        return std::nullopt; // empty, sign, letter, or ".5"
    }

    // Optional fractional part: '.' followed by one or two digits.
    long long fraction = 0;
    if (position < text.size())
    {
        if (text[position] != L'.')
        {
            return std::nullopt; // e.g. "1,250" or "12abc"
        }
        ++position;
        std::size_t fractionDigits = 0;
        while (position < text.size() && TextUtils::isAsciiDigit(text[position]))
        {
            if (++fractionDigits > 2)
            {
                return std::nullopt; // more than two decimal places
            }
            fraction = fraction * 10 + (text[position] - L'0');
            ++position;
        }
        if (fractionDigits == 0 || position != text.size())
        {
            return std::nullopt; // "150." or trailing garbage
        }
        if (fractionDigits == 1)
        {
            fraction *= 10; // "150.5" means 50 cents
        }
    }
    return Money(whole * 100 + fraction);
}

Money Money::parse(const std::wstring& text)
{
    const std::optional<Money> parsed = tryParse(text);
    if (!parsed)
    {
        throw ValidationException(InputField::None,
            L"\"" + TextUtils::trim(text) + L"\" is not a valid amount. Use digits with an optional decimal "
            L"point and at most 2 decimal places (e.g. 150 or 149.99).");
    }
    return *parsed;
}

std::wstring Money::toString() const
{
    return formatCents(m_cents, false);
}

std::wstring Money::toDisplayString() const
{
    return formatCents(m_cents, true);
}

Money Money::operator+(const Money& other) const
{
    const long long maximum = std::numeric_limits<long long>::max();
    const long long minimum = std::numeric_limits<long long>::min();
    if ((other.m_cents > 0 && m_cents > maximum - other.m_cents) ||
        (other.m_cents < 0 && m_cents < minimum - other.m_cents))
    {
        throw std::overflow_error("Money addition overflow.");
    }
    return Money(m_cents + other.m_cents);
}

Money& Money::operator+=(const Money& other)
{
    *this = *this + other;
    return *this;
}

Money Money::operator*(int multiplier) const
{
    if (multiplier < 0)
    {
        throw std::invalid_argument("Money can only be multiplied by a non-negative number.");
    }
    if (multiplier > 0)
    {
        const long long limit = std::numeric_limits<long long>::max() / multiplier;
        if (m_cents > limit || m_cents < -limit)
        {
            throw std::overflow_error("Money multiplication overflow.");
        }
    }
    return Money(m_cents * multiplier);
}

std::wostream& operator<<(std::wostream& out, const Money& money)
{
    return out << money.toString();
}
