// =============================================================================
// Validation.cpp - implementation of the shared validation rules.
// =============================================================================
#include "Validation.h"

#include "TextUtils.h"

#include <algorithm>

namespace
{
    bool containsControlCharacter(const std::wstring& text)
    {
        return std::any_of(text.begin(), text.end(), [](wchar_t character) { return character < L' '; });
    }

    /// Common rules for every free-text field: trimmed, not empty, no '|',
    /// no control characters, not too long.
    std::wstring requireText(const std::wstring& raw, InputField field, const std::wstring& name,
                             std::size_t maxLength)
    {
        std::wstring text = TextUtils::trim(raw);
        if (text.empty())
        {
            throw ValidationException(field, name + L" must not be empty.");
        }
        if (text.find(Validation::FieldSeparator) != std::wstring::npos)
        {
            throw ValidationException(field, name +
                L" must not contain the '|' character (it is reserved as the file separator).");
        }
        if (containsControlCharacter(text))
        {
            throw ValidationException(field, name + L" contains invalid control characters.");
        }
        if (text.size() > maxLength)
        {
            throw ValidationException(field, name + L" must be at most " + std::to_wstring(maxLength) +
                                                 L" characters long.");
        }
        return text;
    }

    std::wstring rateFieldName(InputField field)
    {
        switch (field)
        {
        case InputField::AgreedRate:
            return L"Agreed daily rate";
        case InputField::DailyRate:
            return L"Daily rate";
        default:
            return L"Amount";
        }
    }
}

namespace Validation
{
    std::wstring normalizePlate(const std::wstring& rawPlate)
    {
        const std::wstring plate = requireText(rawPlate, InputField::Plate, L"License plate", MaxPlateLength);

        bool hasLetterOrDigit = false;
        for (const wchar_t character : plate)
        {
            if (TextUtils::isAsciiLetter(character) || TextUtils::isAsciiDigit(character))
            {
                hasLetterOrDigit = true;
            }
            else if (character != L'-')
            {
                throw ValidationException(InputField::Plate, L"License plate \"" + plate +
                    L"\" is invalid. Use only letters A-Z, digits 0-9 and '-' (no spaces).");
            }
        }
        if (!hasLetterOrDigit)
        {
            throw ValidationException(InputField::Plate, L"License plate must contain at least one letter or digit.");
        }
        return TextUtils::toUpper(plate);
    }

    std::wstring validateModel(const std::wstring& rawModel)
    {
        return requireText(rawModel, InputField::Model, L"Vehicle model", MaxModelLength);
    }

    std::wstring validateRenterName(const std::wstring& rawName)
    {
        return requireText(rawName, InputField::RenterName, L"Renter name", MaxRenterNameLength);
    }

    std::wstring validatePhone(const std::wstring& rawPhone)
    {
        const std::wstring phone = TextUtils::trim(rawPhone);
        if (phone.empty())
        {
            throw ValidationException(InputField::Phone, L"Telephone number must not be empty.");
        }
        if (phone.size() < MinPhoneLength || phone.size() > MaxPhoneLength)
        {
            throw ValidationException(InputField::Phone, L"Telephone number must be " +
                std::to_wstring(MinPhoneLength) + L" to " + std::to_wstring(MaxPhoneLength) +
                L" characters long (you entered " + std::to_wstring(phone.size()) + L").");
        }

        bool hasDigit = false;
        for (std::size_t index = 0; index < phone.size(); ++index)
        {
            const wchar_t character = phone[index];
            if (TextUtils::isAsciiDigit(character))
            {
                hasDigit = true;
            }
            else if (character == L'+' && index == 0)
            {
                // A leading '+' (international prefix) is allowed.
            }
            else if (character != L' ' && character != L'-')
            {
                throw ValidationException(InputField::Phone,
                    L"Telephone number may contain only digits, spaces, '-' and an optional leading '+'.");
            }
        }
        if (!hasDigit)
        {
            throw ValidationException(InputField::Phone, L"Telephone number must contain digits.");
        }
        return phone;
    }

    void requireValidRate(const Money& rate, InputField field)
    {
        if (!rate.isPositive())
        {
            throw ValidationException(field, rateFieldName(field) + L" must be greater than 0.00.");
        }
        if (rate.cents() > MaxDailyRateCents)
        {
            throw ValidationException(field, rateFieldName(field) + L" must not exceed " +
                Money::fromCents(MaxDailyRateCents).toDisplayString() + L".");
        }
    }

    Money parseRate(const std::wstring& text, InputField field)
    {
        const std::wstring trimmed = TextUtils::trim(text);
        if (trimmed.empty())
        {
            throw ValidationException(field, rateFieldName(field) + L" must not be empty.");
        }
        const std::optional<Money> rate = Money::tryParse(trimmed);
        if (!rate)
        {
            throw ValidationException(field, rateFieldName(field) +
                L" must be a positive number with at most 2 decimal places, without thousands separators "
                L"(e.g. 150 or 149.99). You entered \"" + trimmed + L"\".");
        }
        requireValidRate(*rate, field);
        return *rate;
    }

    void requireValidDays(int days)
    {
        if (days < 1)
        {
            throw ValidationException(InputField::RentalDays, L"Rental days must be at least 1.");
        }
        if (days > MaxRentalDays)
        {
            throw ValidationException(InputField::RentalDays, L"Rental days must not exceed " +
                std::to_wstring(MaxRentalDays) + L" (10 years).");
        }
    }

    int parseRentalDays(const std::wstring& text)
    {
        const std::wstring trimmed = TextUtils::trim(text);
        if (trimmed.empty())
        {
            throw ValidationException(InputField::RentalDays, L"Please enter the number of rental days.");
        }
        for (const wchar_t character : trimmed)
        {
            if (!TextUtils::isAsciiDigit(character))
            {
                throw ValidationException(InputField::RentalDays,
                    L"Rental days must be a positive whole number (digits only - no signs, decimals or letters). "
                    L"You entered \"" + trimmed + L"\".");
            }
        }
        if (trimmed.size() > 9)
        {
            throw ValidationException(InputField::RentalDays, L"Rental days must not exceed " +
                std::to_wstring(MaxRentalDays) + L" (10 years).");
        }

        long long value = 0;
        for (const wchar_t character : trimmed)
        {
            value = value * 10 + (character - L'0');
        }
        if (value > MaxRentalDays)
        {
            throw ValidationException(InputField::RentalDays, L"Rental days must not exceed " +
                std::to_wstring(MaxRentalDays) + L" (10 years).");
        }
        const int days = static_cast<int>(value);
        requireValidDays(days);
        return days;
    }

    std::wstring normalizeUsername(const std::wstring& rawUsername)
    {
        const std::wstring username = TextUtils::toLower(TextUtils::trim(rawUsername));
        if (username.size() < MinUsernameLength || username.size() > MaxUsernameLength)
        {
            throw ValidationException(InputField::Username, L"User name must be " + std::to_wstring(MinUsernameLength) +
                L" to " + std::to_wstring(MaxUsernameLength) + L" characters long.");
        }
        for (const wchar_t character : username)
        {
            const bool allowed = (character >= L'a' && character <= L'z') || TextUtils::isAsciiDigit(character) ||
                                 character == L'.' || character == L'_' || character == L'-';
            if (!allowed)
            {
                throw ValidationException(InputField::Username,
                    L"User name may contain only letters a-z, digits, '.', '_' and '-' (no spaces).");
            }
        }
        return username;
    }

    void validatePassword(const std::wstring& password)
    {
        if (password.size() < MinPasswordLength || password.size() > MaxPasswordLength)
        {
            throw ValidationException(InputField::Password, L"Password must be " + std::to_wstring(MinPasswordLength) +
                L" to " + std::to_wstring(MaxPasswordLength) + L" characters long.");
        }
        if (password.find(FieldSeparator) != std::wstring::npos || containsControlCharacter(password))
        {
            throw ValidationException(InputField::Password,
                L"Password must not contain the '|' character or control characters.");
        }
    }
}
