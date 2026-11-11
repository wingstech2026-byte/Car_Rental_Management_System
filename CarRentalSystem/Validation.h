// =============================================================================
// Validation.h
// -----------------------------------------------------------------------------
// The validation rules of the system, in ONE place. Every rule is used both
// for manual entry (via the business classes) and when loading the data files,
// so a value that is rejected in a dialog is also rejected in a file.
//
// Every function throws ValidationException (with the offending InputField)
// when a rule is broken, and returns the cleaned-up value otherwise.
// =============================================================================
#pragma once

#include "Money.h"
#include "RentalExceptions.h"

#include <cstddef>
#include <string>

namespace Validation
{
    // ---- Limits -------------------------------------------------------------
    constexpr std::size_t MaxPlateLength = 15;
    constexpr std::size_t MaxModelLength = 60;
    constexpr std::size_t MaxRenterNameLength = 80;
    constexpr std::size_t MinPhoneLength = 6;
    constexpr std::size_t MaxPhoneLength = 20;
    constexpr int MaxRentalDays = 3650;               ///< 10 years; larger values are "absurd".
    constexpr long long MaxDailyRateCents = 100000000; ///< 1,000,000.00 per day.

    /// Field separator used in the data files. No text field may contain it.
    constexpr wchar_t FieldSeparator = L'|';

    /// Trims, checks (not empty, only A-Z, 0-9 and '-') and returns the plate in UPPER case.
    std::wstring normalizePlate(const std::wstring& rawPlate);

    /// Trims and checks the vehicle model (not empty, no '|', max length).
    std::wstring validateModel(const std::wstring& rawModel);

    /// Trims and checks the renter's name (not empty, no '|', max length).
    std::wstring validateRenterName(const std::wstring& rawName);

    /// Trims and checks a telephone number: 6-20 characters, digits with an
    /// optional leading '+', spaces or '-'.
    std::wstring validatePhone(const std::wstring& rawPhone);

    /// Checks that a daily rate is positive and not absurdly large.
    void requireValidRate(const Money& rate, InputField field);

    /// Parses a daily rate typed by the user ("150" or "149.99") and checks it.
    Money parseRate(const std::wstring& text, InputField field);

    /// Checks that a number of rental days is between 1 and MaxRentalDays.
    void requireValidDays(int days);

    /// Parses the number of rental days typed by the user. Rejects empty input,
    /// signs, decimals, letters, 0 and values above MaxRentalDays.
    int parseRentalDays(const std::wstring& text);
}
