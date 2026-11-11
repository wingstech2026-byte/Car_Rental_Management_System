// =============================================================================
// BundleDataStore.cpp
// =============================================================================
#include "BundleDataStore.h"

#include "DataFileFormat.h"

#include <utility>

BundleDataStore::BundleDataStore(std::wstring bundle)
    : m_bundle(std::move(bundle))
{
}

LoadResult BundleDataStore::load()
{
    return DataFileFormat::parseBundle(m_bundle);
}

void BundleDataStore::save(const AgencyData& data)
{
    m_bundle = DataFileFormat::formatBundle(data);
}

std::wstring BundleDataStore::describeLocation() const
{
    return L"data bundle";
}

const std::wstring& BundleDataStore::getBundle() const noexcept
{
    return m_bundle;
}

void BundleDataStore::setBundle(std::wstring bundle)
{
    m_bundle = std::move(bundle);
}
