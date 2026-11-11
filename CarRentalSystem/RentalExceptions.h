// =============================================================================
// RentalExceptions.h
// -----------------------------------------------------------------------------
// EXCEPTION HANDLING + INHERITANCE
//
// All business-rule violations are reported by throwing an exception from the
// hierarchy below. The business classes never show message boxes - they only
// throw. The GUI catches the exceptions and displays message().
//
//   std::runtime_error
//     +-- RentalException                 (base of all rule violations)
//           +-- ValidationException        (bad input; knows which field)
//           +-- DuplicatePlateException    (plate already exists)
//           +-- VehicleNotFoundException   (unknown plate)
//           +-- VehicleNotAvailableException (vehicle is rented: cannot rent/edit)
//           +-- VehicleNotRentedException  (vehicle is not rented: cannot return)
//           +-- DataStoreException         (file could not be read/written)
//
// Because every class derives from RentalException, the GUI can catch the
// specific types it cares about (e.g. to focus the bad field) and handle all
// other rule violations with one catch (const RentalException&) - this is
// polymorphism applied to error handling.
// =============================================================================
#pragma once

#include <stdexcept>
#include <string>

/// Identifies the input field a ValidationException refers to, so that a
/// dialog can put the keyboard focus on the field that needs to be fixed.
enum class InputField
{
    None,
    Plate,
    Model,
    DailyRate,
    RenterName,
    Phone,
    AgreedRate,
    RentalDays
};

/// Base class of all exceptions thrown by the rental business logic.
/// Stores the message as UTF-16 (for the Unicode GUI) and also passes a UTF-8
/// copy to std::runtime_error so what() keeps working.
class RentalException : public std::runtime_error
{
public:
    explicit RentalException(const std::wstring& text);

    /// The user-friendly error message.
    const std::wstring& message() const noexcept;

private:
    std::wstring m_message;
};

/// Thrown when user input or file data breaks a validation rule.
class ValidationException : public RentalException
{
public:
    ValidationException(InputField inputField, const std::wstring& text);

    /// The field that contains the invalid value (InputField::None if unknown).
    InputField field() const noexcept;

private:
    InputField m_field;
};

/// Thrown when a vehicle is added with a plate that already exists.
class DuplicatePlateException : public RentalException
{
public:
    explicit DuplicatePlateException(const std::wstring& duplicatePlate);
    const std::wstring& plate() const noexcept;

private:
    std::wstring m_plate;
};

/// Thrown when no vehicle with the given plate exists.
class VehicleNotFoundException : public RentalException
{
public:
    explicit VehicleNotFoundException(const std::wstring& missingPlate);
};

/// Thrown when an operation needs an AVAILABLE vehicle but the vehicle is rented
/// (renting it again, or editing it).
class VehicleNotAvailableException : public RentalException
{
public:
    VehicleNotAvailableException(const std::wstring& vehiclePlate, const std::wstring& attemptedAction);
};

/// Thrown when a vehicle that is not rented is returned.
class VehicleNotRentedException : public RentalException
{
public:
    explicit VehicleNotRentedException(const std::wstring& vehiclePlate);
};

/// Thrown when the data files cannot be read or written.
class DataStoreException : public RentalException
{
public:
    explicit DataStoreException(const std::wstring& text);
};
