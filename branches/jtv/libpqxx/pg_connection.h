/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pg_connection.h
 *
 *   DESCRIPTION
 *      definition of the Pg::Connection class.
 *   Pg::Connection encapsulates a frontend to backend connection
 *
 * Copyright (c) 2001, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_CONNECTION_H
#define PG_CONNECTION_H

#include <map>
#include <stdexcept>

#include "pg_transactor.h"
#include "pg_util.h"


// TODO: Teach Transactor to handle multiple connections--quasi "2-phase commit"


namespace Pg
{
class Result;	// See pg_result.h
class Trigger;	// See pg_trigger.h

extern "C" { typedef void (*NoticeProcessor)(void *arg, const char *msg); }


template<> inline PGSTD::string Classname(const Transaction *) 
{ 
  return "Transaction"; 
}


// Exception class for lost backend connection
// (May be changed once I find a standard exception class for this)
class broken_connection : public PGSTD::runtime_error
{
public:
  broken_connection() : PGSTD::runtime_error("Connection to back end failed") {}
  explicit broken_connection(const PGSTD::string &whatarg) : 
    PGSTD::runtime_error(whatarg) {}
};


// Connection class.  This is the first class to look at when you wish to work
// with a database through libpqxx.  It is automatically opened by its
// constructor, and automatically closed upon destruction, if it hasn't already
// been closed manually.
// To query or manipulate the database once connected, use the Transaction
// class (see pg_transaction.h) or preferably the Transactor framework (see 
// pg_transactor.h).
class Connection
{
public:
  explicit Connection(const PGSTD::string &ConnInfo);
  ~Connection();

  void Disconnect() throw ();

  bool IsOpen() const;

  template<typename TRANSACTOR> void Perform(const TRANSACTOR &, int Attempts=3);

  // Set callback method for postgresql status output; return value is previous
  // handler.  Passing a NULL callback pointer simply returns the existing
  // callback.  The callback must have C linkage.
  NoticeProcessor SetNoticeProcessor(NoticeProcessor, void *arg);

  // Invoke notice processor function.  The message should end in newline.
  void ProcessNotice(const char[]) throw ();
  void ProcessNotice(PGSTD::string msg) throw () { ProcessNotice(msg.c_str()); }

  // Enable/disable tracing to a given output stream
  void Trace(FILE *);
  void Untrace();


  // Check for pending trigger notifications and take appropriate action
  void GetNotifs();

  // Miscellaneous query functions (probably not needed very often)
  const char *DbName() const throw () { return m_Conn ? PQdb(m_Conn) : ""; }
  const char *UserName() const throw () { return m_Conn ? PQuser(m_Conn):""; }
  const char *HostName() const throw () { return m_Conn ? PQhost(m_Conn):""; }
  const char *Port() const throw () { return m_Conn ? PQport(m_Conn) : ""; }
  const char *Options() const throw () { return m_ConnInfo.c_str(); }
  int BackendPID() const;

private:
  void Connect();
  int Status() const { return PQstatus(m_Conn); }
  const char *ErrMsg() const;
  void Reset(const char *OnReconnect=0);

  PGSTD::string m_ConnInfo;	// Connection string
  PGconn *m_Conn;		// Connection handle
  Unique<Transaction> m_Trans;	// Active transaction on connection, if any
  void *m_NoticeProcessorArg;	// Client-set argument to notice processor func

  // TODO: Use multimap when gcc supports them!
  typedef PGSTD::map<PGSTD::string, Pg::Trigger *> TriggerList;
  TriggerList m_Triggers;

  friend class Transaction;
  Result Exec(const char[], int Retries=3, const char OnReconnect[]=0);
  void RegisterTransaction(const Transaction *);
  void UnregisterTransaction(const Transaction *) throw ();
  void MakeEmpty(Result &, ExecStatusType=PGRES_EMPTY_QUERY);
  void BeginCopyRead(PGSTD::string Table);
  bool ReadCopyLine(PGSTD::string &);
  void BeginCopyWrite(PGSTD::string Table);
  void WriteCopyLine(PGSTD::string);
  void EndCopy();

  friend class Trigger;
  void AddTrigger(Trigger *);
  void RemoveTrigger(const Trigger *) throw ();

  // Not allowed:
  Connection(const Connection &);
  Connection &operator=(const Connection &);
};



// Invoke a Transactor, making at most Attempts attempts to perform the
// encapsulated code on the database.  If the code throws any exception other
// than broken_connection, it will be aborted right away.
// Take care: neither OnAbort() nor OnCommit() will be invoked on the original
// transactor you pass into the function.  It only serves as a prototype for
// the transaction to be performed.  In fact, this function may copy-construct
// any number of Transactors from the one you passed in, calling either 
// OnCommit() or OnAbort() only on those that actually have their operator()
// invoked.
template<typename TRANSACTOR> 
inline void Connection::Perform(const TRANSACTOR &T,
                                int Attempts)
{
  if (Attempts <= 0) return;

  bool Done = false;

  // Make attempts to perform T
  // TODO: Differentiate between db-related exceptions and other exceptions?
  do
  {
    --Attempts;

    // Work on a copy of T2 so we can restore the starting situation if need be
    TRANSACTOR T2(T);
    try
    {
      Transaction X(*this, T2.Name());
      T2(X);
      X.Commit();
      Done = true;
    }
    catch (const PGSTD::exception &e)
    {
      // Could be any kind of error.  
      T2.OnAbort(e.what());
      if (Attempts <= 0) throw;
      continue;
    }
    catch (...)
    {
      // This shouldn't happen!  If it does, don't try to forge ahead
      T2.OnAbort("Unknown exception");
      throw;
    }

    // Commit has to happen inside loop where T2 is still in scope.
    if (!Done) 
      throw PGSTD::logic_error("Internal libpqxx error: Pg::Connection: "
		             "broken Perform() loop");

    T2.OnCommit();
  } while (!Done);
}


}

#endif

