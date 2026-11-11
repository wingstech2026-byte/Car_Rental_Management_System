// =============================================================================
// Receipt.h
// -----------------------------------------------------------------------------
// Read-only record of a completed rental, created by Vehicle::returnVehicle().
// It keeps a copy of all details, because the vehicle's rental information is
// cleared as soon as the vehicle is returned.
// =============================================================================
#pragma once

#include "Money.h"

#include <string>

class Receipt
{
public:
    Receipt(std::wstring plate, std::wstring model, std::wstring renterName, std::wstring phone,
            const Money& agreedDailyRate, int days, const Money& totalCharge);

    const std::wstring& getPlate() const noexcept;
    const std::wstring& getModel() const noexcept;
    const std::wstring& getRenterName() const noexcept;
    const std::wstring& getPhone() const noexcept;
    Money getAgreedDailyRate() const noexcept;
    int getDays() const noexcept;
    Money getTotalCharge() const noexcept;

    /// Multi-line text for the receipt message box.
    std::wstring toDisplayText() const;

private:
    std::wstring m_plate;
    std::wstring m_model;
    std::wstring m_renterName;
    std::wstring m_phone;
    Money m_agreedDailyRate;
    int m_days;
    Money m_totalCharge;
};
