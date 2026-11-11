// =============================================================================
// RentalRecord.h
// -----------------------------------------------------------------------------
// One entry of the rental history: a completed rental, created by
// RentalAgency::returnVehicle(). It is made of (COMPOSITION) a sequential
// receipt number, the date/time of the return and the Receipt itself.
//
// ENCAPSULATION: the constructor validates the number and the timestamp, and
// there are no setters - a record never changes after it has been created.
// =============================================================================
#pragma once

#include "Receipt.h"

#include <string>

class RentalRecord
{
public:
    /// @throws ValidationException if the number is not positive or the
    ///         timestamp is not in the format "YYYY-MM-DD HH:MM".
    RentalRecord(int number, std::wstring returnedOn, Receipt receipt);

    /// Sequential receipt number (1, 2, 3, ...).
    int getNumber() const noexcept;

    /// Date and time of the return, "YYYY-MM-DD HH:MM" (sorts correctly as text).
    const std::wstring& getReturnedOn() const noexcept;

    /// All details of the completed rental.
    const Receipt& getReceipt() const noexcept;

    /// True if the plate, model, renter name or telephone contains the query
    /// (case-insensitive). An empty query matches every record.
    bool matches(const std::wstring& query) const;

    /// The current local date and time in the format "YYYY-MM-DD HH:MM".
    static std::wstring currentTimestamp();

    /// Checks the format "YYYY-MM-DD HH:MM" (with plausible month/day/hour/minute).
    static bool isValidTimestamp(const std::wstring& text);

private:
    int m_number;
    std::wstring m_returnedOn;
    Receipt m_receipt;
};
