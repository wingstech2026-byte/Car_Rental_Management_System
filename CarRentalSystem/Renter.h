// =============================================================================
// Renter.h
// -----------------------------------------------------------------------------
// ENCAPSULATION: a Renter is the person who rents a vehicle (name + phone).
// The constructor validates both values, so a Renter object that exists is
// always valid - there are no setters that could break that guarantee.
// =============================================================================
#pragma once

#include <string>

class Renter
{
public:
    /// Validates and stores the renter's details.
    /// @throws ValidationException if the name or telephone number is invalid.
    Renter(const std::wstring& name, const std::wstring& phone);

    const std::wstring& getName() const noexcept;
    const std::wstring& getPhone() const noexcept;

private:
    std::wstring m_name;
    std::wstring m_phone;
};
