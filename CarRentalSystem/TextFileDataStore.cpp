// =============================================================================
// TextFileDataStore.cpp
// =============================================================================
#include "TextFileDataStore.h"

#include "DataFileFormat.h"
#include "RentalExceptions.h"
#include "TextUtils.h"

#include <fstream>
#include <iterator>
#include <system_error>
#include <utility>

TextFileDataStore::TextFileDataStore(std::filesystem::path directory)
    : m_directory(std::move(directory))
{
}

LoadResult TextFileDataStore::load()
{
    LoadResult result;
    DataFileFormat::parseVehicles(readText(DataFileFormat::VehiclesFile, result), result);
    DataFileFormat::parseRevenue(readText(DataFileFormat::RevenueFile, result), result);
    DataFileFormat::parseHistory(readText(DataFileFormat::HistoryFile, result), result);
    DataFileFormat::parseUsers(readText(DataFileFormat::UsersFile, result), result);
    return result;
}

void TextFileDataStore::save(const AgencyData& data)
{
    std::error_code directoryError;
    std::filesystem::create_directories(m_directory, directoryError);

    writeFileAtomically(m_directory / DataFileFormat::VehiclesFile,
                        TextUtils::toUtf8(DataFileFormat::formatVehicles(data.vehicles)));
    writeFileAtomically(m_directory / DataFileFormat::RevenueFile,
                        TextUtils::toUtf8(DataFileFormat::formatRevenue(data.revenue)));
    writeFileAtomically(m_directory / DataFileFormat::HistoryFile,
                        TextUtils::toUtf8(DataFileFormat::formatHistory(data.history)));
    writeFileAtomically(m_directory / DataFileFormat::UsersFile,
                        TextUtils::toUtf8(DataFileFormat::formatUsers(data.users)));
}

std::wstring TextFileDataStore::describeLocation() const
{
    return m_directory.wstring();
}

std::wstring TextFileDataStore::readText(const wchar_t* fileName, LoadResult& result) const
{
    const std::filesystem::path path = m_directory / fileName;
    std::error_code existsError;
    if (!std::filesystem::exists(path, existsError))
    {
        return std::wstring(); // missing file = no data
    }

    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        throw DataStoreException(L"Cannot open " + path.wstring() + L" for reading.");
    }
    std::string bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (bytes.size() >= 3 && bytes.compare(0, 3, "\xEF\xBB\xBF") == 0)
    {
        bytes.erase(0, 3); // UTF-8 byte order mark (Notepad may add one)
    }

    std::wstring text;
    if (!TextUtils::tryFromUtf8(bytes, text))
    {
        result.warnings.push_back(std::wstring(fileName) +
                                  L" contains invalid UTF-8 text; the invalid characters were replaced.");
        text = TextUtils::fromUtf8Lenient(bytes);
    }
    return text;
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
