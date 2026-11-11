// =============================================================================
// Money.h
// -----------------------------------------------------------------------------
// ENCAPSULATION + OPERATOR OVERLOADING
//
// Money is a small immutable VALUE CLASS. Internally it stores an integer
// number of cents (long long). float/double are never used, because binary
// floating point cannot represent values such as 0.10 exactly and rounding
// errors would accumulate in the revenue total.
//
// The cents value is private. Outside code can only create Money through
// fromCents() or parse(), and can only combine values through the overloaded
// operators, so a Money object can never be in an invalid state.
// =============================================================================
#pragma once

#include <iosfwd>
#include <optional>
#include <string>

class Money
{
public:
    /// Creates 0.00.
    Money() noexcept = default;

    /// Creates an amount from a whole number of cents (e.g. 15050 -> 150.50).
    static Money fromCents(long long amountInCents) noexcept;

    /// Parses "150", "150.5" or "150.50" (digits, optional '.', at most two
    /// decimals, no sign, no thousands separators). Returns std::nullopt if invalid.
    static std::optional<Money> tryParse(const std::wstring& text);

    /// Same as tryParse() but throws ValidationException if the text is invalid.
    static Money parse(const std::wstring& text);

    /// The raw amount in cents.
    long long cents() const noexcept { return m_cents; }

    bool isPositive() const noexcept { return m_cents > 0; }
    bool isZero() const noexcept { return m_cents == 0; }

    /// Plain format with exactly two decimals, e.g. "1250.00" (used in files and edit boxes).
    std::wstring toString() const;

    /// Display format with thousands separators and two decimals, e.g. "1,250.00".
    std::wstring toDisplayString() const;

    // ---- Arithmetic (throws std::overflow_error instead of silently wrapping)
    Money operator+(const Money& other) const;
    Money& operator+=(const Money& other);
    Money operator*(int multiplier) const;

    // ---- Comparison
    bool operator==(const Money& other) const noexcept { return m_cents == other.m_cents; }
    bool operator!=(const Money& other) const noexcept { return m_cents != other.m_cents; }
    bool operator<(const Money& other) const noexcept { return m_cents < other.m_cents; }
    bool operator<=(const Money& other) const noexcept { return m_cents <= other.m_cents; }
    bool operator>(const Money& other) const noexcept { return m_cents > other.m_cents; }
    bool operator>=(const Money& other) const noexcept { return m_cents >= other.m_cents; }

private:
    explicit Money(long long amountInCents) noexcept : m_cents(amountInCents) {}

    long long m_cents = 0; ///< The amount in cents - the only data member.
};

/// Writes the amount in plain format ("1250.00") - used when saving files.
std::wostream& operator<<(std::wostream& out, const Money& money);
