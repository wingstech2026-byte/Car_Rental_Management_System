// =============================================================================
// Receipt.cpp
// =============================================================================
#include "Receipt.h"

#include <utility>

Receipt::Receipt(std::wstring plate, std::wstring model, std::wstring renterName, std::wstring phone,
                 const Money& agreedDailyRate, int days, const Money& totalCharge)
    : m_plate(std::move(plate)),
      m_model(std::move(model)),
      m_renterName(std::move(renterName)),
      m_phone(std::move(phone)),
      m_agreedDailyRate(agreedDailyRate),
      m_days(days),
      m_totalCharge(totalCharge)
{
}

const std::wstring& Receipt::getPlate() const noexcept { return m_plate; }
const std::wstring& Receipt::getModel() const noexcept { return m_model; }
const std::wstring& Receipt::getRenterName() const noexcept { return m_renterName; }
const std::wstring& Receipt::getPhone() const noexcept { return m_phone; }
Money Receipt::getAgreedDailyRate() const noexcept { return m_agreedDailyRate; }
int Receipt::getDays() const noexcept { return m_days; }
Money Receipt::getTotalCharge() const noexcept { return m_totalCharge; }

std::wstring Receipt::toDisplayText() const
{
    std::wstring text;
    text += L"RENTAL RECEIPT\n";
    text += L"==============================\n\n";
    text += L"License plate:  " + m_plate + L"\n";
    text += L"Model:  " + m_model + L"\n";
    text += L"Renter:  " + m_renterName + L"\n";
    text += L"Telephone:  " + m_phone + L"\n";
    text += L"Agreed daily rate:  " + m_agreedDailyRate.toDisplayString() + L"\n";
    text += L"Rental days:  " + std::to_wstring(m_days) + L"\n\n";
    text += L"TOTAL CHARGE:  " + m_agreedDailyRate.toDisplayString() + L" x " + std::to_wstring(m_days) +
            L" = " + m_totalCharge.toDisplayString() + L"\n";
    return text;
}
