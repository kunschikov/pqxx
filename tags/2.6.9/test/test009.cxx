#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

#include <pqxx/connection>
#include <pqxx/tablewriter>
#include <pqxx/transaction>

using namespace PGSTD;
using namespace pqxx;


// Test program for libpqxx.  Create a table and write data to it, using
// tablewriter's back_insert_iterator.
//
// Usage: test009 [connect-string] [table] [column]
//
// Where the connect-string is a set of connection options in Postgresql's
// PQconnectdb() format, eg. "dbname=template1" to select from a database
// called template1, or "host=foo.bar.net user=smith" to connect to a backend
// running on host foo.bar.net, logging in as user smith.
//
// The default table name is "testtable."

namespace
{
set< vector<string> > Contents;


void PrepareContents()
{
  const char *Strings[] =
  {
    "foo",
    "bar",
    "!",
    "\t",
    "'",
    "\"",
    " ",
    "|",
    "*",

    0
  };

  for (int i=0; Strings[i]; ++i)
  {
    vector<string> v;
    v.push_back(Strings[i]);
    Contents.insert(v);
  }
}


void FillTable(transaction_base &T, string TableName, string Column)
{
  tablewriter W(T, TableName, &Column, &Column+1);
  W.reserve(Contents.size());

  copy(Contents.begin(), Contents.end(), back_inserter(W));

  cout << Contents.size() << " rows written." << endl;
}


void CheckTable(transaction_base &T, string TableName)
{
  result Count = T.exec(("SELECT COUNT(*) FROM " + TableName).c_str());
  size_t Rows = 0;

  if (!Count[0][0].to(Rows)) throw runtime_error("NULL row count!");
  cout << Rows << " rows in table." << endl;

  if (Rows != Contents.size())
    throw runtime_error("Found " +
		        string(Count[0][0].c_str()) +
			" rows in table--after writing " +
			to_string(Contents.size()) +
			"!");

  // TODO: Compare table contents to Contents
}
}

int main(int argc, char *argv[])
{
  try
  {
    const char *ConnStr = argv[1];

    PrepareContents();

    // Set up two connections to the backend: one to read our original table,
    // and another to write our copy
    connection C(ConnStr);

    // Select our original and destination table names
    string TableName = "pqxxtesttable";
    if (argc > 2) TableName = argv[2];

    string Column = "content";
    if (argc > 3) Column = argv[3];

    work T(C, "test9");

    // Create table.  If the table already existed, better to fail now.
    stringstream ctq;
    ctq << "CREATE TABLE " << TableName << '(' << Column << " VARCHAR)";
    T.exec(ctq);

    FillTable(T, TableName, Column);
    CheckTable(T, TableName);

    T.exec(("DROP TABLE " + TableName).c_str());
    T.commit();
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

