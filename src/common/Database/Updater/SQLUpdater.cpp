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

#include "SQLUpdater.h"
#include "Log.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

SqlUpdater::SqlUpdater(const std::string& sqlFolderPath)
{
    fs::path p(sqlFolderPath);

    if (p.is_absolute())
        _sqlFolderPath = p.string();
    else
        _sqlFolderPath = (fs::current_path() / p).string();

    TC_LOG_INFO("sql.updater", "SQL update folder resolved to: %s", _sqlFolderPath.c_str());
}

std::vector<std::string> SqlUpdater::GetSqlFiles() const
{
    std::vector<std::string> files;

    if (!fs::exists(_sqlFolderPath) || !fs::is_directory(_sqlFolderPath))
    {
        TC_LOG_ERROR("sql.updater", "SQL update folder does not exist: %s", _sqlFolderPath.c_str());
        return files;
    }

    for (const auto& entry : fs::directory_iterator(_sqlFolderPath))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".sql")
        {
            files.push_back(entry.path().string());
            TC_LOG_INFO("sql.updater", "Found SQL file: %s", entry.path().filename().string().c_str());
        }
    }

    std::sort(files.begin(), files.end());
    return files;
}

bool SqlUpdater::LoadSqlFile(const std::string& filePath, std::vector<std::string>& statements) const
{
    std::ifstream file(filePath);
    if (!file)
    {
        TC_LOG_ERROR("sql.updater", "Cannot open SQL file: %s", filePath.c_str());
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    std::string delimiter = ";";
    std::string statement;
    size_t pos = 0;

    while (pos < content.size())
    {
        size_t nextPos = content.find(delimiter, pos);

        // Handle DELIMITER change
        size_t delimiterPos = content.find("DELIMITER ", pos);
        if (delimiterPos != std::string::npos && delimiterPos < nextPos)
        {
            size_t lineEnd = content.find_first_of("\r\n", delimiterPos);
            if (lineEnd == std::string::npos) lineEnd = content.size();

            std::string newDelimiter = content.substr(delimiterPos + 10, lineEnd - (delimiterPos + 10));
            newDelimiter.erase(newDelimiter.find_last_not_of(" \t\r\n") + 1);

            delimiter = newDelimiter;
            pos = lineEnd + 1;
            continue;
        }

        if (nextPos == std::string::npos)
            nextPos = content.size();

        statement = content.substr(pos, nextPos - pos);
        pos = nextPos + delimiter.size();

        // Clean up whitespace
        statement.erase(0, statement.find_first_not_of(" \t\r\n"));
        statement.erase(statement.find_last_not_of(" \t\r\n") + 1);

        if (statement.empty())
            continue;

        // Skip comments
        if (statement.rfind("--", 0) == 0 || statement.rfind("#", 0) == 0)
            continue;

        statements.push_back(statement);
    }

    return true;
}
