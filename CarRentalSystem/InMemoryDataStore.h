// =============================================================================
// InMemoryDataStore.h
// -----------------------------------------------------------------------------
// POLYMORPHISM: an implementation of IDataStore that simply keeps the data in
// memory. The built-in self-test and the unit tests save a RentalAgency into it
// and load it back - through an IDataStore& reference - to prove that the
// agency works with ANY data store, not just text files.
// =============================================================================
#pragma once

#include "IDataStore.h"

#include <string>

class InMemoryDataStore final : public IDataStore
{
public:
    LoadResult load() override;
    void save(const AgencyData& data) override;
    std::wstring describeLocation() const override;

private:
    AgencyData m_data;
};
