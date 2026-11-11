// =============================================================================
// WebBindings.cpp
// -----------------------------------------------------------------------------
// Bridge between the web page and the C++ business logic.
//
// The web demo uses the SAME business classes as the desktop program
// (RentalAgency, Vehicle, Rental, Renter, Money, Validation, the exception
// hierarchy and TextFileDataStore). They are compiled to WebAssembly with
// Emscripten; only the user interface is replaced by HTML.
//
// WebApp plays the role of the desktop GUI classes: each method performs
// exactly the same steps as the corresponding dialog's onOk() and returns the
// result as a small JSON string. Business exceptions are caught here and
// returned as {"ok":false,"field":...,"message":...}, just like
// ModalDialog::handleOk() shows them in a message box.
//
// Persistence: TextFileDataStore reads and writes /data/vehicles.txt and
// /data/revenue.txt in Emscripten's in-memory file system; the page copies
// those files to and from the browser's localStorage.
// =============================================================================
#include "BusinessRuleSelfTest.h"
#include "IDataStore.h"
#include "Money.h"
#include "Receipt.h"
#include "RentalAgency.h"
#include "RentalExceptions.h"
#include "RentalRecord.h"
#include "Renter.h"
#include "TextFileDataStore.h"
#include "TextUtils.h"
#include "Validation.h"
#include "Vehicle.h"

#include <emscripten/bind.h>

#include <cstdio>
#include <exception>
#include <string>
#include <vector>

namespace
{
    // ---- Minimal JSON writer ------------------------------------------------

    std::string jsonString(const std::string& utf8)
    {
        std::string out = "\"";
        for (const char character : utf8)
        {
            switch (character)
            {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(character) < 0x20)
                {
                    char buffer[8];
                    std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(character));
                    out += buffer;
                }
                else
                {
                    out += character;
                }
            }
        }
        return out + "\"";
    }

    std::string jsonString(const std::wstring& text)
    {
        return jsonString(TextUtils::toUtf8(text));
    }

    std::wstring fromJs(const std::string& utf8)
    {
        return TextUtils::fromUtf8Lenient(utf8);
    }

    const char* fieldName(InputField field)
    {
        switch (field)
        {
        case InputField::Plate: return "plate";
        case InputField::Model: return "model";
        case InputField::DailyRate: return "rate";
        case InputField::RenterName: return "renter";
        case InputField::Phone: return "phone";
        case InputField::AgreedRate: return "agreed";
        case InputField::RentalDays: return "days";
        default: return "";
        }
    }

    std::string failure(const std::wstring& message, const char* field)
    {
        return std::string("{\"ok\":false,\"field\":\"") + field + "\",\"message\":" + jsonString(message) + "}";
    }

    /// Runs an action that returns the JSON body of a successful result.
    /// Business exceptions become {"ok":false,...} - the web equivalent of
    /// ModalDialog::handleOk() showing the message and focusing the field.
    template <typename Action>
    std::string guarded(Action action)
    {
        try
        {
            const std::string body = action();
            return body.empty() ? "{\"ok\":true}" : "{\"ok\":true," + body + "}";
        }
        catch (const ValidationException& error)
        {
            return failure(error.message(), fieldName(error.field()));
        }
        catch (const DuplicatePlateException& error)
        {
            return failure(error.message(), "plate");
        }
        catch (const RentalException& error)
        {
            return failure(error.message(), "");
        }
        catch (const std::exception& error)
        {
            return failure(TextUtils::fromUtf8Lenient(error.what()), "");
        }
    }

    std::string vehicleJson(const Vehicle& vehicle)
    {
        std::string json = "{\"plate\":" + jsonString(vehicle.getPlate()) +
                           ",\"model\":" + jsonString(vehicle.getModel()) +
                           ",\"rate\":" + jsonString(vehicle.getDailyRate().toDisplayString()) +
                           ",\"ratePlain\":" + jsonString(vehicle.getDailyRate().toString()) +
                           ",\"rateCents\":" + std::to_string(vehicle.getDailyRate().cents()) +
                           ",\"status\":" + jsonString(toString(vehicle.getStatus()));
        if (const std::optional<Rental>& rental = vehicle.getRental())
        {
            json += ",\"renter\":" + jsonString(rental->getRenter().getName()) +
                    ",\"phone\":" + jsonString(rental->getRenter().getPhone()) +
                    ",\"agreed\":" + jsonString(rental->getAgreedDailyRate().toDisplayString()) +
                    ",\"agreedCents\":" + std::to_string(rental->getAgreedDailyRate().cents());
        }
        return json + "}";
    }

    std::string stringArray(const std::vector<std::wstring>& items)
    {
        std::string json = "[";
        for (std::size_t index = 0; index < items.size(); ++index)
        {
            json += (index > 0 ? "," : "") + jsonString(items[index]);
        }
        return json + "]";
    }
}

class WebApp
{
public:
    WebApp() : m_store("/data") {}

    /// Loads /data/*.txt through the IDataStore interface (polymorphism).
    std::string load()
    {
        return guarded([this] {
            IDataStore& store = m_store;
            const std::vector<std::wstring> warnings = m_agency.loadFrom(store);
            return "\"warnings\":" + stringArray(warnings);
        });
    }

    /// File > Save: writes /data/*.txt through the IDataStore interface.
    std::string save()
    {
        return guarded([this] {
            IDataStore& store = m_store;
            m_agency.saveTo(store);
            return std::string();
        });
    }

    /// Everything the page needs to draw the three tabs and the status bar.
    std::string state() const
    {
        std::string vehicles = "[";
        bool first = true;
        for (const Vehicle& vehicle : m_agency.getAllVehicles())
        {
            vehicles += (first ? "" : ",") + vehicleJson(vehicle);
            first = false;
        }
        vehicles += "]";
        return "{\"vehicles\":" + vehicles +
               ",\"revenue\":" + jsonString(m_agency.getRevenue().toDisplayString()) +
               ",\"total\":" + std::to_string(m_agency.getVehicleCount()) +
               ",\"available\":" + std::to_string(m_agency.getAvailableCount()) +
               ",\"rented\":" + std::to_string(m_agency.getRentedCount()) +
               ",\"modified\":" + (m_agency.hasUnsavedChanges() ? "true" : "false") + "}";
    }

    /// Function 3: plates of the available vehicles whose model matches.
    std::string search(const std::string& query) const
    {
        std::vector<std::wstring> plates;
        for (const Vehicle& vehicle : m_agency.searchAvailableByModel(fromJs(query)))
        {
            plates.push_back(vehicle.getPlate());
        }
        return "{\"plates\":" + stringArray(plates) + "}";
    }

    /// Function 1 - same steps as AddEditVehicleDialog::onOk() in add mode.
    std::string addVehicle(const std::string& plateText, const std::string& modelText, const std::string& rateText)
    {
        return guarded([&] {
            const std::wstring plate = Validation::normalizePlate(fromJs(plateText));
            const std::wstring model = Validation::validateModel(fromJs(modelText));
            const Money rate = Validation::parseRate(fromJs(rateText), InputField::DailyRate);
            const Vehicle& added = m_agency.addVehicle(plate, model, rate);
            return "\"plate\":" + jsonString(added.getPlate());
        });
    }

    /// Checks that a vehicle may be edited (throws for a rented vehicle).
    std::string checkEditable(const std::string& plate) const
    {
        return guarded([&] { return "\"vehicle\":" + vehicleJson(m_agency.findEditableVehicle(fromJs(plate))); });
    }

    /// Function 2 - same steps as AddEditVehicleDialog::onOk() in edit mode.
    std::string editVehicle(const std::string& plate, const std::string& modelText, const std::string& rateText)
    {
        return guarded([&] {
            const std::wstring model = Validation::validateModel(fromJs(modelText));
            const Money rate = Validation::parseRate(fromJs(rateText), InputField::DailyRate);
            m_agency.editVehicle(fromJs(plate), model, rate);
            return std::string();
        });
    }

    /// Checks that a vehicle may be rented (throws if it is already rented).
    std::string checkRentable(const std::string& plate) const
    {
        return guarded([&] { return "\"vehicle\":" + vehicleJson(m_agency.findRentableVehicle(fromJs(plate))); });
    }

    /// Function 4 - same steps as RentDialog::onOk().
    std::string rentVehicle(const std::string& plate, const std::string& name, const std::string& phone,
                            const std::string& agreedText)
    {
        return guarded([&] {
            const Renter renter(fromJs(name), fromJs(phone));
            const Money agreedRate = Validation::parseRate(fromJs(agreedText), InputField::AgreedRate);
            m_agency.rentVehicle(fromJs(plate), renter, agreedRate);
            return "\"renter\":" + jsonString(renter.getName());
        });
    }

    /// Checks that a vehicle may be returned (throws if it is not rented).
    std::string checkReturnable(const std::string& plate) const
    {
        return guarded([&] { return "\"vehicle\":" + vehicleJson(m_agency.findRentedVehicle(fromJs(plate))); });
    }

    /// Live preview, calculated by Rental::calculateCharge() like ReturnDialog::updatePreview().
    std::string previewCharge(const std::string& plate, const std::string& daysText) const
    {
        return guarded([&] {
            const Vehicle& vehicle = m_agency.findRentedVehicle(fromJs(plate));
            const int days = Validation::parseRentalDays(fromJs(daysText));
            const Rental& rental = *vehicle.getRental();
            const Money charge = rental.calculateCharge(days);
            return "\"days\":" + std::to_string(days) +
                   ",\"rate\":" + jsonString(rental.getAgreedDailyRate().toDisplayString()) +
                   ",\"charge\":" + jsonString(charge.toDisplayString());
        });
    }

    /// Function 5 - same steps as ReturnDialog::onOk(); returns the receipt.
    std::string returnVehicle(const std::string& plate, const std::string& daysText)
    {
        return guarded([&] {
            const int days = Validation::parseRentalDays(fromJs(daysText));
            const Receipt receipt = m_agency.returnVehicle(fromJs(plate), days);
            const RentalRecord& record = m_agency.getLastRecord(); // the return was archived
            return "\"receipt\":{\"number\":" + std::to_string(record.getNumber()) +
                   ",\"returnedOn\":" + jsonString(record.getReturnedOn()) +
                   ",\"plate\":" + jsonString(receipt.getPlate()) +
                   ",\"model\":" + jsonString(receipt.getModel()) +
                   ",\"renter\":" + jsonString(receipt.getRenterName()) +
                   ",\"phone\":" + jsonString(receipt.getPhone()) +
                   ",\"rate\":" + jsonString(receipt.getAgreedDailyRate().toDisplayString()) +
                   ",\"days\":" + std::to_string(receipt.getDays()) +
                   ",\"total\":" + jsonString(receipt.getTotalCharge().toDisplayString()) +
                   ",\"text\":" + jsonString(receipt.toDisplayText()) + "}";
        });
    }

    /// Rental History tab: completed rentals matching the query, plus their total.
    std::string history(const std::string& query) const
    {
        const std::vector<RentalRecord> records = m_agency.searchHistory(fromJs(query));
        std::string json = "{\"records\":[";
        for (std::size_t index = 0; index < records.size(); ++index)
        {
            const RentalRecord& record = records[index];
            const Receipt& receipt = record.getReceipt();
            json += (index > 0 ? "," : "");
            json += "{\"number\":" + std::to_string(record.getNumber()) +
                    ",\"returnedOn\":" + jsonString(record.getReturnedOn()) +
                    ",\"plate\":" + jsonString(receipt.getPlate()) +
                    ",\"model\":" + jsonString(receipt.getModel()) +
                    ",\"renter\":" + jsonString(receipt.getRenterName()) +
                    ",\"phone\":" + jsonString(receipt.getPhone()) +
                    ",\"rate\":" + jsonString(receipt.getAgreedDailyRate().toDisplayString()) +
                    ",\"rateCents\":" + std::to_string(receipt.getAgreedDailyRate().cents()) +
                    ",\"days\":" + std::to_string(receipt.getDays()) +
                    ",\"total\":" + jsonString(receipt.getTotalCharge().toDisplayString()) +
                    ",\"totalCents\":" + std::to_string(receipt.getTotalCharge().cents()) + "}";
        }
        return json + "],\"count\":" + std::to_string(records.size()) +
               ",\"total\":" + jsonString(RentalAgency::totalCharged(records).toDisplayString()) + "}";
    }

    /// Help > Run Business Rule Self-Test.
    std::string runSelfTest() const
    {
        const BusinessRuleSelfTest selfTest{};
        const std::vector<SelfTestResult> results = selfTest.run();
        std::string json = "{\"results\":[";
        for (std::size_t index = 0; index < results.size(); ++index)
        {
            json += (index > 0 ? "," : "");
            json += "{\"name\":" + jsonString(results[index].name) +
                    ",\"passed\":" + (results[index].passed ? "true" : "false") +
                    ",\"detail\":" + jsonString(results[index].detail) + "}";
        }
        return json + "]}";
    }

private:
    RentalAgency m_agency;
    TextFileDataStore m_store;
};

EMSCRIPTEN_BINDINGS(car_rental)
{
    emscripten::class_<WebApp>("WebApp")
        .constructor<>()
        .function("load", &WebApp::load)
        .function("save", &WebApp::save)
        .function("state", &WebApp::state)
        .function("search", &WebApp::search)
        .function("addVehicle", &WebApp::addVehicle)
        .function("checkEditable", &WebApp::checkEditable)
        .function("editVehicle", &WebApp::editVehicle)
        .function("checkRentable", &WebApp::checkRentable)
        .function("rentVehicle", &WebApp::rentVehicle)
        .function("checkReturnable", &WebApp::checkReturnable)
        .function("previewCharge", &WebApp::previewCharge)
        .function("returnVehicle", &WebApp::returnVehicle)
        .function("history", &WebApp::history)
        .function("runSelfTest", &WebApp::runSelfTest);
}
