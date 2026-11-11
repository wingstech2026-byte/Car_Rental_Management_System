// =============================================================================
// RentalExceptions.cpp - constructors and accessors of the exception hierarchy.
// Each subclass builds its own clear, user-friendly message.
// =============================================================================
#include "RentalExceptions.h"

#include "TextUtils.h"

// ---- RentalException --------------------------------------------------------

RentalException::RentalException(const std::wstring& text)
    : std::runtime_error(TextUtils::toUtf8(text)), m_message(text)
{
}

const std::wstring& RentalException::message() const noexcept
{
    return m_message;
}

// ---- ValidationException ----------------------------------------------------

ValidationException::ValidationException(InputField inputField, const std::wstring& text)
    : RentalException(text), m_field(inputField)
{
}

InputField ValidationException::field() const noexcept
{
    return m_field;
}

// ---- DuplicatePlateException ------------------------------------------------

DuplicatePlateException::DuplicatePlateException(const std::wstring& duplicatePlate)
    : RentalException(L"A vehicle with license plate " + duplicatePlate +
                      L" already exists. License plates must be unique (upper/lower case is ignored)."),
      m_plate(duplicatePlate)
{
}

const std::wstring& DuplicatePlateException::plate() const noexcept
{
    return m_plate;
}

// ---- VehicleNotFoundException -----------------------------------------------

VehicleNotFoundException::VehicleNotFoundException(const std::wstring& missingPlate)
    : RentalException(L"No vehicle with license plate \"" + missingPlate + L"\" exists.")
{
}

// ---- VehicleNotAvailableException -------------------------------------------

VehicleNotAvailableException::VehicleNotAvailableException(const std::wstring& vehiclePlate,
                                                           const std::wstring& attemptedAction)
    : RentalException(L"Vehicle " + vehiclePlate + L" is currently rented and cannot be " +
                      attemptedAction + L". Return the vehicle first.")
{
}

// ---- VehicleNotRentedException ----------------------------------------------

VehicleNotRentedException::VehicleNotRentedException(const std::wstring& vehiclePlate)
    : RentalException(L"Vehicle " + vehiclePlate + L" is not rented, so it cannot be returned.")
{
}

// ---- DataStoreException -----------------------------------------------------

DataStoreException::DataStoreException(const std::wstring& text)
    : RentalException(text)
{
}
