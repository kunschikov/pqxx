/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pqxx/connection_base.hxx
 *
 *   DESCRIPTION
 *      definition of the pqxx::connection_base abstract base class.
 *   pqxx::connection_base encapsulates a frontend to backend connection
 *   DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/connection_base instead.
 *
 * Copyright (c) 2001-2003, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */

#include <map>
#include <memory>

#include "pqxx/except"
#include "pqxx/util"


/* Use of the libpqxx library starts here.
 *
 * Everything that can be done with a database through libpqxx must go through
 * a connection object derived from connection_base.
 */

/* Methods tested in eg. self-test program test1 are marked with "//[t1]"
 */

namespace pqxx
{
class result;
class transaction_base;
class trigger;

/// Base class for user-definable error/warning message processor
/** To define a custom method of handling notices, derive a new class from
 * noticer and override the virtual function "operator()(const char[]) throw()"
 * to process the message passed to it.
 */
struct PQXX_LIBEXPORT noticer : PGSTD::unary_function<const char[], void>
{
  virtual ~noticer() {}  
  virtual void operator()(const char Msg[]) throw () =0;
};


/// Human-readable class names for use by unique template.
template<> inline PGSTD::string Classname(const transaction_base *) 
{ 
  return "transaction_base"; 
}


/// connection_base abstract base class; represents a connection to a database.
/** This is the first class to look at when you wish to work with a database 
 * through libpqxx.  Depending on the implementing concrete child class, a
 * connection can be automatically opened when it is constructed, or when it is
 * first used.  The connection is automatically closed upon destruction, if it 
 * hasn't already been closed manually.
 * To query or manipulate the database once connected, use one of the 
 * transaction classes (see pqxx/transaction_base.h) or preferably the 
 * transactor framework (see pqxx/transactor.h).
 * A word of caution: if a network connection to the database server fails, the
 * connection will be restored automatically (although any transaction going on
 * at the time will have to be aborted).  This also means that any information
 * set in previous transactions that is not stored in the database, such as
 * connection-local variables defined with PostgreSQL's SET command, will be
 * lost.  Whenever you need to create such state, either do it within each
 * transaction that may need it, or use specialized functions made available by
 * libpqxx.  Always avoid raw queries if libpqxx offers a dedicated function for
 * the same purpose.
 */
class PQXX_LIBEXPORT connection_base
{
public:
  /// Set up connection based on PostgreSQL connection string
  /** @param ConnInfo a PostgreSQL connection string specifying any required
   * parameters, such as server, port, database, and password.  These values
   * override any of the environment variables recognized by libpq that may have
   * been defined for the same parameters.
   */
  explicit connection_base(const PGSTD::string &ConnInfo);		//[t2]

  /// Set up connection based on PostgreSQL connection string
  /** @param ConnInfo a PostgreSQL connection string specifying any required
   * parameters, such as server, port, database, and password.  As a special
   * case, a null pointer is taken as the empty string.
   */
  explicit connection_base(const char ConnInfo[]);			//[t2]

  /// Destructor.  Implicitly closes the connection.
  virtual ~connection_base() =0;					//[t1]

#ifdef PQXX_DEPRECATED_HEADERS
  /// @deprecated Use disconnect() instead
  void Disconnect() throw () { disconnect(); }
#endif

  /// Explicitly close connection.
  void disconnect() throw ();						//[t2]

  /// Is this connection open?
  bool is_open() const;							//[t1]

#ifdef PQXX_DEPRECATED_HEADERS
  /// @deprecated Use perform() instead
  template<typename TRANSACTOR> void Perform(const TRANSACTOR &T, int A=3)
	{ return perform(T,A); }
#endif

  /// Perform the transaction defined by a transactor-based object.
  /** The function may create and execute several copies of the transactor
   * before it succeeds.  If there is any doubt over whether it succeeded
   * (this can happen if the connection is lost just before the backend can
   * confirm success), it is no longer retried and an error message is
   * generated.
   * @param T the transactor to be executed.
   * @param Attempts the maximum number of attempts to be made to execute T.
   */
  template<typename TRANSACTOR> 
  void perform(const TRANSACTOR &T, int Attempts=3);			//[t4]

#ifdef PQXX_DEPRECATED_HEADERS
  /// @deprecated Use set_noticer() instead
  PGSTD::auto_ptr<noticer> SetNoticer(PGSTD::auto_ptr<noticer> N)
  	{ return set_noticer(N); }
  /// @deprecated Use get_noticer() instead
  noticer *GetNoticer() const throw () 
  	{ return get_noticer(); }
  /// @deprecated Use process_notice() instead
  void ProcessNotice(const char msg[]) throw () { return process_notice(msg); }
  /// @deprecated Use process_notice() instead
  void ProcessNotice(const PGSTD::string &msg) throw () 
  	{ return process_notice(msg); }
#endif

  // TODO: Define a default noticer (mainly to help out Windows users)
  /// Set handler for postgresql errors or warning messages.
  /** Return value is the previous handler.  Ownership of any previously set 
   * noticer is also passed to the caller, so unless it is stored in another 
   * auto_ptr, it will be deleted from the caller's context.  
   * This may be important when running under Windows, where a DLL cannot free 
   * memory allocated by the main program.
   * If a noticer exists when the connection_base is destructed, it will also be
   * deleted.
   * @param N the new message handler; must not be null or equal to the old one
   */
  PGSTD::auto_ptr<noticer> set_noticer(PGSTD::auto_ptr<noticer> N);	//[t14]
  noticer *get_noticer() const throw () { return m_Noticer.get(); }	//[t14]

  /// Invoke notice processor function.  The message should end in newline.
  void process_notice(const char[]) throw ();				//[t14]
  /// Invoke notice processor function.  The message should end in newline.
  void process_notice(const PGSTD::string &msg) throw () 		//[t14]
  	{ process_notice(msg.c_str()); }

#ifdef PQXX_DEPRECATED_HEADERS
  /// @deprecated Use trace() instead
  void Trace(FILE *F) { trace(F); }

  /// @deprecated Use get_notifs() instead
  void GetNotifs() { get_notifs(); }
#endif

  /// Enable tracing to a given output stream, or NULL to disable.
  void trace(FILE *);							//[t3]

  /// Check for pending trigger notifications and take appropriate action.
  void get_notifs();							//[t4]

  // Miscellaneous query functions (probably not needed very often)
 
#ifdef PQXX_DEPRECATED_HEADERS
  /// @deprecated Use dbname() instead
  const char *DbName() { return dbname(); }
  /// @deprecated Use username() instead
  const char *UserName() { return username(); }
  /// @deprecated Use hostname() instead
  const char *HostName() { return hostname(); }
  /// @deprecated Use port() instead
  const char *Port() { return port(); }
  /// @deprecated Use options() instead
  const char *Options() const throw () { return options(); }
  /// @deprecated Use backendpid() instead
  int BackendPID() const { return backendpid(); }
#endif

  /// Name of database we're connected to, if any.
  const char *dbname()							//[t1]
  	{ activate(); return PQdb(m_Conn); }

  /// Database user ID we're connected under, if any.
  const char *username()						//[t1]
  	{ activate(); return  PQuser(m_Conn); }

  /// Address of server (NULL for local connections).
  const char *hostname()						//[t1]
  	{ activate(); return PQhost(m_Conn); }

  /// Server port number we're connected to.
  const char *port()		 					//[t1]
  	{ activate(); return PQport(m_Conn); }

  /// Full connection string as used to set up this connection.
  const char *options() const throw () 					//[t1]
  	{ return m_ConnInfo.c_str(); }


  /// Process ID for backend process.
  /** Use with care: connections may be lost and automatically re-established
   * without your knowledge, in which case this process ID may no longer be
   * correct.  You may, however, assume that this number remains constant and
   * reliable within the span of a successful backend transaction.  If the
   * transaction fails, which may be due to a lost connection, then this number
   * will have become invalid at some point within the transaction.
   */
  int backendpid() const						//[t1]
    	{ return m_Conn ? PQbackendPID(m_Conn) : 0; }


#ifdef PQXX_DEPRECATED_HEADERS
  /// @deprecated Use activate() instead
  void Activate() { activate(); }
  /// @deprecated Use deactivate() instead
  void Deactivate() { deactivate(); }
  /// @deprecated Use set_client_encoding() instead
  void SetClientEncoding(const PGSTD::string &E) { set_client_encoding(E); }
  /// @deprecated Use set_variable() instead
  void SetVariable(const PGSTD::string &Var, const PGSTD::string &Val)
  	{ set_variable(Var, Val); }
#endif

  /// Explicitly activate deferred or deactivated connection.
  /** Use of this method is entirely optional.  Whenever a connection is used
   * while in a deferred or deactivated state, it will transparently try to
   * bring itself into an actiaveted state.  This function is best viewed as an
   * explicit hint to the connection that "if you're not in an active state, now
   * would be a good time to get into one."  Whether a connection is currently
   * in an active state or not makes no real difference to its functionality.
   * There is also no particular need to match calls to Activate() with calls to
   * Deactivate().  A good time to call Activate() might be just before you
   * first open a transaction on a lazy connection.
   */
  void activate() { if (!m_Conn) Connect(); }				//[t12]

  /// Explicitly deactivate connection.
  /** Like its counterpart Activate(), this method is entirely optional.  
   * Calling this function really only makes sense if you won't be using this
   * connection for a while and want to reduce the number of open connections on
   * the database server.
   * There is no particular need to match or pair calls to Deactivate() with
   * calls to Activate(), but calling Deactivate() during a transaction is an
   * error.
   */
  void deactivate();							//[t12]

  /// Set client-side character encoding
  /** Search the PostgreSQL documentation for "multibyte" or "character set
   * encodings" to find out more about the available encodings, how to extend
   * them, and how to use them.  Not all server-side encodings are compatible 
   * with all client-side encodings or vice versa.
   * @param Encoding name of the character set encoding to use
   */
  void set_client_encoding(const PGSTD::string &Encoding)		//[t7]
  	{ set_variable("CLIENT_ENCODING", Encoding); }

  /// Set session variable
  /** Set a session variable for this connection, using the SET command.  If the
   * connection to the database is lost and recovered, the last-set value will
   * be restored automatically.  See the PostgreSQL documentation for a list of
   * variables that can be set and their permissible values.
   * If a transaction is currently in progress, aborting that transaction will
   * normally discard the newly set value.  Known exceptions are nontransaction
   * (which doesn't start a real backend transaction) and PostgreSQL versions
   * prior to 7.3.
   * @param Var variable to set
   * @param Value value to set, which may be an identifier, a quote string, etc.
   */
  void set_variable(const PGSTD::string &Var, 
      		    const PGSTD::string &Value);			//[t60]

protected:
  /// To be used by implementation classes: really connect to database
  void Connect();

private:
  void SetupState();
  void InternalSetTrace();
  int Status() const { return PQstatus(m_Conn); }
  const char *ErrMsg() const;
  void Reset(const char OnReconnect[]=0);
  void close() throw ();
  void RestoreVars();

  /// Connection string
  PGSTD::string m_ConnInfo;
  /// Connection handle
  PGconn *m_Conn;
  /// Active transaction on connection, if any
  unique<transaction_base> m_Trans;

  /// User-defined notice processor, if any
  PGSTD::auto_ptr<noticer> m_Noticer;
  /// File to trace to, if any
  FILE *m_Trace;

  typedef PGSTD::multimap<PGSTD::string, pqxx::trigger *> TriggerList;
  /// Triggers this session is listening on
  TriggerList m_Triggers;

  /// Variables set in this session
  PGSTD::map<PGSTD::string, PGSTD::string> m_Vars;

  friend class transaction_base;
  result Exec(const char[], int Retries=3, const char OnReconnect[]=0);
  void RegisterTransaction(transaction_base *);
  void UnregisterTransaction(transaction_base *) throw ();
  void MakeEmpty(result &, ExecStatusType=PGRES_EMPTY_QUERY);
  void BeginCopyRead(const PGSTD::string &Table);
  bool ReadCopyLine(PGSTD::string &);
  void BeginCopyWrite(const PGSTD::string &Table);
  void WriteCopyLine(const PGSTD::string &);
  void EndCopy() throw ();
  void RawSetVar(const PGSTD::string &Var, const PGSTD::string &Value);
  void AddVariables(const PGSTD::map<PGSTD::string, PGSTD::string> &);

  friend class largeobject;
  PGconn *RawConnection() const { return m_Conn; }

  friend class trigger;
  void AddTrigger(trigger *);
  void RemoveTrigger(trigger *) throw ();

  // Not allowed:
  connection_base(const connection_base &);
  connection_base &operator=(const connection_base &);
};


}


// Put this here so on Windows, any noticer will be deleted in caller's context
inline pqxx::connection_base::~connection_base()
{
  close();
}


