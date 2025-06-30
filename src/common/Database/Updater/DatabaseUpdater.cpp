/*
 * This file is part of the DestinyCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "DatabaseUpdater.h"
#include "DatabaseEnv.h"
#include "MySQLConnection.h"
#include "Log.h"
#include "Configuration/Config.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

// -----------------------------
// Implementation of DatabaseUpdater<T> methods
// -----------------------------

template<typename T>
DatabaseUpdater<T>::DatabaseUpdater(DatabaseWorkerPool<T>& dbPool, const std::string& sqlFolderPath)
    : _dbPool(dbPool), _sqlUpdater(sqlFolderPath)
{
}

template<typename T>
bool DatabaseUpdater<T>::ApplyUpdates()
{
    bool allSuccess = true;
    auto files = _sqlUpdater.GetSqlFiles();

    if (files.empty())
        TC_LOG_INFO("sql.updater", "No SQL update files found in folder '%s'.", _sqlUpdater.GetPath().c_str());

    for (const auto& file : files)
    {
        std::string filename = std::filesystem::path(file).filename().string();

        if (HasUpdateBeenApplied(filename))
        {
            TC_LOG_INFO("sql.updater", "Update '%s' has already been applied. Skipping.", filename.c_str());
            continue;
        }

        TC_LOG_INFO("sql.updater", "Applying update '%s'...", filename.c_str());

        if (ExecuteSqlFile(file))
        {
            SetUpdateAsApplied(filename);
            TC_LOG_INFO("sql.updater", "Update '%s' applied successfully.", filename.c_str());
        }
        else
        {
            TC_LOG_ERROR("sql.updater", "Failed to apply update '%s'!", filename.c_str());
            allSuccess = false;
        }
    }

    return allSuccess;
}

template<typename T>
bool DatabaseUpdater<T>::ExecuteSqlFile(const std::string& filePath)
{
    TC_LOG_INFO("sql.updater", "Applying SQL file: %s", filePath.c_str());

    std::vector<std::string> statements;
    if (!_sqlUpdater.LoadSqlFile(filePath, statements))
    {
        TC_LOG_ERROR("sql.updater", "Failed to load SQL file: %s", filePath.c_str());
        return false;
    }

    try
    {
        for (const std::string& stmt : statements)
        {
            if (stmt.empty())
                continue;

            TC_LOG_DEBUG("sql.updater", "Executing: %s", stmt.c_str());
            _dbPool.DirectExecute(stmt.c_str()); // void Rückgabe, keine Prüfung hier
        }
    }
    catch (const std::exception& ex)
    {
        TC_LOG_ERROR("sql.updater", "SQL execution failed: %s", ex.what());
        return false;
    }

    return true;
}

template<typename T>
bool DatabaseUpdater<T>::HasUpdateBeenApplied(const std::string& filename)
{
    auto stmt = _dbPool.GetPreparedStatement(GetSelectStatementId());
    stmt->setString(0, filename);

    if (auto result = _dbPool.Query(stmt))
    {
        return true;
    }

    return false;
}

template<typename T>
void DatabaseUpdater<T>::SetUpdateAsApplied(const std::string& filename)
{
    auto stmt = _dbPool.GetPreparedStatement(GetInsertStatementId());
    stmt->setString(0, filename);
    _dbPool.Execute(stmt);
}

// -----------------------------
// Statement ID specializations
// -----------------------------

template<>
uint32 DatabaseUpdater<WorldDatabaseConnection>::GetSelectStatementId() { return WORLD_SEL_APPLIED_UPDATE; }

template<>
uint32 DatabaseUpdater<WorldDatabaseConnection>::GetInsertStatementId() { return WORLD_INS_APPLIED_UPDATE; }

template<>
uint32 DatabaseUpdater<CharacterDatabaseConnection>::GetSelectStatementId() { return CHAR_SEL_APPLIED_UPDATE; }

template<>
uint32 DatabaseUpdater<CharacterDatabaseConnection>::GetInsertStatementId() { return CHAR_INS_APPLIED_UPDATE; }

template<>
uint32 DatabaseUpdater<LoginDatabaseConnection>::GetSelectStatementId() { return LOGIN_SEL_APPLIED_UPDATE; }

template<>
uint32 DatabaseUpdater<LoginDatabaseConnection>::GetInsertStatementId() { return LOGIN_INS_APPLIED_UPDATE; }

// -----------------------------
// Explicit template instantiations for classes and functions
// -----------------------------

template class DatabaseUpdater<WorldDatabaseConnection>;
template class DatabaseUpdater<CharacterDatabaseConnection>;
template class DatabaseUpdater<LoginDatabaseConnection>;

template bool ApplyDatabaseUpdates<WorldDatabaseConnection>(DatabaseWorkerPool<WorldDatabaseConnection>&, const char*, const char*);
template bool ApplyDatabaseUpdates<CharacterDatabaseConnection>(DatabaseWorkerPool<CharacterDatabaseConnection>&, const char*, const char*);
template bool ApplyDatabaseUpdates<LoginDatabaseConnection>(DatabaseWorkerPool<LoginDatabaseConnection>&, const char*, const char*);
