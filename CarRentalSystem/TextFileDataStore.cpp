// =============================================================================
// TextFileDataStore.cpp
// =============================================================================
#include "TextFileDataStore.h"

#include "RentalExceptions.h"
#include "TextUtils.h"
#include "Validation.h"

#include <fstream>
#include <iterator>
#include <optional>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

TextFileDataStore::TextFileDataStore(std::filesystem::path directory)
    : m_directory(std::move(directory))
{
}

LoadResult TextFileDataStore::load()
{
    LoadResult result;
    loadVehicles(result);
    loadRevenue(result);
    return result;
}

void TextFileDataStore::save(const std::vector<Vehicle>& vehicles, const Money& revenue)
{
    std::error_code directoryError;
    std::filesystem::create_directories(m_directory, directoryError);

    std::wostringstream vehicleText;
    for (const Vehicle& vehicle : vehicles)
    {
        vehicleText << formatVehicleLine(vehicle) << L"\r\n";
    }
    writeFileAtomically(m_directory / VehiclesFileName, TextUtils::toUtf8(vehicleText.str()));

    std::wostringstream revenueText;
    revenueText << revenue << L"\r\n"; // uses Money's operator<< -> "3250.00"
    writeFileAtomically(m_directory / RevenueFileName, TextUtils::toUtf8(revenueText.str()));
}

std::wstring TextFileDataStore::describeLocation() const
{
    return m_directory.wstring();
}

void TextFileDataStore::loadVehicles(LoadResult& result) const
{
    const std::filesystem::path path = m_directory / VehiclesFileName;
    std::error_code existsError;
    if (!std::filesystem::exists(path, existsError))
    {
        return; // missing file = empty fleet
    }

    const std::vector<std::string> lines = readLines(path);
    std::set<std::wstring> seenPlates;

    for (std::size_t index = 0; index < lines.size(); ++index)
    {
        const std::wstring lineLabel =
            std::wstring(VehiclesFileName) + L" line " + std::to_wstring(index + 1) + L" skipped: ";

        std::wstring line;
        if (!TextUtils::tryFromUtf8(lines[index], line))
        {
            result.warnings.push_back(lineLabel + L"the line is not valid UTF-8 text.");
            continue;
        }
        if (TextUtils::trim(line).empty())
        {
            continue; // blank lines are simply ignored
        }

        try
        {
            Vehicle vehicle = parseVehicleLine(line);
            if (!seenPlates.insert(vehicle.getPlate()).second)
            {
                result.warnings.push_back(lineLabel + L"duplicate license plate " + vehicle.getPlate() + L".");
                continue;
            }
            result.vehicles.push_back(std::move(vehicle));
        }
        catch (const RentalException& problem)
        {
            result.warnings.push_back(lineLabel + problem.message());
        }
    }
}

void TextFileDataStore::loadRevenue(LoadResult& result) const
{
    const std::filesystem::path path = m_directory / RevenueFileName;
    std::error_code existsError;
    if (!std::filesystem::exists(path, existsError))
    {
        return; // missing file = zero revenue
    }

    std::wstring text;
    for (const std::string& rawLine : readLines(path))
    {
        std::wstring decoded;
        if (TextUtils::tryFromUtf8(rawLine, decoded) && !TextUtils::trim(decoded).empty())
        {
            text = TextUtils::trim(decoded);
            break;
        }
    }
    if (text.empty())
    {
        return; // empty file = zero revenue
    }

    const std::optional<Money> amount = Money::tryParse(text);
    if (!amount)
    {
        result.warnings.push_back(std::wstring(RevenueFileName) + L" skipped: \"" + text +
                                  L"\" is not a valid amount - cumulative revenue starts at 0.00.");
        return;
    }
    result.revenue = *amount;
}

Vehicle TextFileDataStore::parseVehicleLine(const std::wstring& line)
{
    const std::vector<std::wstring> fields = TextUtils::split(line, Validation::FieldSeparator);
    if (fields.size() != FieldCount)
    {
        throw ValidationException(InputField::None, L"expected " + std::to_wstring(FieldCount) +
            L" fields separated by '|' but found " + std::to_wstring(fields.size()) + L".");
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

    if (status == VehicleStatus::Available)
    {
        if (!renterName.empty() || !phone.empty() || !agreedRate.empty())
        {
            throw ValidationException(InputField::None,
                L"an Available vehicle must not have renter name, telephone or agreed rate.");
        }
    }
    else
    {
        if (renterName.empty() || phone.empty() || agreedRate.empty())
        {
            throw ValidationException(InputField::None,
                L"a Rented vehicle must have renter name, telephone and agreed rate.");
        }
        vehicle.rent(Renter(renterName, phone), Validation::parseRate(agreedRate, InputField::AgreedRate));
    }
    return vehicle;
}

std::wstring TextFileDataStore::formatVehicleLine(const Vehicle& vehicle)
{
    const wchar_t separator = Validation::FieldSeparator;
    std::wostringstream line;
    line << vehicle.getPlate() << separator << vehicle.getModel() << separator << vehicle.getDailyRate()
         << separator << toString(vehicle.getStatus()) << separator;

    if (const std::optional<Rental>& rental = vehicle.getRental())
    {
        line << rental->getRenter().getName() << separator << rental->getRenter().getPhone() << separator
             << rental->getAgreedDailyRate();
    }
    else
    {
        line << separator << separator; // three empty rental fields
    }
    return line.str();
}

std::vector<std::string> TextFileDataStore::readLines(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw DataStoreException(L"Cannot open " + path.wstring() + L" for reading.");
    }
    std::string content((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());

    // Remove a UTF-8 byte order mark (Notepad may add one).
    if (content.size() >= 3 && content.compare(0, 3, "\xEF\xBB\xBF") == 0)
    {
        content.erase(0, 3);
    }

    std::vector<std::string> lines;
    std::istringstream stream(content);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back(); // Windows line ending
        }
        lines.push_back(line);
    }
    return lines;
}

void TextFileDataStore::writeFileAtomically(const std::filesystem::path& target, const std::string& content)
{
    std::filesystem::path temporary = target;
    temporary += L".tmp";

    {
        // RAII: the ofstream closes the file automatically at the end of this block.
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        if (!output)
        {
            throw DataStoreException(L"Cannot open " + temporary.wstring() + L" for writing.");
        }
        output.write(content.data(), static_cast<std::streamsize>(content.size()));
        output.flush();
        if (!output)
        {
            throw DataStoreException(L"Could not write " + temporary.wstring() + L" (disk full?).");
        }
    }

    std::error_code renameError;
    std::filesystem::rename(temporary, target, renameError); // replaces the old file
    if (renameError)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw DataStoreException(L"Could not replace " + target.wstring() + L": " +
                                 TextUtils::fromUtf8Lenient(renameError.message()));
    }
}
