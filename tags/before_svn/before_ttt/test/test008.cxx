#include <iostream>
#include <stdexcept>
#include <vector>

#include <pqxx/connection.h>
#include <pqxx/tablereader.h>
#include <pqxx/transaction.h>

using namespace PGSTD;
using namespace pqxx;


// Simple test program for libpqxx.  Read a table using a TableReader, which 
// may be faster than a conventional query.  A TableReader is really a frontend
// for a PostgreSQL COPY TO stdout command.
//
// Usage: test8 [connect-string] [table]
//
// Where connect-string is a set of connection options in Postgresql's
// PQconnectdb() format, eg. "dbname=template1" to select from a database
// called template1, or "host=foo.bar.net user=smith" to connect to a backend 
// running on host foo.bar.net, logging in as user smith.
//
// The default table name is "events" as used by other test programs.
// PostgreSQL currently implements pg_tables as a view, which cannot be read by
// using the COPY command.  Otherwise, pg_tables would have made a better 
// default value here.
int main(int argc, char *argv[])
{
  try
  {
    // Set up a connection to the backend
    Connection C(argv[1]);

    string Table = "events";
    if (argc > 2) Table = argv[2];

    // Begin a transaction acting on our current connection
    Transaction T(C, "test8");

    vector<string> R, First;

    // (Set up a block here to ensure our TableReader goes out of scope
    //  before our Transaction does, see below)
    {
      // Set up a TableReader stream to read data from table pg_tables
      TableReader Stream(T, Table);

      // Read results into string vectors and print them
      for (int n=0; (Stream >> R); ++n)
      {
        // Keep the first row for later consistency check
        if (n == 0) First = R;

        cout << n << ":\t";
        for (vector<string>::const_iterator i = R.begin(); i != R.end(); ++i)
          cout << *i << '\t';
        cout << endl;
        R.clear();
      }
    }

    // Verify the contents we got for the first row
    if (!First.empty())
    {
      TableReader Verify(T, Table);
      string Line;

      if (!Verify.GetRawLine(Line))
	throw logic_error("TableReader got rows the first time around, "
	                  "but none the second time!");

      cout << "First tuple was: " << endl << Line << endl;

      Verify.Tokenize(Line, R);
      if (R != First)
        throw logic_error("Got different results re-parsing first tuple!");
    }
  }
  catch (const sql_error &e)
  {
    // If we're interested in the text of a failed query, we can write separate
    // exception handling code for this type of exception
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

