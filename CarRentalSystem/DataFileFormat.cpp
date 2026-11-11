// =============================================================================
// DataFileFormat.cpp
// =============================================================================
#include "DataFileFormat.h"

#include "DateTime.h"
#include "RentalExceptions.h"
#include "TextUtils.h"
#include "Validation.h"

#include <optional>
#include <set>
#include <sstream>
#include <utility>

namespace
{
    constexpr wchar_t Separator = Validation::FieldSeparator;
    constexpr const wchar_t* NewLine = L"\r\n";

    std::wstring skippedLabel(const wchar_t* file, std::size_t index)
    {
        return std::wstring(file) + L" line " + std::to_wstring(index + 1) + L" skipped: ";
    }

    std::wstring fieldCountMessage(const wchar_t* expected, std::size_t found)
    {
        return std::wstring(L"expected ") + expected + L" fields separated by '|' but found " +
               std::to_wstring(found) + L".";
    }
}

// =============================================================================
// Reading
// =============================================================================

std::vector<std::wstring> DataFileFormat::splitLines(const std::wstring& text)
{
    std::vector<std::wstring> lines;
    std::wstring current;
    for (const wchar_t character : text)
    {
        if (character == L'\n')
        {
            if (!current.empty() && current.back() == L'\r')
            {
                current.pop_back();
            }
            lines.push_back(current);
            current.clear();
        }
        else
        {
            current += character;
        }
    }
    if (!current.empty())
    {
        if (current.back() == L'\r')
        {
            current.pop_back();
        }
        lines.push_back(current);
    }
    return lines;
}

void DataFileFormat::parseVehicles(const std::wstring& text, LoadResult& result)
{
    const std::vector<std::wstring> lines = splitLines(text);
    std::set<std::wstring> seenPlates;
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        if (TextUtils::trim(lines[index]).empty())
        {
            continue; // blank lines are ignored
        }
        try
        {
            Vehicle vehicle = parseVehicleLine(lines[index]);
            if (!seenPlates.insert(vehicle.getPlate()).second)
            {
                result.warnings.push_back(skippedLabel(VehiclesFile, index) + L"duplicate license plate " +
                                          vehicle.getPlate() + L".");
                continue;
            }
            result.vehicles.push_back(std::move(vehicle));
        }
        catch (const RentalException& problem)
        {
            result.warnings.push_back(skippedLabel(VehiclesFile, index) + problem.message());
        }
    }
}

void DataFileFormat::parseRevenue(const std::wstring& text, LoadResult& result)
{
    std::wstring value;
    for (const std::wstring& line : splitLines(text))
    {
        if (!TextUtils::trim(line).empty())
        {
            value = TextUtils::trim(line);
            break;
        }
    }
    if (value.empty())
    {
        return; // empty = zero revenue
    }
    const std::optional<Money> amount = Money::tryParse(value);
    if (!amount)
    {
        result.warnings.push_back(std::wstring(RevenueFile) + L" skipped: \"" + value +
                                  L"\" is not a valid amount - cumulative revenue starts at 0.00.");
        return;
    }
    result.revenue = *amount;
}

void DataFileFormat::parseHistory(const std::wstring& text, LoadResult& result)
{
    const std::vector<std::wstring> lines = splitLines(text);
    std::set<int> seenNumbers;
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        if (TextUtils::trim(lines[index]).empty())
        {
            continue;
        }
        try
        {
            RentalRecord record = parseHistoryLine(lines[index]);
            if (!seenNumbers.insert(record.getNumber()).second)
            {
                result.warnings.push_back(skippedLabel(HistoryFile, index) + L"duplicate receipt number " +
                                          std::to_wstring(record.getNumber()) + L".");
                continue;
            }
            result.history.push_back(std::move(record));
        }
        catch (const RentalException& problem)
        {
            result.warnings.push_back(skippedLabel(HistoryFile, index) + problem.message());
        }
    }
}

void DataFileFormat::parseUsers(const std::wstring& text, LoadResult& result)
{
    const std::vector<std::wstring> lines = splitLines(text);
    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        if (TextUtils::trim(lines[index]).empty())
        {
            continue;
        }
        try
        {
            result.users.push_back(parseUserLine(lines[index]));
        }
        catch (const RentalException& problem)
        {
            result.warnings.push_back(skippedLabel(UsersFile, index) + problem.message());
        }
    }
}

Vehicle DataFileFormat::parseVehicleLine(const std::wstring& line)
{
    const std::vector<std::wstring> fields = TextUtils::split(line, Separator);
    if (fields.size() != 7 && fields.size() != 8)
    {
        throw ValidationException(InputField::None, fieldCountMessage(L"7 or 8", fields.size()));
    }

    // Re-use the business classes: their constructors apply the same rules as manual entry.
    Vehicle vehicle(fields[0], fields[1], Validation::parseRate(fields[2], InputField::DailyRate));

    VehicleStatus status = VehicleStatus::Available;
    if (!tryParseVehicleStatus(fields[3], status))
    {
        throw ValidationException(InputField::None, L"unknown status \"" + TextUtils::trim(fields[3]) +
                                                    L"\" (expected Available or Rented).");
    }

    const std::wstring renterName = TextUtils::trim(fields[4]);
    const std::wstring phone = TextUtils::trim(fields[5]);
    const std::wstring agreedRate = TextUtils::trim(fields[6]);
    const std::wstring startText = fields.size() == 8 ? TextUtils::trim(fields[7]) : std::wstring();

    if (status == VehicleStatus::Available)
    {
        if (!renterName.empty() || !phone.empty() || !agreedRate.empty() || !startText.empty())
        {
            throw ValidationException(InputField::None,
                L"an Available vehicle must not have renter name, telephone, agreed rate or start date.");
        }
        return vehicle;
    }

    if (renterName.empty() || phone.empty() || agreedRate.empty())
    {
        throw ValidationException(InputField::None,
            L"a Rented vehicle must have renter name, telephone and agreed rate.");
    }
    std::optional<DateTime> startDate;
    if (!startText.empty())
    {
        startDate = DateTime::tryParse(startText);
        if (!startDate)
        {
            throw ValidationException(InputField::None, L"invalid rental start date \"" + startText +
                                                        L"\" (expected YYYY-MM-DD HH:MM).");
        }
    }
    vehicle.rent(Renter(renterName, phone), Validation::parseRate(agreedRate, InputField::AgreedRate), startDate);
    return vehicle;
}

RentalRecord DataFileFormat::parseHistoryLine(const std::wstring& line)
{
    const std::vector<std::wstring> fields = TextUtils::split(line, Separator);
    if (fields.size() != 9 && fields.size() != 10)
    {
        throw ValidationException(InputField::None, fieldCountMessage(L"9 or 10", fields.size()));
    }

    const std::wstring numberText = TextUtils::trim(fields[0]);
    if (numberText.empty() || numberText.size() > 9 || numberText.find_first_not_of(L"0123456789") != std::wstring::npos)
    {
        throw ValidationException(InputField::None, L"invalid receipt number \"" + numberText + L"\".");
    }
    const int number = std::stoi(numberText);

    // Every field is checked with the same rules as manual entry.
    const std::wstring plate = Validation::normalizePlate(fields[2]);
    const std::wstring model = Validation::validateModel(fields[3]);
    const Renter renter(fields[4], fields[5]);
    const Money agreedRate = Validation::parseRate(fields[6], InputField::AgreedRate);
    const int days = Validation::parseRentalDays(fields[7]);

    const std::optional<Money> charge = Money::tryParse(fields[8]);
    if (!charge)
    {
        throw ValidationException(InputField::None, L"invalid charge \"" + TextUtils::trim(fields[8]) + L"\".");
    }
    if (*charge != agreedRate * days)
    {
        throw ValidationException(InputField::None, L"the charge " + charge->toString() +
            L" does not equal agreed rate x days (" + (agreedRate * days).toString() + L").");
    }

    std::wstring startedOn = fields.size() == 10 ? TextUtils::trim(fields[9]) : std::wstring();
    if (!startedOn.empty() && !DateTime::tryParse(startedOn))
    {
        throw ValidationException(InputField::None, L"invalid rental start date \"" + startedOn + L"\".");
    }

    Receipt receipt(plate, model, renter.getName(), renter.getPhone(), agreedRate, days, *charge, std::move(startedOn));
    return RentalRecord(number, fields[1], std::move(receipt)); // validates number and return date
}

User DataFileFormat::parseUserLine(const std::wstring& line)
{
    const std::vector<std::wstring> fields = TextUtils::split(line, Separator);
    if (fields.size() != 4)
    {
        throw ValidationException(InputField::None, fieldCountMessage(L"4", fields.size()));
    }
    Role role = Role::Clerk;
    if (!tryParseRole(fields[1], role))
    {
        throw ValidationException(InputField::None, L"unknown role \"" + TextUtils::trim(fields[1]) +
                                                    L"\" (expected Manager or Clerk).");
    }
    return User::fromStored(fields[0], role, fields[2], fields[3]);
}

// =============================================================================
// Writing
// =============================================================================

std::wstring DataFileFormat::formatVehicles(const std::vector<Vehicle>& vehicles)
{
    std::wostringstream text;
    for (const Vehicle& vehicle : vehicles)
    {
        text << vehicle.getPlate() << Separator << vehicle.getModel() << Separator << vehicle.getDailyRate()
             << Separator << toString(vehicle.getStatus()) << Separator;
        if (const std::optional<Rental>& rental = vehicle.getRental())
        {
            text << rental->getRenter().getName() << Separator << rental->getRenter().getPhone() << Separator
                 << rental->getAgreedDailyRate() << Separator
                 << (rental->getStartDate() ? rental->getStartDate()->toString() : std::wstring());
        }
        else
        {
            text << Separator << Separator << Separator; // four empty rental fields
        }
        text << NewLine;
    }
    return text.str();
}

std::wstring DataFileFormat::formatRevenue(const Money& revenue)
{
    std::wostringstream text;
    text << revenue << NewLine; // uses Money's operator<< -> "3250.00"
    return text.str();
}

std::wstring DataFileFormat::formatHistory(const std::vector<RentalRecord>& history)
{
    std::wostringstream text;
    for (const RentalRecord& record : history)
    {
        const Receipt& receipt = record.getReceipt();
        text << record.getNumber() << Separator << record.getReturnedOn() << Separator
             << receipt.getPlate() << Separator << receipt.getModel() << Separator
             << receipt.getRenterName() << Separator << receipt.getPhone() << Separator
             << receipt.getAgreedDailyRate() << Separator << receipt.getDays() << Separator
             << receipt.getTotalCharge() << Separator << receipt.getStartedOn() << NewLine;
    }
    return text.str();
}

std::wstring DataFileFormat::formatUsers(const std::vector<User>& users)
{
    std::wostringstream text;
    for (const User& user : users)
    {
        text << user.getUsername() << Separator << toString(user.getRole()) << Separator << user.getSalt()
             << Separator << user.getPasswordHash() << NewLine;
    }
    return text.str();
}

// =============================================================================
// Bundle
// =============================================================================

std::wstring DataFileFormat::formatBundle(const AgencyData& data)
{
    std::wstring text = std::wstring(BundleHeader) + NewLine;
    text += std::wstring(L"--- ") + VehiclesFile + NewLine + formatVehicles(data.vehicles);
    text += std::wstring(L"--- ") + RevenueFile + NewLine + formatRevenue(data.revenue);
    text += std::wstring(L"--- ") + HistoryFile + NewLine + formatHistory(data.history);
    text += std::wstring(L"--- ") + UsersFile + NewLine + formatUsers(data.users);
    return text;
}

LoadResult DataFileFormat::parseBundle(const std::wstring& text)
{
    LoadResult result;
    if (TextUtils::trim(text).empty())
    {
        return result; // nothing stored yet
    }

    const std::vector<std::wstring> lines = splitLines(text);
    std::size_t index = 0;
    while (index < lines.size() && TextUtils::trim(lines[index]).empty())
    {
        ++index;
    }
    if (index >= lines.size() || TextUtils::trim(lines[index]) != BundleHeader)
    {
        throw DataStoreException(L"The shared data is not in the expected format (missing \"" +
                                 std::wstring(BundleHeader) + L"\").");
    }
    ++index;

    // Collect the lines of each section, then parse them with the normal file readers.
    std::wstring vehicles, revenue, history, users;
    std::wstring* section = nullptr;
    for (; index < lines.size(); ++index)
    {
        const std::wstring& line = lines[index];
        if (line.rfind(L"--- ", 0) == 0)
        {
            const std::wstring name = TextUtils::trim(line.substr(4));
            section = name == VehiclesFile ? &vehicles
                    : name == RevenueFile ? &revenue
                    : name == HistoryFile ? &history
                    : name == UsersFile ? &users
                                        : nullptr;
            if (section == nullptr)
            {
                result.warnings.push_back(L"Unknown section \"" + name + L"\" in the shared data skipped.");
            }
            continue;
        }
        if (section != nullptr)
        {
            *section += line + L"\n";
        }
    }
    parseVehicles(vehicles, result);
    parseRevenue(revenue, result);
    parseHistory(history, result);
    parseUsers(users, result);
    return result;
}
