// =============================================================================
// BusinessRuleSelfTest.cpp
// =============================================================================
#include "BusinessRuleSelfTest.h"

#include "BundleDataStore.h"
#include "DateTime.h"
#include "IDataStore.h"
#include "InMemoryDataStore.h"
#include "Money.h"
#include "RentalAgency.h"
#include "RentalExceptions.h"
#include "RentalStatistics.h"
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

    // ---- Fleet search and filter ----------------------------------------------------
    results.push_back(expectSuccess(L"Fleet search by plate/model with status filter", [&]() -> std::wstring {
        check(agency.searchFleet(L"tst", StatusFilter::All).size() == 2, "Plate search should find 2 vehicles.");
        check(agency.searchFleet(L"civic", StatusFilter::All).size() == 1, "Model search should find 1 vehicle.");
        check(agency.searchFleet(L"", StatusFilter::Available).size() == 2, "Both vehicles should be available.");
        check(agency.searchFleet(L"", StatusFilter::Rented).empty(), "No vehicle should be rented.");
        return L"\"tst\" finds 2, \"civic\" finds 1, filter Rented finds 0";
    }));

    // ---- Removing vehicles --------------------------------------------------------------
    results.push_back(expectThrow<VehicleNotAvailableException>(L"Reject removing a rented vehicle", [&] {
        agency.rentVehicle(L"TST-002", Renter(L"Bob Ray", L"0912345678"), Money::parse(L"100"));
        agency.removeVehicle(L"TST-002");
    }));

    // ---- Statistics -----------------------------------------------------------------------
    results.push_back(expectSuccess(L"Statistics: utilization, averages, revenue by model", [&]() -> std::wstring {
        const RentalStatistics stats(agency);
        check(stats.getVehicleCount() == 2 && stats.getRentedCount() == 1, "Wrong fleet counts.");
        check(stats.getUtilizationPercent() == 50, "Utilization should be 50%.");
        check(stats.getActiveDailyIncome() == Money::parse(L"100"), "Active daily income should be 100.00.");
        check(stats.getCompletedRentals() == 1, "Should count 1 completed rental.");
        check(stats.getAverageCharge() == Money::parse(L"421.50"), "Wrong average charge.");
        check(stats.getAverageDaysText() == L"3.0", "Wrong average rental length.");
        check(stats.getRevenueByModel().size() == 1 && stats.getTopVehicles().size() == 1, "Wrong grouping.");
        return L"50% rented, average 421.50 over " + stats.getAverageDaysText() + L" days";
    }));

    results.push_back(expectSuccess(L"Remove an available vehicle (history is kept)", [&]() -> std::wstring {
        agency.removeVehicle(L"tst-001");
        check(agency.searchFleet(L"TST-001", StatusFilter::All).empty(), "Vehicle was not removed.");
        check(agency.getHistory().size() == 1, "History must be kept after removal.");
        return L"TST-001 removed, its receipt is still in the history";
    }));

    // ---- Rental start dates ------------------------------------------------------
    results.push_back(expectSuccess(L"Rental days are calculated from the start date", []() -> std::wstring {
        const DateTime start = DateTime::fromParts(2026, 1, 1, 8, 0);
        check(start.rentalDaysUntil(DateTime::fromParts(2026, 1, 4, 8, 0)) == 3, "Exactly 72 hours must be 3 days.");
        check(start.rentalDaysUntil(DateTime::fromParts(2026, 1, 4, 9, 0)) == 4, "A started day must count.");
        check(start.rentalDaysUntil(start) == 1, "The minimum must be 1 day.");
        check(DateTime::fromParts(2024, 2, 29, 23, 59).plusMinutes(1).toString() == L"2024-03-01 00:00", "Leap year error.");
        return L"72 h = 3 days, 73 h = 4 days, minimum 1 day";
    }));
    results.push_back(expectThrow<ValidationException>(L"Reject an impossible date (2026-02-30)", [] {
        static_cast<void>(DateTime::fromParts(2026, 2, 30, 10, 0));
    }));
    results.push_back(expectSuccess(L"Renting records the start date", []() -> std::wstring {
        RentalAgency dates;
        dates.addVehicle(L"DAT-001", L"Test Car", Money::parse(L"100"));
        dates.rentVehicle(L"DAT-001", Renter(L"Ann Lee", L"0912345678"), Money::parse(L"100"));
        const std::optional<DateTime>& start = dates.findVehicle(L"DAT-001").getRental()->getStartDate();
        check(start.has_value(), "No start date was recorded.");
        check(dates.suggestedRentalDays(L"DAT-001") == 1, "A rental started now must suggest 1 day.");
        return L"started " + start->toString() + L", suggested 1 day";
    }));

    // ---- Users and roles ------------------------------------------------------------
    results.push_back(expectThrow<AuthenticationException>(L"Reject a wrong password", [] {
        RentalAgency users;
        users.login(L"manager", L"wrong-password");
    }));
    results.push_back(expectThrow<AccessDeniedException>(L"A Clerk cannot add vehicles", [] {
        RentalAgency users;
        users.login(L"clerk", L"clerk123");
        users.addVehicle(L"CLK-001", L"Test Car", Money::parse(L"100"));
    }));
    results.push_back(expectSuccess(L"A Clerk can rent and return; a Manager can do everything", []() -> std::wstring {
        RentalAgency users;
        users.addVehicle(L"CLK-002", L"Test Car", Money::parse(L"100")); // no login yet: allowed (setup)
        users.login(L"clerk", L"clerk123");
        users.rentVehicle(L"CLK-002", Renter(L"Ann Lee", L"0912345678"), Money::parse(L"100"));
        users.returnVehicle(L"CLK-002", 2);
        check(!users.isAllowed(Permission::ViewReports), "A Clerk must not see the reports.");
        users.login(L"manager", L"manager123");
        check(users.isAllowed(Permission::ManageUsers) && users.isAllowed(Permission::ManageFleet), "Manager rights missing.");
        users.addUser(L"alice", L"secret99", Role::Clerk);
        check(users.getUsers().size() == 3, "The new account was not added.");
        return L"Clerk rented/returned; Manager added user \"alice\"";
    }));
    results.push_back(expectThrow<ValidationException>(L"Cannot delete your own account (or the last Manager)", [] {
        RentalAgency users;
        users.login(L"manager", L"manager123");
        users.addUser(L"helper", L"secret99", Role::Manager);
        users.login(L"helper", L"secret99");
        users.removeUser(L"manager"); // allowed: another Manager remains
        users.removeUser(L"helper");  // own account: must be refused
    }));
    results.push_back(expectThrow<ValidationException>(L"Reject a password shorter than 6 characters", [] {
        RentalAgency users;
        users.addUser(L"bob", L"123", Role::Clerk);
    }));
    results.push_back(expectSuccess(L"Passwords are stored as salted hashes", []() -> std::wstring {
        RentalAgency users;
        for (const User& user : users.getUsers())
        {
            check(user.getPasswordHash().size() == 64 && user.getSalt().size() == 16, "A password is not hashed.");
            check(!user.checkPassword(user.getPasswordHash()), "The hash must not work as a password.");
        }
        check(User::hashPassword(L"0123456789abcdef", L"x") != User::hashPassword(L"fedcba9876543210", L"x"),
              "Different salts must give different hashes.");
        return L"SHA-256 x 1000 with a random 16-digit salt";
    }));

    // ---- Shared data bundle -------------------------------------------------------------
    results.push_back(expectSuccess(L"Data bundle round trip (shared online format)", [&]() -> std::wstring {
        BundleDataStore bundle;
        IDataStore& store = bundle;
        agency.saveTo(store);
        RentalAgency reloaded;
        const std::vector<std::wstring> warnings = reloaded.loadFrom(store);
        check(warnings.empty(), "Unexpected warnings.");
        check(reloaded.getVehicleCount() == agency.getVehicleCount() && reloaded.getRevenue() == agency.getRevenue() &&
              reloaded.getHistory().size() == agency.getHistory().size() && reloaded.getUsers().size() == 2,
              "Reloaded data differs.");
        reloaded.login(L"manager", L"manager123"); // the password hashes survived the round trip
        return L"vehicles, revenue, history and user accounts restored";
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
