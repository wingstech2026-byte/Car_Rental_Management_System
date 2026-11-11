// =============================================================================
// User.h
// -----------------------------------------------------------------------------
// User accounts and role-based access control.
//
// Two roles exist:
//   * Manager - may do everything.
//   * Clerk   - may search, rent and return vehicles and view the lists and the
//               history, but may not change the fleet, see the reports, manage
//               user accounts or replace the data.
//
// ENCAPSULATION / SECURITY: a User never stores the password itself, only a
// random salt and a salted, iterated SHA-256 hash. The only way to check a
// password is checkPassword(); the hash cannot be turned back into the password.
// =============================================================================
#pragma once

#include <string>

/// The role of a user account.
enum class Role
{
    Manager,
    Clerk
};

/// Operations that are restricted by role.
enum class Permission
{
    ManageFleet,    ///< add, edit and remove vehicles
    RentAndReturn,  ///< rent and return vehicles
    ViewReports,    ///< reports and statistics
    ManageUsers,    ///< create and remove user accounts, reset passwords
    ManageData      ///< reset or replace all data (sample data, test files)
};

/// "Manager" / "Clerk".
std::wstring toString(Role role);

/// Parses "Manager" / "Clerk" (case-insensitive). Returns false if unknown.
bool tryParseRole(const std::wstring& text, Role& role);

/// Human-readable description of a permission, e.g. "manage the fleet".
std::wstring describe(Permission permission);

class User
{
public:
    /// Creates a new account with a random salt.
    /// @throws ValidationException for an invalid user name or password.
    static User create(const std::wstring& username, const std::wstring& password, Role role);

    /// Re-creates an account read from storage (salt and hash are kept as they are).
    /// @throws ValidationException if a value is malformed.
    static User fromStored(const std::wstring& username, Role role, const std::wstring& salt, const std::wstring& hash);

    const std::wstring& getUsername() const noexcept;
    Role getRole() const noexcept;
    const std::wstring& getSalt() const noexcept;
    const std::wstring& getPasswordHash() const noexcept;

    /// True if the password is correct.
    bool checkPassword(const std::wstring& password) const;

    /// Replaces the password (new random salt). @throws ValidationException
    void setPassword(const std::wstring& newPassword);

    /// True if this user's role allows the operation.
    bool can(Permission permission) const noexcept;

    /// The permission table of the roles.
    static bool roleAllows(Role role, Permission permission) noexcept;

    /// Salted, iterated hash: h = SHA256(salt ":" password), then 999 times h = SHA256(salt ":" h).
    static std::wstring hashPassword(const std::wstring& salt, const std::wstring& password);

private:
    User(std::wstring username, Role role, std::wstring salt, std::wstring passwordHash);
    static std::wstring generateSalt();

    std::wstring m_username;
    Role m_role;
    std::wstring m_salt;
    std::wstring m_passwordHash;
};
