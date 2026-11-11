// =============================================================================
// IDataStore.h
// -----------------------------------------------------------------------------
// ABSTRACTION + INHERITANCE + POLYMORPHISM
//
// IDataStore is an abstract base class (an "interface"): it says WHAT a data
// store can do - load() and save() - but not HOW. It has only pure virtual
// functions, so it cannot be instantiated.
//
// Concrete classes inherit from it and provide the "how":
//   * TextFileDataStore  - UTF-8 text files next to the program
//   * RemoteDataStore    - the shared online database (desktop program, HTTPS)
//   * BundleDataStore    - all data as one text "bundle" in memory (web version)
//   * InMemoryDataStore  - plain objects in memory (used by the self-test)
//
// RentalAgency::loadFrom(IDataStore&) and RentalAgency::saveTo(IDataStore&)
// only know this interface. The call store.load() is resolved at RUN TIME to
// the implementation of the actual object (dynamic dispatch through the
// virtual table) - that is polymorphism. Switching from local files to the
// shared online database did not require any change in RentalAgency.
// =============================================================================
#pragma once

#include "Money.h"
#include "RentalRecord.h"
#include "User.h"
#include "Vehicle.h"

#include <string>
#include <vector>

/// All data of the agency.
struct AgencyData
{
    std::vector<Vehicle> vehicles;       ///< The fleet.
    Money revenue;                       ///< Cumulative rental revenue.
    std::vector<RentalRecord> history;   ///< Completed rentals.
    std::vector<User> users;             ///< User accounts (empty = use the default accounts).
};

/// Everything a data store returns when loading.
struct LoadResult : AgencyData
{
    std::vector<std::wstring> warnings;  ///< One message per skipped/invalid entry.
};

class IDataStore
{
public:
    /// Virtual destructor: deleting a derived object through an IDataStore
    /// pointer/reference must run the derived destructor.
    virtual ~IDataStore();

    /// Loads all data. Invalid entries are skipped and reported in LoadResult::warnings.
    /// @throws DataStoreException if the storage cannot be read at all.
    virtual LoadResult load() = 0;

    /// Replaces the stored data.
    /// @throws DataStoreException if the data cannot be written
    ///         (DataConflictException if another user changed it in the meantime).
    virtual void save(const AgencyData& data) = 0;

    /// Human-readable description of where the data lives (e.g. a folder or a web address).
    virtual std::wstring describeLocation() const = 0;
};
