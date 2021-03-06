#include <iostream>
#include <stdexcept>

#include <pqxx/connection>
#include <pqxx/transaction>
#include <pqxx/pipeline>

using namespace PGSTD;
using namespace pqxx;

namespace
{
void TestPipeline(pipeline &P, int numqueries)
{
    const string Q("SELECT count(*) FROM pg_tables");
    const result Empty;
    if (!Empty.empty())
      throw logic_error("Default-constructed result is not empty");
    if (!Empty.query().empty())
      throw logic_error("Default-constructed result has query");

    P.retain();
    for (int i=numqueries; i; --i) P.insert(Q);
    P.resume();

    if (numqueries && P.empty())
      throw logic_error("Pipeline is inexplicably empty");

    int res = 0;
    result Prev;
    if (Prev != Empty)
      throw logic_error("Default-constructed results not equal");

    for (int i=numqueries; i; --i)
    {
      if (P.empty())
	throw logic_error("Got " + to_string(numqueries-i) + " "
	    "results from pipeline; expected " + to_string(numqueries));

      pair<pipeline::query_id, result> R = P.retrieve();
      if (R.second == Empty)
	throw logic_error("Got empty result");
      if ((Prev != Empty) && !(R.second == Prev))
	throw logic_error("Results to same query claim to be different");
      Prev = R.second;
      if (Prev != R.second)
	throw logic_error("Result equality does not hold after assignment");

      if (R.second.query() != Q)
        throw logic_error("Unexpected query in result: "
		"'" + R.second.query() + "'");

      if (res && (Prev[0][0].as<int>() != res))
	throw logic_error("Expected " + to_string(res) + " out of pipeline, "
	    "got " + Prev[0][0].c_str());
      res = Prev[0][0].as<int>();
    }

    if (!P.empty()) throw logic_error("Pipeline not empty after retrieval!");
}
} // namespace


// Test program for libpqxx.  Issue a query repeatedly through a pipeline, and
// compare results.  Use retain() and resume() for performance.
//
// Usage: test070 [connect-string]
//
// Where connect-string is a set of connection options in Postgresql's
// PQconnectdb() format, eg. "dbname=template1" to select from a database
// called template1, or "host=foo.bar.net user=smith" to connect to a backend
// running on host foo.bar.net, logging in as user smith.
int main(int, char *argv[])
{
  try
  {
    asyncconnection C(argv[1]);
    work W(C, "test70");
    pipeline P(W);

    if (!P.empty()) throw logic_error("Pipeline not empty initially!");

    // Try to confuse the pipeline by feeding it a query and flushing
    P.retain();
    const string Q = "SELECT * FROM pg_tables";
    P.insert(Q);
    P.flush();

    if (!P.empty()) throw logic_error("Pipeline not empty after flush()");

    // See if complete() breaks retain() as it should
    P.retain();
    P.insert(Q);
    if (P.empty()) throw logic_error("Pipeline empty after insert()");
    P.complete();
    if (P.empty()) throw logic_error("Pipeline empty after complete()");

    if (P.retrieve().second.query() != Q)
      throw logic_error("Unexpected query in result");

    if (!P.empty()) throw logic_error("Pipeline not empty after retrieve()");

    // See if retrieve() breaks retain() when it needs to
    P.retain();
    P.insert(Q);
    if (P.retrieve().second.query() != Q)
      throw logic_error("Unexpected query in result");

    // See if regular retain()/resume() works
    for (int i=0; i<5; ++i) TestPipeline(P, i);

    // See if retrieve() fails on an empty pipeline, as it should
    bool failed = true;
    try
    {
      disable_noticer d(C);
      P.retrieve();
      failed = false;
    }
    catch (const exception &e)
    {
      failed = true;
      cout << "(Expected) " << e.what() << endl;
    }
    if (!failed)
      throw logic_error("retrieve() from empty pipeline failed to complain");
  }
  catch (const sql_error &e)
  {
    cerr << "Database error: " << e.what() << endl
         << "Query was: " << e.query() << endl;
    return 2;
  }
  catch (const exception &e)
  {
    cerr << "Exception: " << e.what() << endl;
    return 2;
  }
  catch (...)
  {
    cerr << "Unhandled exception" << endl;
    return 100;
  }

  return 0;
}

