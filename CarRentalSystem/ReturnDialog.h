// =============================================================================
// ReturnDialog.h
// -----------------------------------------------------------------------------
// Dialog for returning a rented vehicle. Shows the rental details, asks for the
// number of days (edit box + up-down spin control, digits only) and shows a
// live preview of the charge, e.g. "Charge: 3 days x 150.00 = 450.00".
// The charge itself is always calculated by Rental::calculateCharge().
// =============================================================================
#pragma once

#include "GuiUtils.h"
#include "ModalDialog.h"
#include "Receipt.h"
#include "Vehicle.h"

#include <optional>

class RentalAgency;

class ReturnDialog final : public ModalDialog
{
public:
    /// @param suggestedDays pre-filled number of days (calculated from the rental start date).
    ReturnDialog(HINSTANCE instance, RentalAgency& agency, const Vehicle& vehicle, int suggestedDays);

    /// The receipt of the completed return (set after OK).
    const std::optional<Receipt>& receipt() const noexcept;

private:
    bool onInitDialog() override;
    bool onOk() override;
    void onCommand(int controlId, int notificationCode) override;
    int controlForField(InputField field) const override;

    void updatePreview() const;

    RentalAgency& m_agency;
    Vehicle m_vehicle; ///< Copy, used for display and the preview.
    int m_suggestedDays;
    std::optional<Receipt> m_receipt;
    Gui::FontHandle m_previewFont; ///< Bold font for the preview (RAII).
};
