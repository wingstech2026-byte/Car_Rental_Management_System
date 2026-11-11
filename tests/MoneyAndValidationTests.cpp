// =============================================================================
// MoneyAndValidationTests.cpp - unit tests (GoogleTest) for Money, Validation,
// DateTime and Sha256: the small value classes and rules everything else uses.
// =============================================================================
#include "DateTime.h"
#include "Money.h"
#include "RentalExceptions.h"
#include "Sha256.h"
#include "TextUtils.h"
#include "Validation.h"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <string>

// ---- Money ------------------------------------------------------------------------

TEST(Money, ParsesAndFormatsWithTwoDecimals)
{
    EXPECT_EQ(Money::parse(L"150").cents(), 15000);
    EXPECT_EQ(Money::parse(L"150.5").cents(), 15050);
    EXPECT_EQ(Money::parse(L" 149.99 ").cents(), 14999);
    EXPECT_EQ(Money::parse(L"150.5").toString(), L"150.50");
    EXPECT_EQ(Money::fromCents(123456789).toDisplayString(), L"1,234,567.89");
    EXPECT_EQ(Money().toString(), L"0.00");
}

TEST(Money, RejectsInvalidText)
{
    for (const wchar_t* text : { L"", L"-5", L"12.345", L"abc", L"1,250.00", L"150.", L".5", L"1e5" })
    {
        EXPECT_FALSE(Money::tryParse(text).has_value()) << "accepted: " << TextUtils::toUtf8(text);
        EXPECT_THROW(Money::parse(text), ValidationException);
    }
}

TEST(Money, ArithmeticIsExact)
{
    EXPECT_EQ(Money::parse(L"0.10") + Money::parse(L"0.20"), Money::parse(L"0.30"));
    EXPECT_EQ(Money::parse(L"145.50") * 3, Money::parse(L"436.50"));
    Money total;
    total += Money::parse(L"600");
    total += Money::parse(L"1050");
    EXPECT_EQ(total.toDisplayString(), L"1,650.00");
    EXPECT_LT(Money::parse(L"1"), Money::parse(L"1.01"));
}

TEST(Money, DetectsOverflowAndNegativeMultipliers)
{
    const Money huge = Money::fromCents(std::numeric_limits<long long>::max());
    EXPECT_THROW(huge + Money::fromCents(1), std::overflow_error);
    EXPECT_THROW(huge * 2, std::overflow_error);
    EXPECT_THROW(Money::fromCents(100) * -1, std::invalid_argument);
}

// ---- Validation ------------------------------------------------------------------------

TEST(Validation, NormalizesPlates)
{
    EXPECT_EQ(Validation::normalizePlate(L"  abc-123 "), L"ABC-123");
    EXPECT_THROW(Validation::normalizePlate(L""), ValidationException);
    EXPECT_THROW(Validation::normalizePlate(L"AB 123"), ValidationException);
    EXPECT_THROW(Validation::normalizePlate(L"AB_123"), ValidationException);
    EXPECT_THROW(Validation::normalizePlate(L"---"), ValidationException);
    EXPECT_THROW(Validation::normalizePlate(L"AB|12"), ValidationException);
    EXPECT_THROW(Validation::normalizePlate(L"ABCDEFGHIJKLMNOP"), ValidationException); // 16 characters
}

TEST(Validation, ChecksTelephoneNumbers)
{
    EXPECT_EQ(Validation::validatePhone(L" +44 20 7946 0958 "), L"+44 20 7946 0958");
    EXPECT_EQ(Validation::validatePhone(L"0912-345-678"), L"0912-345-678");
    for (const wchar_t* phone : { L"", L"12345", L"0912ab5678", L"12+3456789", L"------", L"123456789012345678901" })
    {
        EXPECT_THROW(Validation::validatePhone(phone), ValidationException);
    }
}

TEST(Validation, ParsesRentalDays)
{
    EXPECT_EQ(Validation::parseRentalDays(L"3"), 3);
    EXPECT_EQ(Validation::parseRentalDays(L" 3650 "), 3650);
    for (const wchar_t* days : { L"", L"0", L"-2", L"2.5", L"abc", L"3651", L"99999", L"9999999999" })
    {
        EXPECT_THROW(Validation::parseRentalDays(days), ValidationException);
    }
}

TEST(Validation, ChecksRates)
{
    EXPECT_EQ(Validation::parseRate(L"1000000", InputField::DailyRate), Money::parse(L"1000000"));
    EXPECT_THROW(Validation::parseRate(L"0", InputField::DailyRate), ValidationException);
    EXPECT_THROW(Validation::parseRate(L"1000000.01", InputField::DailyRate), ValidationException);
    try
    {
        Validation::parseRate(L"12.345", InputField::AgreedRate);
        FAIL() << "no exception";
    }
    catch (const ValidationException& error)
    {
        EXPECT_EQ(error.field(), InputField::AgreedRate); // the GUI uses this to focus the right field
    }
}

TEST(Validation, ChecksUserNamesAndPasswords)
{
    EXPECT_EQ(Validation::normalizeUsername(L"  Alice.B "), L"alice.b");
    EXPECT_THROW(Validation::normalizeUsername(L"ab"), ValidationException);
    EXPECT_THROW(Validation::normalizeUsername(L"has space"), ValidationException);
    EXPECT_NO_THROW(Validation::validatePassword(L"secret"));
    EXPECT_THROW(Validation::validatePassword(L"12345"), ValidationException);
    EXPECT_THROW(Validation::validatePassword(L"pass|word"), ValidationException);
}

// ---- DateTime ------------------------------------------------------------------------

TEST(DateTime, ParsesAndFormats)
{
    const auto parsed = DateTime::tryParse(L"2026-11-11 09:05");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->toString(), L"2026-11-11 09:05");
    EXPECT_EQ(DateTime().toString(), L"1970-01-01 00:00");
    EXPECT_EQ(DateTime::fromParts(1969, 12, 31, 23, 59).toString(), L"1969-12-31 23:59");
}

TEST(DateTime, RejectsImpossibleDates)
{
    for (const wchar_t* text : { L"2026-02-29 10:00", L"2026-13-01 10:00", L"2026-04-31 10:00",
                                 L"2026-01-01 24:00", L"2026-01-01 10:60", L"2026-1-1 10:00", L"tomorrow" })
    {
        EXPECT_FALSE(DateTime::tryParse(text).has_value());
    }
    EXPECT_TRUE(DateTime::tryParse(L"2024-02-29 10:00").has_value()); // leap year
    EXPECT_THROW(DateTime::fromParts(2026, 2, 30, 10, 0), ValidationException);
}

TEST(DateTime, CountsStartedRentalDays)
{
    const DateTime start = DateTime::fromParts(2026, 1, 1, 8, 0);
    EXPECT_EQ(start.rentalDaysUntil(start), 1);
    EXPECT_EQ(start.rentalDaysUntil(start.plusMinutes(1)), 1);
    EXPECT_EQ(start.rentalDaysUntil(DateTime::fromParts(2026, 1, 2, 8, 0)), 1);
    EXPECT_EQ(start.rentalDaysUntil(DateTime::fromParts(2026, 1, 2, 8, 1)), 2);
    EXPECT_EQ(start.rentalDaysUntil(DateTime::fromParts(2026, 3, 1, 8, 0)), 59); // 31 + 28 days
    EXPECT_EQ(DateTime::fromParts(2024, 2, 28, 12, 0).rentalDaysUntil(DateTime::fromParts(2024, 3, 1, 12, 0)), 2);
}

// ---- Sha256 ------------------------------------------------------------------------------

TEST(Sha256, MatchesTheStandardTestVectors)
{
    EXPECT_EQ(Sha256::hashHex(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(Sha256::hashHex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_EQ(Sha256::hashHex(std::string(1000, 'a')).size(), 64u);
}
