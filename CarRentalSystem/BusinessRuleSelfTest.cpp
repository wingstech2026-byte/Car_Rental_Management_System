// =============================================================================
// BusinessRuleSelfTest.cpp
// =============================================================================
#include "BusinessRuleSelfTest.h"

#include "IDataStore.h"
#include "InMemoryDataStore.h"
#include "Money.h"
#include "RentalAgency.h"
#include "RentalExceptions.h"
#include "Renter.h"
#include "TextUtils.h"
#include "Validation.h"

#include <initializer_list>
#include <stdexcept>

namespace
{
    /// Runs the action and expects it to throw ExpectedException.
    template <typename ExpectedException, typename Action>
    SelfTestResult expectThrow(const std::wstring& name, Action action)
    {
        try
        {
            action();
        }
        catch (const ExpectedException& error)
        {
            return SelfTestResult{ name, true, L"Rejected: " + error.message() };
        }
        catch (const RentalException& error)
        {
            return SelfTestResult{ name, false, L"Wrong exception type: " + error.message() };
        }
        catch (const std::exception& error)
        {
            return SelfTestResult{ name, false, L"Unexpected error: " + TextUtils::fromUtf8Lenient(error.what()) };
        }
        return SelfTestResult{ name, false, L"No exception was thrown - the rule is NOT enforced." };
    }

    /// Runs the action and expects it to succeed. The action returns a short
    /// description of the result, or throws std::logic_error if a check fails.
    template <typename Action>
    SelfTestResult expectSuccess(const std::wstring& name, Action action)
    {
        std::wstring detail;
        try
        {
            detail = action();
        }
        catch (const RentalException& error)
        {
            return SelfTestResult{ name, false, error.message() };
        }
        catch (const std::exception& error)
        {
            return SelfTestResult{ name, false, TextUtils::fromUtf8Lenient(error.what()) };
        }
        return SelfTestResult{ name, true, detail };
    }

    void check(bool condition, const char* failureMessage)
    {
        if (!condition)
        {
            throw std::logic_error(failureMessage);
        }
    }
}

std::vector<SelfTestResult> BusinessRuleSelfTest::run() const
{
    std::vector<SelfTestResult> results;
    RentalAgency agency; // temporary agency - the real data is never touched
    const std::wstring plate = L"TST-001";

    // ---- Adding vehicles --------------------------------------------------------
    results.push_back(expectSuccess(L"Add a valid vehicle (plate stored in upper case)", [&]() -> std::wstring {
        agency.addVehicle(L"tst-001", L"Toyota Corolla", Money::parse(L"150"));
        agency.addVehicle(L"TST-002", L"Honda Civic", Money::parse(L"120.50"));
        check(agency.findVehicle(plate).getPlate() == plate, "Plate was not converted to upper case.");
        return L"Stored as " + plate;
    }));
    results.push_back(expectThrow<DuplicatePlateException>(L"Reject duplicate plate (different case)", [&] {
        agency.addVehicle(L"Tst-001", L"Ford Focus", Money::parse(L"100"));
    }));
    results.push_back(expectThrow<ValidationException>(L"Reject empty plate", [&] {
        agency.addVehicle(L"   ", L"Ford Focus", Money::parse(L"100"));
    }));
    results.push_back(expectThrow<ValidationException>(L"Reject plate with invalid characters", [&] {
        agency.addVehicle(L"AB 12_X", L"Ford Focus", Money::parse(L"100"));
    }));
    results.push_back(expectThrow<ValidationException>(L"Reject empty model", [&] {
        agency.addVehicle(L"TST-003", L"", Money::parse(L"100"));
    }));
    results.push_back(expectThrow<ValidationException>(L"Reject '|' in model", [&] {
        agency.addVehicle(L"TST-003", L"Ford|Focus", Money::parse(L"100"));
    }));
    results.push_back(expectThrow<ValidationException>(L"Reject daily rate 0.00", [&] {
        agency.addVehicle(L"TST-003", L"Ford Focus", Money());
    }));
    for (const wchar_t* input : { L"-5", L"12.345", L"abc", L"", L"1,250.00" })
    {
        results.push_back(expectThrow<ValidationException>(
            std::wstring(L"Reject daily rate \"") + input + L"\"",
            [input] { Validation::parseRate(input, InputField::DailyRate); }));
    }

    // ---- Editing and searching ------------------------------------------------
    results.push_back(expectSuccess(L"Edit an available vehicle", [&]() -> std::wstring {
        agency.editVehicle(plate, L"Toyota Corolla Hybrid", Money::parse(L"160.00"));
        check(agency.findVehicle(plate).getDailyRate() == Money::fromCents(16000), "Rate was not changed.");
        return L"New rate " + agency.findVehicle(plate).getDailyRate().toDisplayString();
    }));
    results.push_back(expectSuccess(L"Search \"COR\" finds \"Toyota Corolla Hybrid\"", [&]() -> std::wstring {
        check(agency.searchAvailableByModel(L"COR").size() == 1, "Expected exactly one match.");
        check(agency.searchAvailableByModel(L"").size() == 2, "Empty search must return all available vehicles.");
        return L"1 match; empty search returns 2 vehicles";
    }));
    results.push_back(expectThrow<VehicleNotRentedException>(L"Reject returning an available vehicle", [&] {
        agency.returnVehicle(plate, 1);
    }));

    // ---- Renter validation ------------------------------------------------------
    results.push_back(expectThrow<ValidationException>(L"Reject empty renter name", [] {
        static_cast<void>(Renter(L"  ", L"0912345678"));
    }));
    results.push_back(expectThrow<ValidationException>(L"Reject telephone with letters", [] {
        static_cast<void>(Renter(L"Ann Lee", L"0912ab5678"));
    }));
    results.push_back(expectThrow<ValidationException>(L"Reject telephone shorter than 6 characters", [] {
        static_cast<void>(Renter(L"Ann Lee", L"12345"));
    }));
    results.push_back(expectThrow<ValidationException>(L"Reject '|' in renter name", [] {
        static_cast<void>(Renter(L"Ann|Lee", L"0912345678"));
    }));

    // ---- Renting ----------------------------------------------------------------
    results.push_back(expectSuccess(L"Rent an available vehicle at an agreed rate", [&]() -> std::wstring {
        agency.rentVehicle(plate, Renter(L"Ann Lee", L"+1 555-0100"), Money::parse(L"140.50"));
        check(agency.findVehicle(plate).isRented(), "Vehicle is not marked as rented.");
        check(agency.searchAvailableByModel(L"cor").empty(), "Rented vehicle still appears in search.");
        return L"Rented at 140.50 (current rate 160.00)";
    }));
    results.push_back(expectThrow<VehicleNotAvailableException>(L"Reject renting a rented vehicle again", [&] {
        agency.rentVehicle(plate, Renter(L"Bob Ray", L"0912345678"), Money::parse(L"100"));
    }));
    results.push_back(expectThrow<VehicleNotAvailableException>(L"Reject editing a rented vehicle", [&] {
        agency.editVehicle(plate, L"Toyota Yaris", Money::parse(L"99"));
    }));

    // ---- Returning --------------------------------------------------------------
    for (const wchar_t* input : { L"0", L"-2", L"2.5", L"abc", L"", L"99999" })
    {
        results.push_back(expectThrow<ValidationException>(
            std::wstring(L"Reject rental days \"") + input + L"\"",
            [input] { Validation::parseRentalDays(input); }));
    }
    results.push_back(expectThrow<ValidationException>(L"Reject returning with 0 days (vehicle stays rented)", [&] {
        agency.returnVehicle(plate, 0);
    }));
    results.push_back(expectSuccess(L"Return after 3 days: 140.50 x 3 = 421.50", [&]() -> std::wstring {
        const Receipt receipt = agency.returnVehicle(plate, 3);
        check(receipt.getTotalCharge() == Money::parse(L"421.50"), "Wrong charge.");
        check(agency.getRevenue() == Money::parse(L"421.50"), "Revenue was not updated.");
        check(agency.findVehicle(plate).isAvailable(), "Vehicle is not available after return.");
        check(!agency.findVehicle(plate).getRental().has_value(), "Rental details were not cleared.");
        return L"Charge " + receipt.getTotalCharge().toDisplayString() + L", revenue " +
               agency.getRevenue().toDisplayString();
    }));
    results.push_back(expectThrow<VehicleNotFoundException>(L"Reject unknown plate", [&] {
        agency.findVehicle(L"NOPE-999");
    }));

    // ---- Rental history -----------------------------------------------------------
    results.push_back(expectSuccess(L"Return is archived in the rental history", [&]() -> std::wstring {
        check(agency.getHistory().size() == 1, "History should contain exactly one record.");
        const RentalRecord& record = agency.getLastRecord();
        check(record.getNumber() == 1, "First receipt number should be 1.");
        check(record.getReceipt().getPlate() == plate, "Wrong plate in history.");
        check(record.getReceipt().getTotalCharge() == Money::parse(L"421.50"), "Wrong charge in history.");
        check(RentalRecord::isValidTimestamp(record.getReturnedOn()), "Invalid return date.");
        check(RentalAgency::totalCharged(agency.getHistory()) == agency.getRevenue(), "History total differs from revenue.");
        return L"Receipt #1 on " + record.getReturnedOn();
    }));
    results.push_back(expectSuccess(L"Search history by renter, plate and model", [&]() -> std::wstring {
        check(agency.searchHistory(L"ann").size() == 1, "Renter search failed.");
        check(agency.searchHistory(L"tst-001").size() == 1, "Plate search failed.");
        check(agency.searchHistory(L"hybrid").size() == 1, "Model search failed.");
        check(agency.searchHistory(L"nobody").empty(), "Search should find nothing.");
        return L"\"ann\" finds 1 record, \"nobody\" finds 0";
    }));
    results.push_back(expectThrow<ValidationException>(L"Reject history record with an invalid date", [] {
        static_cast<void>(RentalRecord(1, L"2026-13-45 99:99",
            Receipt(L"TST-001", L"Toyota", L"Ann Lee", L"0912345678", Money::fromCents(1000), 1, Money::fromCents(1000))));
    }));

    // ---- Money ------------------------------------------------------------------
    results.push_back(expectSuccess(L"Money is exact: 0.10 + 0.20 == 0.30", []() -> std::wstring {
        check(Money::parse(L"0.10") + Money::parse(L"0.20") == Money::parse(L"0.30"), "0.10 + 0.20 != 0.30");
        check(Money::fromCents(123456789).toDisplayString() == L"1,234,567.89", "Wrong display format.");
        check(Money::parse(L"150.5").toString() == L"150.50", "Wrong plain format.");
        return L"1,234,567.89 / 150.50";
    }));

    // ---- Polymorphism: save/load through the IDataStore interface ---------------
    results.push_back(expectSuccess(L"Save and reload through IDataStore& (in-memory store)", [&]() -> std::wstring {
        InMemoryDataStore memoryStore;
        IDataStore& store = memoryStore; // the agency only sees the interface
        agency.saveTo(store);
        RentalAgency reloaded;
        const std::vector<std::wstring> warnings = reloaded.loadFrom(store);
        check(warnings.empty(), "Unexpected warnings.");
        check(reloaded.getVehicleCount() == agency.getVehicleCount(), "Vehicle count differs.");
        check(reloaded.getRevenue() == agency.getRevenue(), "Revenue differs.");
        check(reloaded.getHistory().size() == agency.getHistory().size(), "History differs.");
        return L"Reloaded " + std::to_wstring(reloaded.getVehicleCount()) + L" vehicles and " +
               std::to_wstring(reloaded.getHistory().size()) + L" history record(s)";
    }));

    return results;
}
