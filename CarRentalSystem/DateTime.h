// =============================================================================
// DateTime.h
// -----------------------------------------------------------------------------
// Value class for a date and time with minute precision ("YYYY-MM-DD HH:MM").
//
// ENCAPSULATION: internally a DateTime is just the number of minutes since
// 1970-01-01 00:00 (local time). It can only be created through now(),
// tryParse() or fromParts(), which validate the calendar (month lengths, leap
// years), so an invalid date can never exist. Like Money, it is immutable and
// cheap to copy.
//
// Used for the rental start date and the return date, and to calculate the
// number of rental days automatically.
// =============================================================================
#pragma once

#include <optional>
#include <string>

class DateTime
{
public:
    /// 1970-01-01 00:00.
    DateTime() noexcept = default;

    /// The current local date and time (seconds are dropped).
    static DateTime now();

    /// Parses "YYYY-MM-DD HH:MM". Returns std::nullopt if the format or the date is invalid.
    static std::optional<DateTime> tryParse(const std::wstring& text);

    /// Builds a date from its parts. @throws ValidationException if the date does not exist.
    static DateTime fromParts(int year, int month, int day, int hour, int minute);

    /// "YYYY-MM-DD HH:MM" (sorts correctly as text).
    std::wstring toString() const;

    /// Minutes since 1970-01-01 00:00.
    long long totalMinutes() const noexcept { return m_minutes; }

    /// Number of rental days from this moment until 'end': every started
    /// 24 hours counts as one day, and the minimum is 1 day.
    int rentalDaysUntil(const DateTime& end) const noexcept;

    /// A new DateTime moved by the given number of minutes.
    DateTime plusMinutes(long long minutes) const noexcept;

    bool operator==(const DateTime& other) const noexcept { return m_minutes == other.m_minutes; }
    bool operator!=(const DateTime& other) const noexcept { return m_minutes != other.m_minutes; }
    bool operator<(const DateTime& other) const noexcept { return m_minutes < other.m_minutes; }
    bool operator<=(const DateTime& other) const noexcept { return m_minutes <= other.m_minutes; }
    bool operator>(const DateTime& other) const noexcept { return m_minutes > other.m_minutes; }
    bool operator>=(const DateTime& other) const noexcept { return m_minutes >= other.m_minutes; }

private:
    explicit DateTime(long long minutes) noexcept : m_minutes(minutes) {}

    long long m_minutes = 0; ///< Minutes since 1970-01-01 00:00 (local time).
};
