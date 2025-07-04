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

#ifndef DATABASEWORKERPOOL_H
#define DATABASEWORKERPOOL_H

#include <ace/Thread_Mutex.h>

#include "Common.h"
#include "Callback.h"
#include "MySQLConnection.h"
#include "Transaction.h"
#include "DatabaseWorker.h"
#include "PreparedStatement.h"
#include "Log.h"
#include "QueryResult.h"
#include "QueryHolder.h"
#include "AdhocStatement.h"
#include "Task.h"

#define MIN_MYSQL_SERVER_VERSION 50100u
#define MIN_MYSQL_CLIENT_VERSION 50100u

class PingOperation : public SQLOperation
{
    //! Operation for idle delaythreads
    bool Execute()
    {
        m_conn->Ping();
        return true;
    }
};

enum class DBConnection
{
    Any     = -1,
    Guild,
    Group,
    Instances,
    Player,
};

template <class T>
class DatabaseWorkerPool
{
    public:
        /* Activity state */
        DatabaseWorkerPool()
        {
            WPFatal(mysql_thread_safe(), "Used MySQL library isn't thread-safe.");
            WPFatal(mysql_get_client_version() >= MIN_MYSQL_CLIENT_VERSION, "TrinityCore does not support MySQL versions below 5.1");
        }

        using ConnectionType = T;

        ~DatabaseWorkerPool()
        {
        }

        bool Open(const std::string& infoString, uint8 async_threads, uint8 synch_threads)
        {
            bool res = true;
            _connectionInfo = MySQLConnectionInfo(infoString);

            TC_LOG_INFO("sql.driver", "Opening DatabasePool '%s'. Asynchronous connections: %u, synchronous connections: %u.",
                GetDatabaseName(), async_threads, synch_threads);

            //! Open asynchronous connections (delayed operations)
            for (uint8 i = 0; i < async_threads; ++i)
            {
                T* t = new T(_connectionInfo, CONNECTION_ASYNC);
                res &= t->Open();
                _workers.push_back(new DatabaseWorker(t));
            }

            //! Open synchronous connections (direct, blocking operations)
            for (uint8 i = 0; i < synch_threads; ++i)
            {
                T* t = new T(_connectionInfo, CONNECTION_SYNCH);
                res &= t->Open();
                _connections.push_back(t);
            }

            if (res)
                TC_LOG_INFO("sql.driver", "DatabasePool '%s' opened successfully. %u total connections running.", GetDatabaseName(),
                    (_connections.size() + _workers.size()));
            else
                TC_LOG_ERROR("sql.driver", "DatabasePool %s NOT opened. There were errors opening the MySQL connections. Check your SQLDriverLogFile "
                    "for specific errors.", GetDatabaseName());
            return res;
        }

        void Close()
        {
            TC_LOG_INFO("sql.driver", "Closing down DatabasePool '%s'.", GetDatabaseName());

            //! Shuts down delaythreads for this connection pool by underlying deactivate().
            //! The next dequeue attempt in the worker thread tasks will result in an error,
            //! ultimately ending the worker thread task.
            for (auto&& worker : _workers)
                delete worker;

            TC_LOG_INFO("sql.driver", "Asynchronous connections on DatabasePool '%s' terminated. Proceeding with synchronous connections.",
                GetDatabaseName());

            //! Shut down the synchronous connections
            //! There's no need for locking the connection, because DatabaseWorkerPool<>::Close
            //! should only be called after any other thread tasks in the core have exited,
            //! meaning there can be no concurrent access at this point.
            for (auto&& conn : _connections)
                delete conn;

            _connections.clear();

            TC_LOG_INFO("sql.driver", "All connections on DatabasePool '%s' closed.", GetDatabaseName());
        }

        /**
            Delayed one-way statement methods.
        */

        //! Enqueues a one-way SQL operation in string format that will be executed asynchronously.
        //! This method should only be used for queries that are only executed once, e.g during startup.
        void Execute(const char* sql)
        {
            if (!sql)
                return;

            BasicStatementTask* task = new BasicStatementTask(sql);
            Enqueue(task);
        }

        //! Enqueues a one-way SQL operation in string format -with variable args- that will be executed asynchronously.
        //! This method should only be used for queries that are only executed once, e.g during startup.
        void PExecute(const char* sql, ...)
        {
            if (!sql)
                return;

            va_list ap;
            char szQuery[MAX_QUERY_LEN];
            va_start(ap, sql);
            vsnprintf(szQuery, MAX_QUERY_LEN, sql, ap);
            va_end(ap);

            Execute(szQuery);
        }

        //! Enqueues a one-way SQL operation in prepared statement format that will be executed asynchronously.
        //! Statement must be prepared with CONNECTION_ASYNC flag.
        void Execute(PreparedStatement* stmt, DBConnection connIndex = DBConnection::Any)
        {
            PreparedStatementTask* task = new PreparedStatementTask(stmt);
            Enqueue(task, connIndex);
        }

        /**
            Direct synchronous one-way statement methods.
        */

        //! Directly executes a one-way SQL operation in string format, that will block the calling thread until finished.
        //! This method should only be used for queries that are only executed once, e.g during startup.
        void DirectExecute(const char* sql)
        {
            if (!sql)
                return;

            T* t = GetFreeConnection();
            t->Execute(sql);
            t->Unlock();
        }

        //! Directly executes a one-way SQL operation in string format -with variable args-, that will block the calling thread until finished.
        //! This method should only be used for queries that are only executed once, e.g during startup.
        void DirectPExecute(const char* sql, ...)
        {
            if (!sql)
                return;

            va_list ap;
            char szQuery[MAX_QUERY_LEN];
            va_start(ap, sql);
            vsnprintf(szQuery, MAX_QUERY_LEN, sql, ap);
            va_end(ap);

            return DirectExecute(szQuery);
        }

        //! Directly executes a one-way SQL operation in prepared statement format, that will block the calling thread until finished.
        //! Statement must be prepared with the CONNECTION_SYNCH flag.
        void DirectExecute(PreparedStatement* stmt)
        {
            T* t = GetFreeConnection();
            t->Execute(stmt);
            t->Unlock();

            //! Delete proxy-class. Not needed anymore
            delete stmt;
        }

        /**
            Synchronous query (with resultset) methods.
        */

        //! Directly executes an SQL query in string format that will block the calling thread until finished.
        //! Returns reference counted auto pointer, no need for manual memory management in upper level code.
        QueryResult Query(const char* sql, T* conn = NULL)
        {
            if (!conn)
                conn = GetFreeConnection();

            ResultSet* result = conn->Query(sql);
            conn->Unlock();
            if (!result || !result->GetRowCount())
            {
                delete result;
                return QueryResult(NULL);
            }

            result->NextRow();
            return QueryResult(result);
        }

        //! Directly executes an SQL query in string format -with variable args- that will block the calling thread until finished.
        //! Returns reference counted auto pointer, no need for manual memory management in upper level code.
        QueryResult PQuery(const char* sql, T* conn, ...)
        {
            if (!sql)
                return QueryResult(NULL);

            va_list ap;
            char szQuery[MAX_QUERY_LEN];
            va_start(ap, conn);
            vsnprintf(szQuery, MAX_QUERY_LEN, sql, ap);
            va_end(ap);

            return Query(szQuery, conn);
        }

        //! Directly executes an SQL query in string format -with variable args- that will block the calling thread until finished.
        //! Returns reference counted auto pointer, no need for manual memory management in upper level code.
        QueryResult PQuery(const char* sql, ...)
        {
            if (!sql)
                return QueryResult(NULL);

            va_list ap;
            char szQuery[MAX_QUERY_LEN];
            va_start(ap, sql);
            vsnprintf(szQuery, MAX_QUERY_LEN, sql, ap);
            va_end(ap);

            return Query(szQuery);
        }

        //! Directly executes an SQL query in prepared format that will block the calling thread until finished.
        //! Returns reference counted auto pointer, no need for manual memory management in upper level code.
        //! Statement must be prepared with CONNECTION_SYNCH flag.
        PreparedQueryResult Query(PreparedStatement* stmt)
        {
            T* t = GetFreeConnection();
            PreparedResultSet* ret = t->Query(stmt);
            t->Unlock();

            //! Delete proxy-class. Not needed anymore
            delete stmt;

            if (!ret || !ret->GetRowCount())
            {
                delete ret;
                return PreparedQueryResult(NULL);
            }

            return PreparedQueryResult(ret);
        }

        /**
            Asynchronous query (with resultset) methods.
        */

        //! Enqueues a query in string format that will set the value of the QueryResultFuture return object as soon as the query is executed.
        //! The return value is then processed in ProcessQueryCallback methods.
        QueryResultFuture AsyncQuery(const char* sql)
        {
            QueryResultFuture res;
            BasicStatementTask* task = new BasicStatementTask(sql, res);
            Enqueue(task);
            return res;         //! Actual return value has no use yet
        }

        //! Enqueues a query in string format -with variable args- that will set the value of the QueryResultFuture return object as soon as the query is executed.
        //! The return value is then processed in ProcessQueryCallback methods.
        QueryResultFuture AsyncPQuery(const char* sql, ...)
        {
            va_list ap;
            char szQuery[MAX_QUERY_LEN];
            va_start(ap, sql);
            vsnprintf(szQuery, MAX_QUERY_LEN, sql, ap);
            va_end(ap);

            return AsyncQuery(szQuery);
        }

        //! Enqueues a query in prepared format that will set the value of the PreparedQueryResultFuture return object as soon as the query is executed.
        //! The return value is then processed in ProcessQueryCallback methods.
        //! Statement must be prepared with CONNECTION_ASYNC flag.
        PreparedQueryResultFuture AsyncQuery(PreparedStatement* stmt)
        {
            PreparedQueryResultFuture res;
            PreparedStatementTask* task = new PreparedStatementTask(stmt, res);
            Enqueue(task);
            return res;
        }

        //! Enqueues a vector of SQL operations (can be both adhoc and prepared) that will set the value of the QueryResultHolderFuture
        //! return object as soon as the query is executed.
        //! The return value is then processed in ProcessQueryCallback methods.
        //! Any prepared statements added to this holder need to be prepared with the CONNECTION_ASYNC flag.
        QueryResultHolderFuture DelayQueryHolder(SQLQueryHolder* holder)
        {
            QueryResultHolderFuture res;
            SQLQueryHolderTask* task = new SQLQueryHolderTask(holder, res);
            Enqueue(task);
            return res;     //! Fool compiler, has no use yet
        }

        /**
            Transaction context methods.
        */

        //! Begins an automanaged transaction pointer that will automatically rollback if not commited. (Autocommit=0)
        SQLTransaction BeginTransaction()
        {
            return SQLTransaction(new Transaction);
        }

        //! Enqueues a collection of one-way SQL operations (can be both adhoc and prepared). The order in which these operations
        //! were appended to the transaction will be respected during execution.
        void CommitTransaction(SQLTransaction transaction, DBConnection connIndex = DBConnection::Any)
        {
            #ifdef TRINITY_DEBUG
            //! Only analyze transaction weaknesses in Debug mode.
            //! Ideally we catch the faults in Debug mode and then correct them,
            //! so there's no need to waste these CPU cycles in Release mode.
            switch (transaction->GetSize())
            {
                case 0:
                    TC_LOG_DEBUG("sql.driver", "Transaction contains 0 queries. Not executing.");
                    return;
                case 1:
                    TC_LOG_DEBUG("sql.driver", "Warning: Transaction only holds 1 query, consider removing Transaction context in code.");
                    break;
                default:
                    break;
            }
            #endif // TRINITY_DEBUG

            Enqueue(new TransactionTask(std::move(transaction)), connIndex);
        }

        //! Directly executes a collection of one-way SQL operations (can be both adhoc and prepared). The order in which these operations
        //! were appended to the transaction will be respected during execution.
        void DirectCommitTransaction(SQLTransaction& transaction)
        {
            T* con = GetFreeConnection();
            if (con->ExecuteTransaction(transaction))
            {
                con->Unlock();      // OK, operation succesful
                return;
            }

            //! Handle MySQL Errno 1213 without extending deadlock to the core itself
            /// @todo More elegant way
            if (con->GetLastError() == 1213)
            {
                uint8 loopBreaker = 5;
                for (uint8 i = 0; i < loopBreaker; ++i)
                {
                    if (con->ExecuteTransaction(transaction))
                        break;
                }
            }

            //! Clean up now.
            transaction->Cleanup();

            con->Unlock();
        }

        //! Method used to execute prepared statements in a diverse context.
        //! Will be wrapped in a transaction if valid object is present, otherwise executed standalone.
        void ExecuteOrAppend(SQLTransaction& trans, PreparedStatement* stmt, DBConnection connIndex = DBConnection::Any)
        {
            if (!trans)
                Execute(stmt, connIndex);
            else
                trans->Append(stmt);
        }

        //! Method used to execute ad-hoc statements in a diverse context.
        //! Will be wrapped in a transaction if valid object is present, otherwise executed standalone.
        void ExecuteOrAppend(SQLTransaction& trans, const char* sql)
        {
            if (!trans)
                Execute(sql);
            else
                trans->Append(sql);
        }

        /**
            Other
        */

        //! Automanaged (internally) pointer to a prepared statement object for usage in upper level code.
        //! Pointer is deleted in this->DirectExecute(PreparedStatement*), this->Query(PreparedStatement*) or PreparedStatementTask::~PreparedStatementTask.
        //! This object is not tied to the prepared statement on the MySQL context yet until execution.
        PreparedStatement* GetPreparedStatement(uint32 index)
        {
            return new PreparedStatement(index);
        }

        //! Apply escape string'ing for current collation. (utf8)
        void EscapeString(std::string& str)
        {
            if (str.empty())
                return;

            char* buf = new char[str.size()*2+1];
            EscapeString(buf, str.c_str(), str.size());
            str = buf;
            delete[] buf;
        }

        //! Keeps all our MySQL connections alive, prevent the server from disconnecting us.
        void KeepAlive()
        {
            //! Ping synchronous connections
            for (auto&& conn : _connections)
            {
                if (conn->LockIfReady())
                {
                    conn->Ping();
                    conn->Unlock();
                }
            }

            //! Assuming all worker threads are free, every worker thread will receive 1 ping operation request
            //! If one or more worker threads are busy, the ping operations will not be split evenly, but this doesn't matter
            //! as the sole purpose is to prevent connections from idling.
            for (auto&& worker : _workers)
                worker->Enqueue(new PingOperation());
        }

        Task<QueryResult>* Async(char const* sql, ...)
        {
            va_list ap;
            char szQuery[MAX_QUERY_LEN];
            va_start(ap, sql);
            vsnprintf(szQuery, MAX_QUERY_LEN, sql, ap);
            va_end(ap);

            QueryResultFuture res;
            WaitResultTask<QueryResult>* wait = new WaitResultTask<QueryResult>(res);
            Enqueue(new BasicStatementTask(szQuery, res));
            return wait;
        }

        Task<PreparedQueryResult>* Async(PreparedStatement* stmt, DBConnection connIndex = DBConnection::Any)
        {
            PreparedQueryResultFuture res;
            WaitResultTask<PreparedQueryResult>* wait = new WaitResultTask<PreparedQueryResult>(res);
            Enqueue(new PreparedStatementTask(stmt, res), connIndex);
            return wait;
        }

        Task<SQLQueryHolder*>* Async(SQLQueryHolder* holder)
        {
            QueryResultHolderFuture res;
            WaitResultTask<SQLQueryHolder*>* wait = new WaitResultTask<SQLQueryHolder*>(res);
            Enqueue(new SQLQueryHolderTask(holder, res));
            return wait;
        }

        Task<bool>* Async(SQLTransaction& trans, DBConnection connIndex = DBConnection::Any)
        {
            Task<bool>* next = new Task<bool>();
            Enqueue(new TransactionTask(std::move(trans), next), connIndex);
            return next;
        }

        Task<bool>* SafeAsync(SQLTransaction& trans)
        {
            Task<bool>* next = new Task<bool>();
            Enqueue(new SafeTransactionTask(std::move(trans), next));
            return next;
        }

        T* GetConnection();

        bool ExecuteMultiSQL(const std::string& sql)
        {
            T* conn = GetConnection();
            if (!conn)
                return false;

            return conn->ExecuteMultiSQL(sql);
        }

    private:
        unsigned long EscapeString(char *to, const char *from, unsigned long length)
        {
            if (!to || !from || !length)
                return 0;

            return mysql_real_escape_string(_connections[0]->GetHandle(), to, from, length);
        }

        void Enqueue(SQLOperation* op, DBConnection connIndex = DBConnection::Any)
        {
            int32 i = int32(connIndex);
            if (i < 0)
                GetWorker()->Enqueue(op);
            else
            {
                int32 n = _workers.size() - 1 - i;
                if (n < 0)  // Not enouth threads. Use the last one
                    _workers.back()->Enqueue(op);
                else
                    _workers[n]->Enqueue(op);
            }
        }

        //! Gets a free connection in the synchronous connection pool.
        //! Caller MUST call t->Unlock() after touching the MySQL context to prevent deadlocks.
        T* GetFreeConnection()
        {
            while (true)
            {
                //! Block forever until a connection is free
                for (auto&& conn : _connections)
                {
                    //! Must be matched with t->Unlock() or you will get deadlocks
                    if (conn->LockIfReady())
                        return conn;
                }
            }

            std::abort();
        }

        DatabaseWorker* GetWorker()
        {
            int32 min = 0;
            DatabaseWorker* worker = _workers.front();
            for (auto&& iter : _workers)
            {
                int32 c = iter->Count();
                if (!c)
                    return iter;

                if (c < min)
                {
                    min = c;
                    worker = iter;
                }
            }
            return worker;
        }

        char const* GetDatabaseName() const
        {
            return _connectionInfo.database.c_str();
        }

    private:
        enum _internalIndex
        {
            IDX_ASYNC,
            IDX_SYNCH,
            IDX_SIZE
        };

        std::vector<T*>                 _connections;
        std::vector<DatabaseWorker*>    _workers;
        MySQLConnectionInfo             _connectionInfo;
        std::vector<std::unique_ptr<T>> connections;
};

#endif
