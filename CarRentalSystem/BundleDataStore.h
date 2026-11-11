// =============================================================================
// BundleDataStore.h
// -----------------------------------------------------------------------------
// POLYMORPHISM: an IDataStore that keeps all data as one text "bundle"
// (see DataFileFormat). It is used by the web version: the page moves the
// bundle text to and from the browser storage or the shared online database,
// and the C++ code only ever sees this IDataStore.
// =============================================================================
#pragma once

#include "IDataStore.h"

#include <string>

class BundleDataStore final : public IDataStore
{
public:
    explicit BundleDataStore(std::wstring bundle = std::wstring());

    LoadResult load() override;
    void save(const AgencyData& data) override;
    std::wstring describeLocation() const override;

    const std::wstring& getBundle() const noexcept;
    void setBundle(std::wstring bundle);

private:
    std::wstring m_bundle;
};
