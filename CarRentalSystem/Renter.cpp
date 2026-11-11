// =============================================================================
// Renter.cpp
// =============================================================================
#include "Renter.h"

#include "Validation.h"

Renter::Renter(const std::wstring& name, const std::wstring& phone)
    : m_name(Validation::validateRenterName(name)), // validated in field order: name first,
      m_phone(Validation::validatePhone(phone))     // then telephone
{
}

const std::wstring& Renter::getName() const noexcept
{
    return m_name;
}

const std::wstring& Renter::getPhone() const noexcept
{
    return m_phone;
}
