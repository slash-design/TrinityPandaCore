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

#include "Common.h"

#ifdef _WIN32
  #include <winsock2.h>
#endif
#include <mysql.h>
#include <mysqld_error.h>
#include <errmsg.h>

#include "MySQLConnection.h"
#include "MySQLThreading.h"
#include "QueryResult.h"
#include "SQLOperation.h"
#include "PreparedStatement.h"
#include "DatabaseWorker.h"
#include "Timer.h"
#include "Log.h"

MySQLConnection::MySQLConnection(MySQLConnectionInfo& connInfo, ConnectionFlags index)
    : m_reconnecting(false), m_prepareError(false), m_Mysql(NULL), m_connectionInfo(connInfo), m_connectionFlags(index)
{
}

MySQLConnection::~MySQLConnection()
{
    Close();
}

void MySQLConnection::Close()
{
    ASSERT(m_Mysql); /// MySQL context must be present at this point

    for (size_t i = 0; i < m_stmts.size(); ++i)
        delete m_stmts[i];

    mysql_close(m_Mysql);
}

bool MySQLConnection::Open()
{
    MYSQL *mysqlInit;
    mysqlInit = mysql_init(NULL);
    if (!mysqlInit)
    {
        TC_LOG_ERROR("sql.driver", "Could not initialize Mysql connection to database `%s`", m_connectionInfo.database.c_str());
        return false;
    }

    int port;
    char const* unix_socket;
    //unsigned int timeout = 10;

    mysql_options(mysqlInit, MYSQL_SET_CHARSET_NAME, "utf8");
    //mysql_options(mysqlInit, MYSQL_OPT_READ_TIMEOUT, (char const*)&timeout);
    #ifdef _WIN32
    if (m_connectionInfo.host == ".")                                           // named pipe use option (Windows)
    {
        unsigned int opt = MYSQL_PROTOCOL_PIPE;
        mysql_options(mysqlInit, MYSQL_OPT_PROTOCOL, (char const*)&opt);
        port = 0;
        unix_socket = 0;
    }
    else                                                    // generic case
    {
        port = atoi(m_connectionInfo.port_or_socket.c_str());
        unix_socket = 0;
    }
    #else
    if (m_connectionInfo.host == ".")                                           // socket use option (Unix/Linux)
    {
        unsigned int opt = MYSQL_PROTOCOL_SOCKET;
        mysql_options(mysqlInit, MYSQL_OPT_PROTOCOL, (char const*)&opt);
        m_connectionInfo.host = "localhost";
        port = 0;
        unix_socket = m_connectionInfo.port_or_socket.c_str();
    }
    else                                                    // generic case
    {
        port = atoi(m_connectionInfo.port_or_socket.c_str());
        unix_socket = 0;
    }
    #endif

    m_Mysql = mysql_real_connect(mysqlInit, m_connectionInfo.host.c_str(), m_connectionInfo.user.c_str(),
        m_connectionInfo.password.c_str(), m_connectionInfo.database.c_str(), port, unix_socket, 0);

    if (m_Mysql)
    {
        if (!m_reconnecting)
        {
            TC_LOG_INFO("sql.driver", "MySQL client library: %s", mysql_get_client_info());
            TC_LOG_INFO("sql.driver", "MySQL server ver: %s ", mysql_get_server_info(m_Mysql));
            // MySQL version above 5.1 IS required in both client and server and there is no known issue with different versions above 5.1
            // if (mysql_get_server_version(m_Mysql) != mysql_get_client_version())
            //     TC_LOG_INFO("sql.driver", "[WARNING] MySQL client/server version mismatch; may conflict with behaviour of prepared statements.");
        }

        TC_LOG_INFO("sql.driver", "Connected to MySQL database at %s", m_connectionInfo.host.c_str());
        mysql_autocommit(m_Mysql, 1);

        // set connection properties to UTF8 to properly handle locales for different
        // server configs - core sends data in UTF8, so MySQL must expect UTF8 too
        mysql_set_character_set(m_Mysql, "utf8");
        return PrepareStatements();
    }
    else
    {
        TC_LOG_ERROR("sql.driver", "Could not connect to MySQL database at %s: %s\n", m_connectionInfo.host.c_str(), mysql_error(mysqlInit));
        mysql_close(mysqlInit);
        return false;
    }
}

bool MySQLConnection::PrepareStatements()
{
    DoPrepareStatements();
    return !m_prepareError;
}

bool MySQLConnection::Execute(const char* sql)
{
    if (!m_Mysql)
        return false;

    {
        uint32 _s = getMSTime();

        if (mysql_query(m_Mysql, sql))
        {
            uint32 lErrno = mysql_errno(m_Mysql);

            TC_LOG_ERROR("sql.driver", "SQL: %s", sql);
            TC_LOG_ERROR("sql.driver", "[%u] %s", lErrno, mysql_error(m_Mysql));

            if (_HandleMySQLErrno(lErrno))  // If it returns true, an error was handled successfully (i.e. reconnection)
                return Execute(sql);       // Try again

            return false;
        }
        else
            TC_LOG_DEBUG("sql.driver", "[%u ms] SQL: %s", getMSTimeDiff(_s, getMSTime()), sql);
    }

    return true;
}

bool MySQLConnection::Execute(PreparedStatement* stmt)
{
    if (!m_Mysql)
        return false;

    uint32 index = stmt->m_index;
    {
        MySQLPreparedStatement* m_mStmt = GetPreparedStatement(index);
        ASSERT(m_mStmt);            // Can only be null if preparation failed, server side error or bad query
        m_mStmt->m_stmt = stmt;     // Cross reference them for debug output
        stmt->m_stmt = m_mStmt;     /// @todo Cleaner way

        stmt->BindParameters();

        MYSQL_STMT* msql_STMT = m_mStmt->GetSTMT();
        MYSQL_BIND* msql_BIND = m_mStmt->GetBind();

        uint32 _s = getMSTime();

        if (mysql_stmt_bind_param(msql_STMT, msql_BIND))
        {
            uint32 lErrno = mysql_errno(m_Mysql);
            TC_LOG_ERROR("sql.driver", "SQL(p): %s\n [ERROR]: [%u] %s", m_mStmt->getQueryString(m_queries[index].first).c_str(), lErrno, mysql_stmt_error(msql_STMT));

            if (_HandleMySQLErrno(lErrno))  // If it returns true, an error was handled successfully (i.e. reconnection)
                return Execute(stmt);       // Try again

            m_mStmt->ClearParameters();
            return false;
        }

        if (mysql_stmt_execute(msql_STMT))
        {
            uint32 lErrno = mysql_errno(m_Mysql);
            TC_LOG_ERROR("sql.driver", "SQL(p): %s\n [ERROR]: [%u] %s", m_mStmt->getQueryString(m_queries[index].first).c_str(), lErrno, mysql_stmt_error(msql_STMT));

            if (_HandleMySQLErrno(lErrno))  // If it returns true, an error was handled successfully (i.e. reconnection)
                return Execute(stmt);       // Try again

            m_mStmt->ClearParameters();
            return false;
        }

        TC_LOG_DEBUG("sql.driver", "[%u ms] SQL(p): %s", getMSTimeDiff(_s, getMSTime()), m_mStmt->getQueryString(m_queries[index].first).c_str());

        m_mStmt->ClearParameters();
        return true;
    }
}

bool MySQLConnection::ExecuteMultiSQL(const std::string& sql)
{
    if (!m_Mysql)
        return false;

    // Enable multi-statements
    if (mysql_set_server_option(m_Mysql, MYSQL_OPTION_MULTI_STATEMENTS_ON)) {
        TC_LOG_ERROR("MySQL", "Could not enable multi statements: {}", mysql_error(m_Mysql));
        return false;
    }

    // Execute SQL statements
    if (mysql_real_query(m_Mysql, sql.c_str(), sql.length())) {
        TC_LOG_ERROR("MySQL", "SQL query failed: {}", mysql_error(m_Mysql));
        mysql_set_server_option(m_Mysql, MYSQL_OPTION_MULTI_STATEMENTS_OFF);
        return false;
    }

    // Retrieve and free all result sets to keep the connection clean
    do {
        MYSQL_RES* result = mysql_store_result(m_Mysql);
        if (result)
            mysql_free_result(result);
    } while (!mysql_next_result(m_Mysql));

    // Disable multi-statements again
    mysql_set_server_option(m_Mysql, MYSQL_OPTION_MULTI_STATEMENTS_OFF);

    return true;
}

bool MySQLConnection::_Query(PreparedStatement* stmt, MYSQL_RES **pResult, uint64* pRowCount, uint32* pFieldCount)
{
    if (!m_Mysql)
        return false;

    uint32 index = stmt->m_index;
    {
        MySQLPreparedStatement* m_mStmt = GetPreparedStatement(index);
        ASSERT(m_mStmt);            // Can only be null if preparation failed, server side error or bad query
        m_mStmt->m_stmt = stmt;     // Cross reference them for debug output
        stmt->m_stmt = m_mStmt;     /// @todo Cleaner way

        stmt->BindParameters();

        MYSQL_STMT* msql_STMT = m_mStmt->GetSTMT();
        MYSQL_BIND* msql_BIND = m_mStmt->GetBind();

        uint32 _s = getMSTime();

        if (mysql_stmt_bind_param(msql_STMT, msql_BIND))
        {
            uint32 lErrno = mysql_errno(m_Mysql);
            TC_LOG_ERROR("sql.driver", "SQL(p): %s\n [ERROR]: [%u] %s", m_mStmt->getQueryString(m_queries[index].first).c_str(), lErrno, mysql_stmt_error(msql_STMT));

            if (_HandleMySQLErrno(lErrno))  // If it returns true, an error was handled successfully (i.e. reconnection)
                return _Query(stmt, pResult, pRowCount, pFieldCount);       // Try again

            m_mStmt->ClearParameters();
            return false;
        }

        if (mysql_stmt_execute(msql_STMT))
        {
            uint32 lErrno = mysql_errno(m_Mysql);
            TC_LOG_ERROR("sql.driver", "SQL(p): %s\n [ERROR]: [%u] %s",
                m_mStmt->getQueryString(m_queries[index].first).c_str(), lErrno, mysql_stmt_error(msql_STMT));

            if (_HandleMySQLErrno(lErrno))  // If it returns true, an error was handled successfully (i.e. reconnection)
                return _Query(stmt, pResult, pRowCount, pFieldCount);      // Try again

            m_mStmt->ClearParameters();
            return false;
        }

        TC_LOG_DEBUG("sql.driver", "[%u ms] SQL(p): %s", getMSTimeDiff(_s, getMSTime()), m_mStmt->getQueryString(m_queries[index].first).c_str());

        m_mStmt->ClearParameters();

        *pResult = mysql_stmt_result_metadata(msql_STMT);
        *pRowCount = mysql_stmt_num_rows(msql_STMT);
        *pFieldCount = mysql_stmt_field_count(msql_STMT);

        return true;

    }
}

ResultSet* MySQLConnection::Query(const char* sql)
{
    if (!sql)
        return NULL;

    MYSQL_RES *result = NULL;
    MYSQL_FIELD *fields = NULL;
    uint64 rowCount = 0;
    uint32 fieldCount = 0;

    if (!_Query(sql, &result, &fields, &rowCount, &fieldCount))
        return NULL;

    return new ResultSet(result, fields, rowCount, fieldCount);
}

bool MySQLConnection::_Query(const char *sql, MYSQL_RES **pResult, MYSQL_FIELD **pFields, uint64* pRowCount, uint32* pFieldCount)
{
    if (!m_Mysql)
        return false;

    {
        uint32 _s = getMSTime();

        if (mysql_query(m_Mysql, sql))
        {
            uint32 lErrno = mysql_errno(m_Mysql);
            TC_LOG_ERROR("sql.driver", "SQL: %s", sql);
            TC_LOG_ERROR("sql.driver", "[%u] %s", lErrno, mysql_error(m_Mysql));

            if (_HandleMySQLErrno(lErrno))      // If it returns true, an error was handled successfully (i.e. reconnection)
                return _Query(sql, pResult, pFields, pRowCount, pFieldCount);    // We try again

            return false;
        }
        else
            TC_LOG_DEBUG("sql.driver", "[%u ms] SQL: %s", getMSTimeDiff(_s, getMSTime()), sql);

        *pResult = mysql_store_result(m_Mysql);
        *pRowCount = mysql_affected_rows(m_Mysql);
        *pFieldCount = mysql_field_count(m_Mysql);
    }

    if (!*pResult )
        return false;

    if (!*pRowCount)
    {
        mysql_free_result(*pResult);
        return false;
    }

    *pFields = mysql_fetch_fields(*pResult);

    return true;
}

void MySQLConnection::BeginTransaction()
{
    Execute("START TRANSACTION");
}

void MySQLConnection::RollbackTransaction()
{
    Execute("ROLLBACK");
}

void MySQLConnection::CommitTransaction()
{
    Execute("COMMIT");
}

bool MySQLConnection::ExecuteTransaction(SQLTransaction& transaction)
{
    std::list<SQLElementData> const& queries = transaction->m_queries;
    if (queries.empty())
        return false;

    BeginTransaction();

    std::list<SQLElementData>::const_iterator itr;
    for (itr = queries.begin(); itr != queries.end(); ++itr)
    {
        SQLElementData const& data = *itr;
        switch (itr->type)
        {
            case SQL_ELEMENT_PREPARED:
            {
                PreparedStatement* stmt = data.element.stmt;
                ASSERT(stmt);
                if (!Execute(stmt))
                {
                    TC_LOG_ERROR("sql.driver", "Transaction aborted. %u queries not executed.", (uint32)queries.size());
                    RollbackTransaction();
                    return false;
                }
            }
            break;
            case SQL_ELEMENT_RAW:
            {
                const char* sql = data.element.query;
                ASSERT(sql);
                if (!Execute(sql))
                {
                    TC_LOG_ERROR("sql.driver", "Transaction aborted. %u queries not executed.", (uint32)queries.size());
                    RollbackTransaction();
                    return false;
                }
            }
            break;
        }
    }

    // we might encounter errors during certain queries, and depending on the kind of error
    // we might want to restart the transaction. So to prevent data loss, we only clean up when it's all done.
    // This is done in calling functions DatabaseWorkerPool<T>::DirectCommitTransaction and TransactionTask::Execute,
    // and not while iterating over every element.

    CommitTransaction();
    return true;
}

MySQLPreparedStatement* MySQLConnection::GetPreparedStatement(uint32 index)
{
    ASSERT(index < m_stmts.size());
    MySQLPreparedStatement* ret = m_stmts[index];
    if (!ret)
        TC_LOG_ERROR("sql.driver", "Could not fetch prepared statement %u on database `%s`, connection type: %s.",
            index, m_connectionInfo.database.c_str(), (m_connectionFlags & CONNECTION_ASYNC) ? "asynchronous" : "synchronous");

    return ret;
}

void MySQLConnection::PrepareStatement(uint32 index, const char* sql, ConnectionFlags flags)
{
    m_queries.insert(PreparedStatementMap::value_type(index, std::make_pair(sql, flags)));

    // For reconnection case
    if (m_reconnecting)
        delete m_stmts[index];

    // Check if specified query should be prepared on this connection
    // i.e. don't prepare async statements on synchronous connections
    // to save memory that will not be used.
    if (!(m_connectionFlags & flags))
    {
        m_stmts[index] = NULL;
        return;
    }

    MYSQL_STMT* stmt = mysql_stmt_init(m_Mysql);
    if (!stmt)
    {
        TC_LOG_ERROR("sql.driver", "In mysql_stmt_init() id: %u, sql: \"%s\"", index, sql);
        TC_LOG_ERROR("sql.driver", "%s", mysql_error(m_Mysql));
        m_prepareError = true;
    }
    else
    {
        if (mysql_stmt_prepare(stmt, sql, static_cast<unsigned long>(strlen(sql))))
        {
            TC_LOG_ERROR("sql.driver", "In mysql_stmt_prepare() id: %u, sql: \"%s\"", index, sql);
            TC_LOG_ERROR("sql.driver", "%s", mysql_stmt_error(stmt));
            mysql_stmt_close(stmt);
            m_prepareError = true;
        }
        else
        {
            MySQLPreparedStatement* mStmt = new MySQLPreparedStatement(stmt);
            m_stmts[index] = mStmt;
        }
    }
}

PreparedResultSet* MySQLConnection::Query(PreparedStatement* stmt)
{
    MYSQL_RES *result = NULL;
    uint64 rowCount = 0;
    uint32 fieldCount = 0;

    if (!_Query(stmt, &result, &rowCount, &fieldCount))
        return NULL;

    if (mysql_more_results(m_Mysql))
    {
        mysql_next_result(m_Mysql);
    }
    return new PreparedResultSet(stmt->m_stmt->GetSTMT(), result, rowCount, fieldCount);
}

bool MySQLConnection::_HandleMySQLErrno(uint32 errNo)
{
    switch (errNo)
    {
        case CR_SERVER_GONE_ERROR:
        case CR_SERVER_LOST:
        case CR_INVALID_CONN_HANDLE:
        case CR_SERVER_LOST_EXTENDED:
        {
            m_reconnecting = true;
            uint64 oldThreadId = mysql_thread_id(GetHandle());
            mysql_close(GetHandle());
            if (this->Open())                           // Don't remove 'this' pointer unless you want to skip loading all prepared statements....
            {
                TC_LOG_ERROR("sql.driver", "Connection to the MySQL server is active.");
                if (oldThreadId != mysql_thread_id(GetHandle()))
                    TC_LOG_ERROR("sql.driver", "Successfully reconnected to %s @%s:%s (%s).",
                        m_connectionInfo.database.c_str(), m_connectionInfo.host.c_str(), m_connectionInfo.port_or_socket.c_str(),
                            (m_connectionFlags & CONNECTION_ASYNC) ? "asynchronous" : "synchronous");

                m_reconnecting = false;
                return true;
            }

            uint32 lErrno = mysql_errno(GetHandle());   // It's possible this attempted reconnect throws 2006 at us. To prevent crazy recursive calls, sleep here.
            ACE_OS::sleep(3);                           // Sleep 3 seconds
            return _HandleMySQLErrno(lErrno);           // Call self (recursive)
        }

        case ER_LOCK_DEADLOCK:
            return false;    // Implemented in TransactionTask::Execute and DatabaseWorkerPool<T>::DirectCommitTransaction
        // Query related errors - skip query
        case ER_WRONG_VALUE_COUNT:
        case ER_DUP_ENTRY:
            return false;

        // Outdated table or database structure - terminate core
        case ER_BAD_FIELD_ERROR:
        case ER_NO_SUCH_TABLE:
            TC_LOG_ERROR("sql.driver", "Your database structure is not up to date. Please make sure you've executed all queries in the sql/updates folders.");
            ACE_OS::sleep(10);
            std::abort();
            return false;
        case ER_PARSE_ERROR:
            TC_LOG_ERROR("sql.driver", "Error while parsing SQL. Core fix required.");
            ACE_OS::sleep(10);
            std::abort();
            return false;
        default:
            TC_LOG_ERROR("sql.driver", "Unhandled MySQL errno %u. Unexpected behaviour possible.", errNo);
            return false;
    }
}
