// =============================================================================
// InMemoryDataStore.cpp
// =============================================================================
#include "InMemoryDataStore.h"

LoadResult InMemoryDataStore::load()
{
    LoadResult result;
    static_cast<AgencyData&>(result) = m_data; // copy the stored data into the result
    return result;
}

void InMemoryDataStore::save(const AgencyData& data)
{
    m_data = data;
}

std::wstring InMemoryDataStore::describeLocation() const
{
    return L"(memory)";
}
