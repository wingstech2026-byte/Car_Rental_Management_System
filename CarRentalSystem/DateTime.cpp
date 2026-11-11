// =============================================================================
// DateTime.cpp
// -----------------------------------------------------------------------------
// Calendar conversions use Howard Hinnant's public-domain "days from civil"
// algorithms (proleptic Gregorian calendar), written with signed 64-bit
// arithmetic only.
// =============================================================================
#include "DateTime.h"

#include "RentalExceptions.h"
#include "TextUtils.h"

#include <climits>
#include <ctime>

namespace
{
    constexpr long long MinutesPerDay = 24 * 60;

    bool isLeapYear(long long year) noexcept
    {
        return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    }

    int daysInMonth(long long year, int month) noexcept
    {
        static const int lengths[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        if (month == 2 && isLeapYear(year))
        {
            return 29;
        }
        return lengths[month - 1];
    }

    /// Days since 1970-01-01 for the given date.
    long long daysFromCivil(long long year, long long month, long long day) noexcept
    {
        year -= (month <= 2) ? 1 : 0;
        const long long era = (year >= 0 ? year : year - 399) / 400;
        const long long yearOfEra = year - era * 400;
        const long long dayOfYear = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
        const long long dayOfEra = yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
        return era * 146097 + dayOfEra - 719468;
    }

    struct CivilDate
    {
        long long year;
        int month;
        int day;
    };

    /// Date for the given number of days since 1970-01-01.
    CivilDate civilFromDays(long long days) noexcept
    {
        days += 719468;
        const long long era = (days >= 0 ? days : days - 146096) / 146097;
        const long long dayOfEra = days - era * 146097;
        const long long yearOfEra = (dayOfEra - dayOfEra / 1460 + dayOfEra / 36524 - dayOfEra / 146096) / 365;
        const long long dayOfYear = dayOfEra - (365 * yearOfEra + yearOfEra / 4 - yearOfEra / 100);
        const long long monthIndex = (5 * dayOfYear + 2) / 153;
        const long long day = dayOfYear - (153 * monthIndex + 2) / 5 + 1;
        const long long month = monthIndex < 10 ? monthIndex + 3 : monthIndex - 9;
        const long long year = yearOfEra + era * 400 + (month <= 2 ? 1 : 0);
        return CivilDate{ year, static_cast<int>(month), static_cast<int>(day) };
    }

    /// Reads text[start .. start+count) as digits, or -1 if any character is not a digit.
    int readDigits(const std::wstring& text, std::size_t start, std::size_t count)
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

    std::wstring twoDigits(int value)
    {
        return (value < 10 ? L"0" : L"") + std::to_wstring(value);
    }

    bool isValidDate(int year, int month, int day, int hour, int minute) noexcept
    {
        return year >= 1900 && year <= 9999 && month >= 1 && month <= 12 && day >= 1 &&
               day <= daysInMonth(year, month) && hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59;
    }
}

DateTime DateTime::now()
{
    const std::time_t current = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &current);
#else
    localtime_r(&current, &local);
#endif
    return fromParts(local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour, local.tm_min);
}

std::optional<DateTime> DateTime::tryParse(const std::wstring& rawText)
{
    const std::wstring text = TextUtils::trim(rawText);
    // Expected layout: YYYY-MM-DD HH:MM (16 characters)
    if (text.size() != 16 || text[4] != L'-' || text[7] != L'-' || text[10] != L' ' || text[13] != L':')
    {
        return std::nullopt;
    }
    const int year = readDigits(text, 0, 4);
    const int month = readDigits(text, 5, 2);
    const int day = readDigits(text, 8, 2);
    const int hour = readDigits(text, 11, 2);
    const int minute = readDigits(text, 14, 2);
    if (!isValidDate(year, month, day, hour, minute))
    {
        return std::nullopt;
    }
    return fromParts(year, month, day, hour, minute);
}

DateTime DateTime::fromParts(int year, int month, int day, int hour, int minute)
{
    if (!isValidDate(year, month, day, hour, minute))
    {
        throw ValidationException(InputField::None, L"The date " + std::to_wstring(year) + L"-" + twoDigits(month) +
                                                    L"-" + twoDigits(day) + L" " + twoDigits(hour) + L":" +
                                                    twoDigits(minute) + L" does not exist.");
    }
    const long long days = daysFromCivil(year, month, day);
    return DateTime(days * MinutesPerDay + hour * 60LL + minute);
}

std::wstring DateTime::toString() const
{
    long long days = m_minutes / MinutesPerDay;
    long long minuteOfDay = m_minutes % MinutesPerDay;
    if (minuteOfDay < 0)
    {
        minuteOfDay += MinutesPerDay;
        --days;
    }
    const CivilDate date = civilFromDays(days);
    return std::to_wstring(date.year) + L"-" + twoDigits(date.month) + L"-" + twoDigits(date.day) + L" " +
           twoDigits(static_cast<int>(minuteOfDay / 60)) + L":" + twoDigits(static_cast<int>(minuteOfDay % 60));
}

int DateTime::rentalDaysUntil(const DateTime& end) const noexcept
{
    const long long difference = end.m_minutes - m_minutes;
    if (difference <= 0)
    {
        return 1;
    }
    const long long days = (difference + MinutesPerDay - 1) / MinutesPerDay; // every started day counts
    return days > INT_MAX ? INT_MAX : static_cast<int>(days);
}

DateTime DateTime::plusMinutes(long long minutes) const noexcept
{
    return DateTime(m_minutes + minutes);
}
