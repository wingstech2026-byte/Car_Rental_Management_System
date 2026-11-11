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
// Persistence: all data travels as one text "bundle" (DataFileFormat) through
// a BundleDataStore - an IDataStore like the desktop's TextFileDataStore. The
// page moves the bundle to and from the browser storage or the shared online
// database.
//
// Users and roles: login() authenticates against the C++ UserDirectory; from
// then on RentalAgency itself refuses operations the user's role does not allow.
// =============================================================================
#include "BundleDataStore.h"
#include "BusinessRuleSelfTest.h"
#include "DateTime.h"
#include "IDataStore.h"
#include "Money.h"
#include "Receipt.h"
#include "RentalAgency.h"
#include "RentalExceptions.h"
#include "RentalRecord.h"
#include "RentalStatistics.h"
#include "Renter.h"
#include "User.h"
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
        case InputField::Username: return "username";
        case InputField::Password: return "password";
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
                    ",\"agreedCents\":" + std::to_string(rental->getAgreedDailyRate().cents()) +
                    ",\"startDate\":" + jsonString(rental->getStartDate() ? rental->getStartDate()->toString() : std::wstring());
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
    /// Loads a data bundle through the IDataStore interface (polymorphism).
    /// An empty text means "no data yet" (the default user accounts are created).
    std::string loadBundle(const std::string& bundle)
    {
        return guarded([&] {
            m_store.setBundle(fromJs(bundle));
            IDataStore& store = m_store;
            const std::vector<std::wstring> warnings = m_agency.loadFrom(store);
            return "\"warnings\":" + stringArray(warnings);
        });
    }

    /// Save: writes all data into a bundle through the IDataStore interface and returns it.
    std::string saveBundle()
    {
        return guarded([this] {
            IDataStore& store = m_store;
            m_agency.saveTo(store);
            return "\"bundle\":" + jsonString(m_store.getBundle());
        });
    }

    /// Called by the page when storing the bundle failed (for example a conflict).
    void markUnsaved()
    {
        m_agency.markUnsaved();
    }

    // ---- Users and roles ------------------------------------------------------------

    std::string login(const std::string& username, const std::string& password)
    {
        return guarded([&] {
            m_agency.login(fromJs(username), fromJs(password));
            return sessionBody();
        });
    }

    std::string logout()
    {
        m_agency.logout();
        return "{\"ok\":true}";
    }

    std::string session() const
    {
        return "{" + sessionBody() + "}";
    }

    std::string users() const
    {
        return guarded([this] {
            m_agency.requirePermission(Permission::ManageUsers);
            std::string json = "\"users\":[";
            bool first = true;
            for (const User& user : m_agency.getUsers())
            {
                json += (first ? "" : ",");
                json += "{\"username\":" + jsonString(user.getUsername()) + ",\"role\":" + jsonString(toString(user.getRole())) + "}";
                first = false;
            }
            return json + "]";
        });
    }

    std::string addUser(const std::string& username, const std::string& password, const std::string& role)
    {
        return guarded([&] {
            Role parsedRole = Role::Clerk;
            if (!tryParseRole(fromJs(role), parsedRole))
            {
                throw ValidationException(InputField::None, L"Please choose a role.");
            }
            m_agency.addUser(fromJs(username), fromJs(password), parsedRole);
            return std::string();
        });
    }

    std::string removeUser(const std::string& username)
    {
        return guarded([&] {
            m_agency.removeUser(fromJs(username));
            return std::string();
        });
    }

    std::string resetPassword(const std::string& username, const std::string& password)
    {
        return guarded([&] {
            m_agency.resetPassword(fromJs(username), fromJs(password));
            return std::string();
        });
    }

    std::string changePassword(const std::string& currentPassword, const std::string& newPassword)
    {
        return guarded([&] {
            m_agency.changeOwnPassword(fromJs(currentPassword), fromJs(newPassword));
            return sessionBody();
        });
    }

    /// Rental days calculated from the rental start date (pre-filled in the return dialog).
    std::string suggestedDays(const std::string& plate) const
    {
        return guarded([&] { return "\"days\":" + std::to_string(m_agency.suggestedRentalDays(fromJs(plate))); });
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

    /// Removes an available vehicle (C++ refuses rented vehicles).
    std::string removeVehicle(const std::string& plate)
    {
        return guarded([&] {
            m_agency.removeVehicle(fromJs(plate));
            return std::string();
        });
    }

    /// Fleet search: plates of the vehicles matching the text and the status filter
    /// ("all", "available" or "rented").
    std::string fleet(const std::string& query, const std::string& filter) const
    {
        StatusFilter status = StatusFilter::All;
        if (filter == "available") status = StatusFilter::Available;
        if (filter == "rented") status = StatusFilter::Rented;
        std::vector<std::wstring> plates;
        for (const Vehicle& vehicle : m_agency.searchFleet(fromJs(query), status))
        {
            plates.push_back(vehicle.getPlate());
        }
        return "{\"plates\":" + stringArray(plates) + "}";
    }

    /// Dashboard and reports, calculated by the C++ RentalStatistics class.
    std::string statistics() const
    {
        return guarded([this] { return statisticsBody(); }); // Managers only (RentalStatistics checks)
    }

    std::string statisticsBody() const
    {
        const RentalStatistics stats(m_agency);
        std::string byModel = "[";
        for (std::size_t index = 0; index < stats.getRevenueByModel().size(); ++index)
        {
            const ModelSummary& model = stats.getRevenueByModel()[index];
            byModel += (index > 0 ? "," : "");
            byModel += "{\"model\":" + jsonString(model.model) +
                       ",\"rentals\":" + std::to_string(model.rentals) +
                       ",\"days\":" + std::to_string(model.days) +
                       ",\"revenue\":" + jsonString(model.revenue.toDisplayString()) +
                       ",\"revenueCents\":" + std::to_string(model.revenue.cents()) + "}";
        }
        byModel += "]";

        std::string topVehicles = "[";
        for (std::size_t index = 0; index < stats.getTopVehicles().size(); ++index)
        {
            const VehicleSummary& vehicle = stats.getTopVehicles()[index];
            topVehicles += (index > 0 ? "," : "");
            topVehicles += "{\"plate\":" + jsonString(vehicle.plate) +
                           ",\"model\":" + jsonString(vehicle.model) +
                           ",\"rentals\":" + std::to_string(vehicle.rentals) +
                           ",\"revenue\":" + jsonString(vehicle.revenue.toDisplayString()) +
                           ",\"revenueCents\":" + std::to_string(vehicle.revenue.cents()) + "}";
        }
        topVehicles += "]";

        return "\"vehicles\":" + std::to_string(stats.getVehicleCount()) +
               ",\"available\":" + std::to_string(stats.getAvailableCount()) +
               ",\"rented\":" + std::to_string(stats.getRentedCount()) +
               ",\"utilization\":" + std::to_string(stats.getUtilizationPercent()) +
               ",\"activeDailyIncome\":" + jsonString(stats.getActiveDailyIncome().toDisplayString()) +
               ",\"revenue\":" + jsonString(stats.getRevenue().toDisplayString()) +
               ",\"completedRentals\":" + std::to_string(stats.getCompletedRentals()) +
               ",\"totalCharged\":" + jsonString(stats.getTotalCharged().toDisplayString()) +
               ",\"averageCharge\":" + jsonString(stats.getAverageCharge().toDisplayString()) +
               ",\"averageDays\":" + jsonString(stats.getAverageDaysText()) +
               ",\"byModel\":" + byModel + ",\"topVehicles\":" + topVehicles;
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
                   ",\"startedOn\":" + jsonString(receipt.getStartedOn()) +
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
                    ",\"totalCents\":" + std::to_string(receipt.getTotalCharge().cents()) +
                    ",\"startedOn\":" + jsonString(receipt.getStartedOn()) + "}";
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
    /// "loggedIn", user name, role and the permissions of the logged-in user.
    std::string sessionBody() const
    {
        const User* user = m_agency.getCurrentUser();
        if (user == nullptr)
        {
            return "\"loggedIn\":false";
        }
        const auto flag = [this](Permission permission) { return m_agency.isAllowed(permission) ? "true" : "false"; };
        return std::string("\"loggedIn\":true,\"username\":") + jsonString(user->getUsername()) +
               ",\"role\":" + jsonString(toString(user->getRole())) +
               ",\"defaultPasswords\":" + (m_agency.usesDefaultPasswords() ? "true" : "false") +
               ",\"can\":{\"manageFleet\":" + flag(Permission::ManageFleet) +
               ",\"rentAndReturn\":" + flag(Permission::RentAndReturn) +
               ",\"viewReports\":" + flag(Permission::ViewReports) +
               ",\"manageUsers\":" + flag(Permission::ManageUsers) +
               ",\"manageData\":" + flag(Permission::ManageData) + "}";
    }

    RentalAgency m_agency;
    BundleDataStore m_store;
};

EMSCRIPTEN_BINDINGS(car_rental)
{
    emscripten::class_<WebApp>("WebApp")
        .constructor<>()
        .function("loadBundle", &WebApp::loadBundle)
        .function("saveBundle", &WebApp::saveBundle)
        .function("markUnsaved", &WebApp::markUnsaved)
        .function("login", &WebApp::login)
        .function("logout", &WebApp::logout)
        .function("session", &WebApp::session)
        .function("users", &WebApp::users)
        .function("addUser", &WebApp::addUser)
        .function("removeUser", &WebApp::removeUser)
        .function("resetPassword", &WebApp::resetPassword)
        .function("changePassword", &WebApp::changePassword)
        .function("suggestedDays", &WebApp::suggestedDays)
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
        .function("removeVehicle", &WebApp::removeVehicle)
        .function("fleet", &WebApp::fleet)
        .function("statistics", &WebApp::statistics)
        .function("runSelfTest", &WebApp::runSelfTest);
}
