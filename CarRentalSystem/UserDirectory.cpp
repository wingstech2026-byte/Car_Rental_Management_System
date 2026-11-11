// =============================================================================
// UserDirectory.cpp
// =============================================================================
#include "UserDirectory.h"

#include "RentalExceptions.h"
#include "TextUtils.h"
#include "Validation.h"

#include <algorithm>

UserDirectory::UserDirectory()
{
    addDefaults();
}

void UserDirectory::replaceAll(const std::vector<User>& users, std::vector<std::wstring>& warnings)
{
    std::map<std::wstring, User> accounts;
    for (const User& user : users)
    {
        if (!accounts.emplace(user.getUsername(), user).second)
        {
            warnings.push_back(L"Duplicate user account \"" + user.getUsername() + L"\" skipped.");
        }
    }
    m_users = std::move(accounts);
    if (managerCount() == 0)
    {
        addDefaults(); // never lock everybody out
    }
}

const User& UserDirectory::authenticate(const std::wstring& username, const std::wstring& password) const
{
    const User* user = find(username);
    if (user == nullptr || !user->checkPassword(password))
    {
        throw AuthenticationException(); // same message for both cases (no hint which one was wrong)
    }
    return *user;
}

void UserDirectory::addUser(const std::wstring& username, const std::wstring& password, Role role)
{
    User user = User::create(username, password, role); // validates name and password
    if (m_users.find(user.getUsername()) != m_users.end())
    {
        throw ValidationException(InputField::Username, L"A user named \"" + user.getUsername() + L"\" already exists.");
    }
    const std::wstring key = user.getUsername();
    m_users.emplace(key, std::move(user));
}

void UserDirectory::removeUser(const std::wstring& username, const std::wstring& currentUsername)
{
    User& user = findMutable(username);
    if (TextUtils::equalsIgnoreCase(user.getUsername(), TextUtils::trim(currentUsername)))
    {
        throw ValidationException(InputField::Username, L"You cannot delete your own account while you are logged in.");
    }
    if (user.getRole() == Role::Manager && managerCount() <= 1)
    {
        throw ValidationException(InputField::Username, L"The last Manager account cannot be deleted.");
    }
    const std::wstring key = user.getUsername();
    m_users.erase(key);
}

void UserDirectory::resetPassword(const std::wstring& username, const std::wstring& newPassword)
{
    findMutable(username).setPassword(newPassword);
}

const User* UserDirectory::find(const std::wstring& username) const
{
    const auto position = m_users.find(TextUtils::toLower(TextUtils::trim(username)));
    return position == m_users.end() ? nullptr : &position->second;
}

std::vector<User> UserDirectory::getUsers() const
{
    std::vector<User> users;
    for (const auto& entry : m_users)
    {
        users.push_back(entry.second);
    }
    return users;
}

std::size_t UserDirectory::count() const noexcept
{
    return m_users.size();
}

bool UserDirectory::usesDefaultPasswords() const
{
    const User* manager = find(DefaultManagerName);
    const User* clerk = find(DefaultClerkName);
    return (manager != nullptr && manager->checkPassword(DefaultManagerPassword)) ||
           (clerk != nullptr && clerk->checkPassword(DefaultClerkPassword));
}

User& UserDirectory::findMutable(const std::wstring& username)
{
    const auto position = m_users.find(TextUtils::toLower(TextUtils::trim(username)));
    if (position == m_users.end())
    {
        throw RentalException(L"No user account named \"" + TextUtils::trim(username) + L"\" exists.");
    }
    return position->second;
}

std::size_t UserDirectory::managerCount() const noexcept
{
    return static_cast<std::size_t>(std::count_if(m_users.begin(), m_users.end(),
        [](const auto& entry) { return entry.second.getRole() == Role::Manager; }));
}

void UserDirectory::addDefaults()
{
    if (m_users.find(DefaultManagerName) == m_users.end())
    {
        m_users.emplace(DefaultManagerName, User::create(DefaultManagerName, DefaultManagerPassword, Role::Manager));
    }
    if (m_users.find(DefaultClerkName) == m_users.end())
    {
        m_users.emplace(DefaultClerkName, User::create(DefaultClerkName, DefaultClerkPassword, Role::Clerk));
    }
}
