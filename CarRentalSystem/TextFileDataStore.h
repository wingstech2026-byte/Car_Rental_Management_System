// =============================================================================
// TextFileDataStore.h
// -----------------------------------------------------------------------------
// INHERITANCE: concrete implementation of the IDataStore interface that uses
// four UTF-8 text files in a folder (normally the folder of the .exe):
// vehicles.txt, revenue.txt, history.txt and users.txt. The format of the
// files is defined by DataFileFormat.
//
// Missing files mean "no data yet". Files are written to a temporary file
// first and then renamed, so a crash while saving cannot leave a half-written
// data file behind.
// =============================================================================
#pragma once

#include "IDataStore.h"

#include <filesystem>
#include <string>

class TextFileDataStore final : public IDataStore
{
public:
    explicit TextFileDataStore(std::filesystem::path directory);

    // ---- IDataStore implementation (override = checked by the compiler) ----
    LoadResult load() override;
    void save(const AgencyData& data) override;
    std::wstring describeLocation() const override;

private:
    /// Reads a whole UTF-8 file; an empty text if the file does not exist.
    /// Invalid UTF-8 is replaced and reported as a warning.
    std::wstring readText(const wchar_t* fileName, LoadResult& result) const;

    /// Writes content to "<target>.tmp" and then replaces the target with it.
    static void writeFileAtomically(const std::filesystem::path& target, const std::string& content);

    std::filesystem::path m_directory;
};
