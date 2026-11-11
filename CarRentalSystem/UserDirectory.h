// =============================================================================
// UserDirectory.h
// -----------------------------------------------------------------------------
// The collection of user accounts. It authenticates logins and protects the
// account rules: user names are unique, there is always at least one Manager,
// and nobody can delete their own account.
//
// When no accounts exist (first start), two default accounts are created:
//   manager / manager123  (Manager)     clerk / clerk123  (Clerk)
// The GUIs remind the Manager to change these passwords.
// =============================================================================
#pragma once

#include "User.h"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

class UserDirectory
{
public:
    static constexpr const wchar_t* DefaultManagerName = L"manager";
    static constexpr const wchar_t* DefaultManagerPassword = L"manager123";
    static constexpr const wchar_t* DefaultClerkName = L"clerk";
    static constexpr const wchar_t* DefaultClerkPassword = L"clerk123";

    /// Creates the directory with the two default accounts.
    UserDirectory();

    /// Replaces all accounts (duplicate names are skipped with a warning). If no
    /// Manager remains, the default accounts are added so the system stays usable.
    void replaceAll(const std::vector<User>& users, std::vector<std::wstring>& warnings);

    /// @return the account. @throws AuthenticationException
    const User& authenticate(const std::wstring& username, const std::wstring& password) const;

    /// @throws ValidationException (invalid or duplicate user name, invalid password)
    void addUser(const std::wstring& username, const std::wstring& password, Role role);

    /// @throws ValidationException when removing yourself or the last Manager; RentalException if unknown.
    void removeUser(const std::wstring& username, const std::wstring& currentUsername);

    /// @throws ValidationException (invalid password); RentalException if the user is unknown.
    void resetPassword(const std::wstring& username, const std::wstring& newPassword);

    /// @return nullptr if there is no such user.
    const User* find(const std::wstring& username) const;

    /// All accounts, sorted by user name.
    std::vector<User> getUsers() const;

    std::size_t count() const noexcept;

    /// True if a default account still has its default password.
    bool usesDefaultPasswords() const;

private:
    User& findMutable(const std::wstring& username);
    std::size_t managerCount() const noexcept;
    void addDefaults();

    std::map<std::wstring, User> m_users; ///< Key = lower-case user name.
};
