#include <cerrno>
#include <cstring>
#include <iostream>

#include <pqxx/connection>
#include <pqxx/transaction>
#include <pqxx/transactor>
#include <pqxx/trigger>
#include <pqxx/result>


using namespace PGSTD;
using namespace pqxx;


// Example program for libpqxx.  Test waiting for notification with timeout.
//
// Usage: test079

namespace
{

// Sample implementation of trigger handler
class TestTrig : public trigger
{
  bool m_Done;

public:
  explicit TestTrig(connection_base &C, string Name) : 
    trigger(C, Name), m_Done(false) 
  {
  }

  virtual void operator()(int be_pid)
  {
    m_Done = true;
    if (be_pid != Conn().backendpid())
      throw logic_error("Expected notification from backend process " +
		        to_string(Conn().backendpid()) + 
			", but got one from " +
			to_string(be_pid));

    cout << "Received notification: " << name() << " pid=" << be_pid << endl;
  }

  bool Done() const { return m_Done; }
};


// A transactor to trigger our trigger handler
class Notify : public transactor<>
{
  string m_Trigger;

public:
  explicit Notify(string TrigName) : 
    transactor<>("Notifier"), m_Trigger(TrigName) { }

  void operator()(argument_type &T)
  {
    T.exec("NOTIFY " + m_Trigger);
  }

  void OnAbort(const char Reason[]) throw ()
  {
    try
    {
      cerr << "Notify failed!" << endl;
      if (Reason) cerr << "Reason: " << Reason << endl;
    }
    catch (const exception &)
    {
    }
  }
};

} // namespace

int main()
{
  try
  {
    const string TrigName = "mytrigger";
    connection C;
    cout << "Adding trigger..." << endl;
    TestTrig Trig(C, TrigName);

    // First see if the timeout really works: we're not expecting any notifs
    int notifs = C.await_notification(0, 1);
    if (notifs)
      throw logic_error("Got unexpected notification!");

    cout << "Sending notification..." << endl;
    C.perform(Notify(Trig.name()));

    for (int i=0; (i < 20) && !Trig.Done(); ++i)
    {
      if (notifs)
	throw logic_error("Got " + to_string(notifs) + " notification(s), "
	    "but no handler called!");
      cout << ".";
      notifs = C.await_notification(1,0);
    }
    cout << endl;

    if (!Trig.Done()) 
    {
      cout << "No notification received!" << endl;
      return 1;
    }
    if (notifs != 1)
      throw logic_error("Expected 1 notification, got " + to_string(notifs));
  }
  catch (const sql_error &e)
  {
    cerr << "SQL error: " << e.what() << endl
         << "Query was: '" << e.query() << "'" << endl;
    return 1;
  }
  catch (const exception &e)
  {
    // All exceptions thrown by libpqxx are derived from std::exception
    cerr << "Exception: " << e.what() << endl;
    return 2;
  }
  catch (...)
  {
    // This is really unexpected (see above)
    cerr << "Unhandled exception" << endl;
    return 100;
  }

  return 0;
}

