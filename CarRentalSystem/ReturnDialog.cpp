// =============================================================================
// ReturnDialog.cpp
// =============================================================================
#include "ReturnDialog.h"

#include "RentalAgency.h"
#include "Resource.h"
#include "Validation.h"

#include <commctrl.h>

namespace
{
    constexpr wchar_t MultiplicationSign = static_cast<wchar_t>(0x00D7); // the "x" sign used in the preview
}

ReturnDialog::ReturnDialog(HINSTANCE instance, RentalAgency& agency, const Vehicle& vehicle)
    : ModalDialog(instance, IDD_RETURN), m_agency(agency), m_vehicle(vehicle)
{
}

const std::optional<Receipt>& ReturnDialog::receipt() const noexcept
{
    return m_receipt;
}

bool ReturnDialog::onInitDialog()
{
    const HWND dialog = dialogHandle();

    setControlText(IDC_RET_PLATE, m_vehicle.getPlate());
    setControlText(IDC_RET_MODEL, m_vehicle.getModel());
    if (const std::optional<Rental>& rental = m_vehicle.getRental())
    {
        setControlText(IDC_RET_RENTER, rental->getRenter().getName());
        setControlText(IDC_RET_PHONE, rental->getRenter().getPhone());
        setControlText(IDC_RET_RATE, rental->getAgreedDailyRate().toDisplayString());
    }

    // Bold font for the charge preview, derived from the dialog's own (DPI-scaled) font.
    const HFONT dialogFont = reinterpret_cast<HFONT>(SendMessageW(dialog, WM_GETFONT, 0, 0));
    LOGFONTW fontInfo{};
    if (dialogFont != nullptr && GetObjectW(dialogFont, static_cast<int>(sizeof(fontInfo)), &fontInfo) != 0)
    {
        fontInfo.lfWeight = FW_BOLD;
        m_previewFont.reset(CreateFontIndirectW(&fontInfo));
        SendDlgItemMessageW(dialog, IDC_CHARGE_PREVIEW, WM_SETFONT,
                            reinterpret_cast<WPARAM>(m_previewFont.get()), TRUE);
    }

    // Spin control: range 1..MaxRentalDays, starts at 1.
    SendDlgItemMessageW(dialog, IDC_DAYS_SPIN, UDM_SETRANGE32, 1, Validation::MaxRentalDays);
    SendDlgItemMessageW(dialog, IDC_DAYS_SPIN, UDM_SETPOS32, 0, 1);
    limitText(IDC_DAYS_EDIT, 9);
    setControlText(IDC_DAYS_EDIT, L"1");

    updatePreview();
    focusControl(IDC_DAYS_EDIT);
    return false;
}

bool ReturnDialog::onOk()
{
    const int days = Validation::parseRentalDays(getControlText(IDC_DAYS_EDIT));
    m_receipt = m_agency.returnVehicle(m_vehicle.getPlate(), days);
    return true;
}

void ReturnDialog::onCommand(int controlId, int notificationCode)
{
    if (controlId == IDC_DAYS_EDIT && notificationCode == EN_CHANGE)
    {
        updatePreview(); // live preview while typing or using the spin arrows
    }
}

int ReturnDialog::controlForField(InputField field) const
{
    return field == InputField::RentalDays ? IDC_DAYS_EDIT : 0;
}

void ReturnDialog::updatePreview() const
{
    const std::optional<Rental>& rental = m_vehicle.getRental();
    if (!rental)
    {
        return;
    }

    std::wstring text;
    try
    {
        const int days = Validation::parseRentalDays(getControlText(IDC_DAYS_EDIT));
        const Money charge = rental->calculateCharge(days);
        text = L"Charge: " + std::to_wstring(days) + (days == 1 ? L" day " : L" days ") + std::wstring(1, MultiplicationSign) + L" " +
               rental->getAgreedDailyRate().toDisplayString() + L" = " + charge.toDisplayString();
    }
    catch (const ValidationException&)
    {
        text = L"Charge: enter a whole number of days from 1 to " + std::to_wstring(Validation::MaxRentalDays) + L".";
    }
    setControlText(IDC_CHARGE_PREVIEW, text);
}
