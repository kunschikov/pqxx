/*-------------------------------------------------------------------------
 *
 *   FILE
 *	cachedresult.cxx
 *
 *   DESCRIPTION
 *      implementation of the pqxx::CachedResult class.
 *   pqxx::CachedResult transparently fetches and caches query results on demand
 *
 * Copyright (c) 2001-2003, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 *-------------------------------------------------------------------------
 */
#include <stdexcept>

#include "pqxx/cachedresult.h"

using namespace PGSTD;

pqxx::CachedResult::CachedResult(pqxx::TransactionItf &Trans,
                                 const char Query[],
				 const PGSTD::string &BaseName,
				 size_type Granularity) :
  m_Granularity(Granularity),
  m_Cache(),
  m_Cursor(Trans, Query, BaseName, Granularity)
{
  // We can't accept granularity of 1 here, because some block number 
  // arithmetic might overflow.
  if (m_Granularity <= 1)
    throw out_of_range("Invalid CachedResult granularity");
}


pqxx::CachedResult::size_type pqxx::CachedResult::size() const
{
  if (m_Cursor.size() == Cursor::pos_unknown)
  {
    m_Cursor.Move(Cursor::BACKWARD_ALL());
    m_Cursor.Move(Cursor::ALL());
  }
  return m_Cursor.size();
}


bool pqxx::CachedResult::empty() const
{
  return (m_Cursor.size() == 0) ||
         ((m_Cursor.size() == Cursor::pos_unknown) &&
	  m_Cache.empty() &&
	  GetBlock(0).empty());
}

void pqxx::CachedResult::clear()
{
  m_Cache.clear();
}


void pqxx::CachedResult::MoveTo(blocknum Block) const
{
  if (Block < 0)
    throw out_of_range("Negative result set index");

  const Cursor::size_type BlockStart = FirstRowOf(Block);
  m_Cursor.MoveTo(BlockStart);
  if (m_Cursor.Pos() != BlockStart)
    throw out_of_range("Tuple number out of range");
}


pqxx::Result pqxx::CachedResult::Fetch() const
{
  Result R;
  size_type Pos = m_Cursor.Pos();

  if (m_Cursor >> R) m_Cache.insert(make_pair(BlockFor(Pos), CacheEntry(R)));

  return R;
}


