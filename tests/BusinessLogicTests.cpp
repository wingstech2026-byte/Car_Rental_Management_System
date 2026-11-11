// =============================================================================
// BusinessLogicTests.cpp - unit tests (GoogleTest) for Vehicle, Rental,
// RentalAgency, RentalStatistics and the rental history.
// =============================================================================
#include "DateTime.h"
#include "Money.h"
#include "Receipt.h"
#include "Rental.h"
#include "RentalAgency.h"
#include "RentalExceptions.h"
#include "RentalStatistics.h"
#include "Renter.h"
#include "Validation.h"
#include "Vehicle.h"

#include <gtest/gtest.h>

namespace
{
    const Renter AnnLee(L"Ann Lee", L"+1 555-0100");

    /// An agency with two available vehicles.
    RentalAgency makeAgency()
    {
        RentalAgency agency;
        agency.addVehicle(L"abc-123", L"Toyota Corolla", Money::parse(L"150"));
        agency.addVehicle(L"HON-111", L"Honda Civic", Money::parse(L"140"));
        return agency;
    }
}

// ---- Vehicle ----------------------------------------------------------------------------

TEST(Vehicle, CannotBeRentedTwiceOrEditedWhileRented)
{
    Vehicle vehicle(L"ABC-123", L"Toyota Corolla", Money::parse(L"150"));
    vehicle.rent(AnnLee, Money::parse(L"140"));
    EXPECT_TRUE(vehicle.isRented());
    EXPECT_THROW(vehicle.rent(AnnLee, Money::parse(L"140")), VehicleNotAvailableException);
    EXPECT_THROW(vehicle.edit(L"Toyota Yaris", Money::parse(L"99")), VehicleNotAvailableException);
}

TEST(Vehicle, OnlyRentedVehiclesCanBeReturned)
{
    Vehicle vehicle(L"ABC-123", L"Toyota Corolla", Money::parse(L"150"));
    EXPECT_THROW(vehicle.returnVehicle(1), VehicleNotRentedException);
}

TEST(Vehicle, ReturnUsesTheAgreedRateAndClearsTheRental)
{
    Vehicle vehicle(L"ABC-123", L"Toyota Corolla", Money::parse(L"150"));
    vehicle.rent(AnnLee, Money::parse(L"135"), DateTime::fromParts(2026, 11, 1, 9, 0));
    EXPECT_THROW(vehicle.returnVehicle(0), ValidationException);
    EXPECT_TRUE(vehicle.isRented()) << "an invalid return must not change the vehicle";

    const Receipt receipt = vehicle.returnVehicle(4);
    EXPECT_EQ(receipt.getTotalCharge(), Money::parse(L"540"));
    EXPECT_EQ(receipt.getStartedOn(), L"2026-11-01 09:00");
    EXPECT_TRUE(vehicle.isAvailable());
    EXPECT_FALSE(vehicle.getRental().has_value());
}

TEST(Vehicle, EditKeepsOldValuesWhenInputIsInvalid)
{
    Vehicle vehicle(L"ABC-123", L"Toyota Corolla", Money::parse(L"150"));
    EXPECT_THROW(vehicle.edit(L"", Money::parse(L"99")), ValidationException);
    EXPECT_EQ(vehicle.getModel(), L"Toyota Corolla");
    EXPECT_EQ(vehicle.getDailyRate(), Money::parse(L"150"));
}

TEST(Rental, CalculatesDaysFromTheStartDate)
{
    const Rental rental(AnnLee, Money::parse(L"100"), DateTime::fromParts(2026, 1, 1, 8, 0));
    EXPECT_EQ(rental.rentalDaysUntil(DateTime::fromParts(2026, 1, 4, 9, 0)), 4);
    const Rental unknownStart(AnnLee, Money::parse(L"100"));
    EXPECT_EQ(unknownStart.rentalDaysUntil(DateTime::fromParts(2030, 1, 1, 0, 0)), 1);
    const Rental veryOld(AnnLee, Money::parse(L"100"), DateTime::fromParts(1990, 1, 1, 0, 0));
    EXPECT_EQ(veryOld.rentalDaysUntil(DateTime::fromParts(2030, 1, 1, 0, 0)), Validation::MaxRentalDays);
}

// ---- RentalAgency --------------------------------------------------------------------------

TEST(RentalAgency, RejectsDuplicatePlatesIgnoringCase)
{
    RentalAgency agency = makeAgency();
    EXPECT_THROW(agency.addVehicle(L"ABC-123", L"Other", Money::parse(L"1")), DuplicatePlateException);
    EXPECT_THROW(agency.addVehicle(L"hon-111", L"Other", Money::parse(L"1")), DuplicatePlateException);
    EXPECT_EQ(agency.getVehicleCount(), 2u);
}

TEST(RentalAgency, SearchesAvailableVehiclesByModel)
{
    RentalAgency agency = makeAgency();
    EXPECT_EQ(agency.searchAvailableByModel(L"cor").size(), 1u);
    EXPECT_EQ(agency.searchAvailableByModel(L"").size(), 2u);
    agency.rentVehicle(L"ABC-123", AnnLee, Money::parse(L"150"));
    EXPECT_TRUE(agency.searchAvailableByModel(L"cor").empty()) << "rented vehicles must not be offered";
    EXPECT_EQ(agency.searchFleet(L"cor", StatusFilter::Rented).size(), 1u);
}

TEST(RentalAgency, ReturnAddsRevenueAndHistory)
{
    RentalAgency agency = makeAgency();
    agency.rentVehicle(L"ABC-123", AnnLee, Money::parse(L"140.50"));
    EXPECT_EQ(agency.suggestedRentalDays(L"ABC-123"), 1);
    const Receipt receipt = agency.returnVehicle(L"abc-123", 3);
    EXPECT_EQ(receipt.getTotalCharge(), Money::parse(L"421.50"));
    EXPECT_EQ(agency.getRevenue(), Money::parse(L"421.50"));
    ASSERT_EQ(agency.getHistory().size(), 1u);
    EXPECT_EQ(agency.getLastRecord().getNumber(), 1);
    EXPECT_FALSE(agency.getLastRecord().getReceipt().getStartedOn().empty());
    EXPECT_EQ(agency.searchHistory(L"ann").size(), 1u);
    EXPECT_THROW(agency.returnVehicle(L"ABC-123", 1), VehicleNotRentedException);
}

TEST(RentalAgency, RefusesToRemoveRentedVehicles)
{
    RentalAgency agency = makeAgency();
    agency.rentVehicle(L"ABC-123", AnnLee, Money::parse(L"150"));
    EXPECT_THROW(agency.removeVehicle(L"ABC-123"), VehicleNotAvailableException);
    agency.removeVehicle(L"HON-111");
    EXPECT_EQ(agency.getVehicleCount(), 1u);
    EXPECT_THROW(agency.findVehicle(L"HON-111"), VehicleNotFoundException);
}

TEST(RentalAgency, TracksUnsavedChanges)
{
    RentalAgency agency;
    EXPECT_FALSE(agency.hasUnsavedChanges());
    agency.addVehicle(L"NEW-1", L"Test", Money::parse(L"10"));
    EXPECT_TRUE(agency.hasUnsavedChanges());
}

// ---- RentalStatistics -----------------------------------------------------------------------

TEST(RentalStatistics, CalculatesUtilizationAndAverages)
{
    RentalAgency agency = makeAgency();
    agency.rentVehicle(L"ABC-123", AnnLee, Money::parse(L"100"));
    agency.returnVehicle(L"ABC-123", 4);   // 400.00
    agency.rentVehicle(L"HON-111", AnnLee, Money::parse(L"150"));
    agency.returnVehicle(L"HON-111", 2);   // 300.00
    agency.rentVehicle(L"ABC-123", AnnLee, Money::parse(L"120"));

    const RentalStatistics stats(agency);
    EXPECT_EQ(stats.getUtilizationPercent(), 50);
    EXPECT_EQ(stats.getActiveDailyIncome(), Money::parse(L"120"));
    EXPECT_EQ(stats.getCompletedRentals(), 2u);
    EXPECT_EQ(stats.getTotalCharged(), Money::parse(L"700"));
    EXPECT_EQ(stats.getAverageCharge(), Money::parse(L"350"));
    EXPECT_EQ(stats.getAverageDaysText(), L"3.0");
    ASSERT_EQ(stats.getRevenueByModel().size(), 2u);
    EXPECT_EQ(stats.getRevenueByModel().front().model, L"Toyota Corolla"); // highest revenue first
}
