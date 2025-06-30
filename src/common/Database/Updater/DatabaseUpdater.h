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

#pragma once

#include <string>
#include "SQLUpdater.h"
#include "Log.h"
#include "Configuration/Config.h"
#include <filesystem>

namespace fs = std::filesystem;

template<typename T>
class DatabaseWorkerPool;

template<typename T>
class DatabaseUpdater
{
public:
    DatabaseUpdater(DatabaseWorkerPool<T>& dbPool, const std::string& sqlFolderPath);

    bool ApplyUpdates();

private:
    DatabaseWorkerPool<T>& _dbPool;
    SqlUpdater _sqlUpdater;

    bool ExecuteSqlFile(const std::string& filePath);
    bool HasUpdateBeenApplied(const std::string& filename);
    void SetUpdateAsApplied(const std::string& filename);

    uint32 GetSelectStatementId();
    uint32 GetInsertStatementId();
};

// --- Full template function definition must be in the header ---

template<typename ConnectionType>
bool ApplyDatabaseUpdates(DatabaseWorkerPool<ConnectionType>& pool, const char* name, const char* configKey)
{
    std::string path = sConfigMgr->GetStringDefault(configKey, "");

    if (path.empty())
    {
        TC_LOG_INFO("sql.updater", "No SQL update path configured for '%s'. Skipping.", name);
        return true;
    }

    TC_LOG_INFO("sql.updater", "Checking for SQL updates for '%s' in path '%s'...", name, path.c_str());

    DatabaseUpdater<ConnectionType> updater(pool, path);

    if (!updater.ApplyUpdates())
    {
        TC_LOG_ERROR("sql.updater", "SQL updates for '%s' failed!", name);
        return false;
    }

    TC_LOG_INFO("sql.updater", "SQL updates for '%s' applied successfully.", name);
    return true;
}
