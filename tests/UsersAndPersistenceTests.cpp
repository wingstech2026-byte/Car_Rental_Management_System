// =============================================================================
// UsersAndPersistenceTests.cpp - unit tests (GoogleTest) for user accounts,
// role-based access control and the data stores (text files, bundle, memory).
// =============================================================================
#include "BundleDataStore.h"
#include "DataFileFormat.h"
#include "InMemoryDataStore.h"
#include "Money.h"
#include "RentalAgency.h"
#include "RentalExceptions.h"
#include "RentalStatistics.h"
#include "Renter.h"
#include "TextFileDataStore.h"
#include "User.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <string>

namespace
{
    const Renter AnnLee(L"Ann Lee", L"0912345678");

    bool containsWarning(const std::vector<std::wstring>& warnings, const std::wstring& text)
    {
        for (const std::wstring& warning : warnings)
        {
            if (warning.find(text) != std::wstring::npos)
            {
                return true;
            }
        }
        return false;
    }
}

// ---- Users and roles ---------------------------------------------------------------------

TEST(User, PasswordHashMatchesTheOnlineDatabase)
{
    // The same value is computed by api/data.js (Node.js crypto), so a password
    // set in the desktop program also works for the shared online database.
    EXPECT_EQ(User::hashPassword(L"0123456789abcdef", L"manager123"),
              L"91da3dc2b70b3770a8f13ab5175fd8c189a75a1df26d5ac91e46cf88b490511a");
}

TEST(User, StoresOnlyASaltedHash)
{
    const User user = User::create(L"Alice", L"secret99", Role::Clerk);
    EXPECT_EQ(user.getUsername(), L"alice");
    EXPECT_EQ(user.getSalt().size(), 16u);
    EXPECT_EQ(user.getPasswordHash().size(), 64u);
    EXPECT_TRUE(user.checkPassword(L"secret99"));
    EXPECT_FALSE(user.checkPassword(L"secret98"));
    const User other = User::create(L"bob", L"secret99", Role::Clerk);
    EXPECT_NE(user.getPasswordHash(), other.getPasswordHash()) << "same password, different salt";
}

TEST(Roles, ClerksMayOnlyRentAndReturn)
{
    EXPECT_TRUE(User::roleAllows(Role::Clerk, Permission::RentAndReturn));
    EXPECT_FALSE(User::roleAllows(Role::Clerk, Permission::ManageFleet));
    EXPECT_FALSE(User::roleAllows(Role::Clerk, Permission::ViewReports));
    EXPECT_FALSE(User::roleAllows(Role::Clerk, Permission::ManageUsers));
    EXPECT_FALSE(User::roleAllows(Role::Clerk, Permission::ManageData));
    for (const Permission permission : { Permission::ManageFleet, Permission::RentAndReturn, Permission::ViewReports,
                                         Permission::ManageUsers, Permission::ManageData })
    {
        EXPECT_TRUE(User::roleAllows(Role::Manager, permission));
    }
}

TEST(RentalAgency, EnforcesRolesAfterLogin)
{
    RentalAgency agency;
    agency.addVehicle(L"ABC-123", L"Toyota Corolla", Money::parse(L"150"));
    EXPECT_THROW(agency.login(L"clerk", L"wrong"), AuthenticationException);
    EXPECT_THROW(agency.login(L"nobody", L"clerk123"), AuthenticationException);

    agency.login(L"clerk", L"clerk123");
    EXPECT_THROW(agency.addVehicle(L"NEW-1", L"Test", Money::parse(L"10")), AccessDeniedException);
    EXPECT_THROW(agency.removeVehicle(L"ABC-123"), AccessDeniedException);
    EXPECT_THROW(agency.addUser(L"eve", L"secret99", Role::Manager), AccessDeniedException);
    EXPECT_THROW(RentalStatistics stats(agency), AccessDeniedException);
    agency.rentVehicle(L"ABC-123", AnnLee, Money::parse(L"150"));   // allowed
    agency.returnVehicle(L"ABC-123", 2);                             // allowed

    agency.login(L"manager", L"manager123");
    agency.addVehicle(L"NEW-1", L"Test", Money::parse(L"10"));
    EXPECT_NO_THROW(RentalStatistics stats(agency));
}

TEST(RentalAgency, ProtectsTheAccountRules)
{
    RentalAgency agency;
    agency.login(L"manager", L"manager123");
    agency.addUser(L"alice", L"secret99", Role::Clerk);
    EXPECT_THROW(agency.addUser(L"ALICE", L"secret99", Role::Clerk), ValidationException); // duplicate name
    EXPECT_THROW(agency.addUser(L"bob", L"123", Role::Clerk), ValidationException);         // short password
    EXPECT_THROW(agency.removeUser(L"manager"), ValidationException);                        // own account
    agency.resetPassword(L"alice", L"newpass1");
    agency.login(L"alice", L"newpass1");
    EXPECT_THROW(agency.changeOwnPassword(L"wrong", L"another1"), ValidationException);
    agency.changeOwnPassword(L"newpass1", L"another1");
    EXPECT_NO_THROW(agency.login(L"alice", L"another1"));
}

TEST(RentalAgency, NeverDeletesTheLastManager)
{
    RentalAgency agency;
    agency.addUser(L"helper", L"secret99", Role::Manager);
    agency.login(L"helper", L"secret99");
    agency.removeUser(L"manager");                                    // another Manager remains
    agency.login(L"clerk", L"clerk123");
    EXPECT_THROW(agency.removeUser(L"helper"), AccessDeniedException); // a Clerk may not
    agency.logout();
    EXPECT_THROW(agency.removeUser(L"helper"), ValidationException);   // last Manager
}

// ---- Data stores ---------------------------------------------------------------------------

TEST(DataStores, BundleRoundTripKeepsEverything)
{
    RentalAgency agency;
    agency.addVehicle(L"ABC-123", L"Toyota Corolla", Money::parse(L"150"));
    agency.addVehicle(L"HON-111", L"Honda Civic", Money::parse(L"140"));
    agency.rentVehicle(L"ABC-123", AnnLee, Money::parse(L"135"));
    agency.rentVehicle(L"HON-111", AnnLee, Money::parse(L"140"));
    agency.returnVehicle(L"HON-111", 2);
    agency.addUser(L"alice", L"secret99", Role::Clerk);

    BundleDataStore bundle;
    IDataStore& store = bundle; // polymorphism: the agency only sees the interface
    agency.saveTo(store);
    EXPECT_EQ(bundle.getBundle().rfind(L"CRMS-BUNDLE 1", 0), 0u);

    RentalAgency copy;
    EXPECT_TRUE(copy.loadFrom(store).empty());
    EXPECT_EQ(copy.getVehicleCount(), 2u);
    EXPECT_EQ(copy.getRevenue(), Money::parse(L"280"));
    EXPECT_EQ(copy.getHistory().size(), 1u);
    EXPECT_TRUE(copy.findVehicle(L"ABC-123").getRental()->getStartDate().has_value());
    EXPECT_NO_THROW(copy.login(L"alice", L"secret99"));
}

TEST(DataStores, TextFilesRoundTrip)
{
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path folder = std::filesystem::temp_directory_path() / ("crms_test_" + std::to_string(stamp));

    RentalAgency agency;
    agency.addVehicle(L"ABC-123", L"Toyota Corolla", Money::parse(L"150"));
    agency.rentVehicle(L"ABC-123", AnnLee, Money::parse(L"135"));
    {
        TextFileDataStore files(folder);
        agency.saveTo(files);
    }
    TextFileDataStore files(folder);
    RentalAgency copy;
    EXPECT_TRUE(copy.loadFrom(files).empty());
    EXPECT_TRUE(copy.findVehicle(L"ABC-123").isRented());
    EXPECT_EQ(copy.findVehicle(L"ABC-123").getRental()->getAgreedDailyRate(), Money::parse(L"135"));

    std::error_code ignored;
    std::filesystem::remove_all(folder, ignored);
}

TEST(DataStores, MissingFilesMeanEmptyDataAndDefaultAccounts)
{
    TextFileDataStore files(std::filesystem::temp_directory_path() / "crms_does_not_exist_12345");
    RentalAgency agency;
    EXPECT_TRUE(agency.loadFrom(files).empty());
    EXPECT_EQ(agency.getVehicleCount(), 0u);
    EXPECT_EQ(agency.getRevenue(), Money());
    EXPECT_NO_THROW(agency.login(L"manager", L"manager123"));
    EXPECT_TRUE(agency.usesDefaultPasswords());
}

TEST(DataFileFormat, SkipsInvalidLinesWithWarnings)
{
    LoadResult result;
    DataFileFormat::parseVehicles(
        L"ABC-123|Toyota Corolla|150.00|Available|||\n"            // valid (older 7-field format)
        L"abc-123|Toyota Corolla|160.00|Available||||\n"           // duplicate plate
        L"BAD PLATE|Honda Civic|100.00|Available||||\n"            // invalid plate
        L"NEW-002|Kia Rio|99.999|Available||||\n"                  // three decimals
        L"NEW-003|Kia Rio|80.00|Rented||||\n"                      // rented without renter
        L"NEW-004|Kia Rio|80.00\n"                                 // too few fields
        L"NEW-005|Kia Rio|80.00|Rented|Ann Lee|0912345678|80.00|2026-02-30 10:00\n" // impossible date
        L"NEW-009|Hyundai i20|95.00|Rented|Ann Lee|0912345678|90.00|2026-11-01 09:00\n", // valid
        result);
    EXPECT_EQ(result.vehicles.size(), 2u);
    EXPECT_EQ(result.warnings.size(), 6u);
    EXPECT_TRUE(containsWarning(result.warnings, L"duplicate license plate ABC-123"));
    EXPECT_TRUE(containsWarning(result.warnings, L"expected 7 or 8 fields"));
}

TEST(DataFileFormat, RejectsHistoryWithAWrongCharge)
{
    LoadResult result;
    DataFileFormat::parseHistory(
        L"1|2026-10-03 09:30|ABC-123|Toyota Corolla|Emma Wilson|+1 555-0142|150.00|4|600.00\n"
        L"2|2026-10-11 17:05|HON-111|Honda Civic|Liam Chen|0917-222-333|140.00|5|999.00\n"
        L"1|2026-10-12 17:05|HON-111|Honda Civic|Liam Chen|0917-222-333|140.00|5|700.00|2026-10-07 10:00\n",
        result);
    EXPECT_EQ(result.history.size(), 1u);
    EXPECT_TRUE(containsWarning(result.warnings, L"does not equal agreed rate x days"));
    EXPECT_TRUE(containsWarning(result.warnings, L"duplicate receipt number 1"));
}

TEST(DataFileFormat, RejectsSomethingThatIsNotABundle)
{
    EXPECT_THROW(DataFileFormat::parseBundle(L"hello"), DataStoreException);
    EXPECT_TRUE(DataFileFormat::parseBundle(L"").vehicles.empty());
}

TEST(DataStores, InMemoryStoreWorksThroughTheInterface)
{
    RentalAgency agency;
    agency.addVehicle(L"ABC-123", L"Toyota Corolla", Money::parse(L"150"));
    InMemoryDataStore memory;
    IDataStore& store = memory;
    agency.saveTo(store);
    RentalAgency copy;
    copy.loadFrom(store);
    EXPECT_EQ(copy.getVehicleCount(), 1u);
}
