/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pg_tablereader.h
 *
 *   DESCRIPTION
 *      definition of the Pg::TableReader class.
 *   Pg::TableReader enables optimized batch reads from a database table
 *
 * Copyright (c) 2001, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_TABLEREADER_H
#define PG_TABLEREADER_H

#include <string>

#include "pg_result.h"
#include "pg_tablestream.h"

namespace Pg
{

// A TableReader provides efficient read access to a database table.  This is
// not as flexible as a normal query using the Result class however:
//  - Can only dump tables, not views or arbitrary queries
//  - Has no knowledge of metadata
//  - Is unable to reorder, rename, omit or enrich fields
//  - Does not support filtering of records
//
// On the other hand, it can read rows of data and transform them into any
// container or container-like object that supports STL back-inserters.  Since
// the TableReader has no knowledge of the types of data expected, it treats
// all fields as strings.
class TableReader : public TableStream
{
public:
  TableReader(Transaction &Trans, PGSTD::string Name);
  ~TableReader();

  TableReader &operator>>(Result &);
  TableReader &operator>>(PGSTD::string &);
  template<typename TUPLE> TableReader &operator>>(TUPLE &);

  operator bool() const { return !m_Done; }
  bool operator!() const { return m_Done; }

  bool GetRawLine(PGSTD::string &Line);
  template<typename TUPLE> void Tokenize(PGSTD::string, TUPLE &) const;

private:
  bool m_Done;
};

}

// TODO: Find meaningful definition of input iterator


template<typename TUPLE> 
inline void Pg::TableReader::Tokenize(PGSTD::string Line, TUPLE &T) const
{
  PGSTD::back_insert_iterator<TUPLE> ins = PGSTD::back_inserter(T);

  // Filter and tokenize line, inserting tokens at end of T
  PGSTD::string::size_type token = 0;
  for (PGSTD::string::size_type i=0; i < Line.size(); ++i)
  {
    switch (Line[i])
    {
    case '\t': // End of token
      *ins++ = PGSTD::string(Line, token, i-token);
      token = i+1;
      break;

    case '\\':
      // Ignore the backslash and accept literally whatever comes after it 
      if ((i+1) >= Line.size()) 
	throw PGSTD::runtime_error("Row ends in backslash");

      if (Line[i+1] == 'N')
      {
        // This is a \N, signifying a NULL value.
	Line.replace(i, 2, NullStr());
	i += NullStr().size() - 1;
      }
      else
      {
        Line.erase(i, 1);
      }
      break;
    }
  }

  *ins++ = PGSTD::string(Line, token);
}


template<typename TUPLE> 
inline Pg::TableReader &Pg::TableReader::operator>>(TUPLE &T)
{
  PGSTD::string Line;
  if (GetRawLine(Line)) Tokenize(Line, T);
  return *this;
}



#endif

