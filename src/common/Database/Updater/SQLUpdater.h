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
#include <vector>

class SqlUpdater
{
public:
    explicit SqlUpdater(const std::string& sqlFolderPath);

    std::vector<std::string> GetSqlFiles() const;
    bool LoadSqlFile(const std::string& filePath, std::vector<std::string>& statements) const;

    const std::string& GetPath() const { return _sqlFolderPath; }

private:
    std::string _sqlFolderPath;
};
