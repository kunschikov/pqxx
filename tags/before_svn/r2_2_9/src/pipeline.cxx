/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pipeline.cxx
 *
 *   DESCRIPTION
 *      implementation of the pqxx::pipeline class
 *   Throughput-optimized query manager
 *
 * Copyright (c) 2003-2004, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#include "pqxx/compiler.h"

#include <algorithm>
#include <cassert>

#include "pqxx/dbtransaction"
#include "pqxx/pipeline"

using namespace PGSTD;
using namespace pqxx;
using namespace pqxx::internal::pq;


namespace
{
const string theSeparator("; ");
const string theDummyValue("1");
const string theDummyQuery("SELECT " + theDummyValue + theSeparator);
}


pqxx::pipeline::pipeline(transaction_base &t, const string &PName) :
  internal::transactionfocus(t, PName, "pipeline"),
  m_queries(),
  m_issuedrange(),
  m_retain(0),
  m_num_waiting(0),
  m_q_id(0),
  m_dummy_pending(false),
  m_error(qid_limit())
{
  m_issuedrange = make_pair(m_queries.end(), m_queries.end());
  invariant();
  register_me();
}


pqxx::pipeline::~pipeline() throw ()
{
  try { flush(); } catch (const exception &) {}
  if (registered()) unregister_me();
}


pipeline::query_id pqxx::pipeline::insert(const string &q)
{
  invariant();

  const query_id qid = generate_id();
  assert(qid > 0);
  assert(m_queries.lower_bound(qid)==m_queries.end());
  const QueryMap::iterator i = m_queries.insert(make_pair(qid,Query(q))).first;

  if (m_issuedrange.second == m_queries.end())
  {
    m_issuedrange.second = i;
    if (m_issuedrange.first == m_queries.end()) m_issuedrange.first = i;
  }
  m_num_waiting++;

  assert(m_issuedrange.first != m_queries.end());
  assert(m_issuedrange.second != m_queries.end());
  invariant();

  if (m_num_waiting > m_retain)
  {
    if (have_pending()) receive_if_available();
    if (!have_pending()) issue();
  }

  invariant();

  return qid;
}


void pqxx::pipeline::complete()
{
  invariant();

  if (have_pending()) receive(m_issuedrange.second);
  if (m_num_waiting && (m_error == qid_limit()))
  {
    assert(!have_pending());
    issue();
    assert(!m_num_waiting);
    assert(have_pending());
    assert(m_issuedrange.second == m_queries.end());
    receive(m_queries.end());
    assert((m_error!=qid_limit()) || !have_pending());
  }
  invariant();
  assert((m_num_waiting == 0) || (m_error != qid_limit()));
  assert(!m_dummy_pending);
}


void pqxx::pipeline::flush()
{
  invariant();

  if (m_queries.empty()) return;
  if (have_pending()) receive(m_issuedrange.second);
  m_issuedrange.first = m_issuedrange.second = m_queries.end();
  m_num_waiting = 0;
  m_dummy_pending = false;
  m_queries.clear();

  invariant();
}


bool pqxx::pipeline::is_finished(pipeline::query_id q) const
{
  if (m_queries.find(q) == m_queries.end())
    throw logic_error("Requested status for unknown query " + to_string(q));
  return (m_issuedrange.first==m_queries.end()) ||
         (q < m_issuedrange.first->first && q < m_error);
}


pair<pipeline::query_id, result> pqxx::pipeline::retrieve()
{
  if (m_queries.empty())
    throw logic_error("Attempt to retrieve result from empty pipeline");
  return retrieve(m_queries.begin());
}


int pqxx::pipeline::retain(int retain_max)
{
  invariant();

  if (retain_max < 0)
    throw range_error("Attempt to make pipeline retain " +
	to_string(retain_max) + " queries");

  const int oldvalue = m_retain;
  m_retain = retain_max;

  if (m_num_waiting >= m_retain) resume();

  invariant();

  return oldvalue;
}


void pqxx::pipeline::resume()
{
  invariant();

  if (have_pending()) receive_if_available();
  if (!have_pending() && m_num_waiting)
  {
    issue();
    receive_if_available();
  }

  invariant();
}


void pqxx::pipeline::invariant() const
{
  assert(m_q_id >= 0);
  assert(m_q_id <= qid_limit());

  assert(m_retain >= 0);
  assert(m_num_waiting >= 0);

  const QueryMap::const_iterator 
    start_of_issued = m_issuedrange.first,
    end_of_issued = m_issuedrange.second;

  if (m_queries.empty()) assert(start_of_issued==m_queries.end());
  assert(distance(m_queries.begin(), start_of_issued) >= 0);
  assert(distance(m_issuedrange.first, m_issuedrange.second) >= 0);
  assert(distance(end_of_issued, m_queries.end()) >= 0);

  if (!m_queries.empty())
  {
    assert(m_queries.begin()->first > 0);
    assert(m_queries.rbegin()->first < (m_q_id+1));

    assert(m_num_waiting >= 0);
    assert(size_t(m_num_waiting) <= size_t(m_queries.size()));

    if (have_pending())
    {
      assert(m_issuedrange.first != m_queries.end());
      if (m_error == qid_limit())
        assert(m_num_waiting == distance(end_of_issued, m_queries.end()));
    }
  }
  else
  {
    assert(m_issuedrange.first == m_queries.end());
    assert(m_issuedrange.second == m_queries.end());
    assert(!have_pending());
    assert(!m_num_waiting);
    assert(!m_dummy_pending);
  }

  assert(m_error != 0);
}


pipeline::query_id pqxx::pipeline::generate_id()
{
  if (m_q_id == qid_limit())
    throw overflow_error("Too many queries went through pipeline");
  ++m_q_id;
  return m_q_id;
}


void pqxx::pipeline::issue()
{
  assert(m_num_waiting);
  assert(!have_pending());
  assert(!m_dummy_pending);
  assert(m_num_waiting);
  invariant();

  // Retrieve that NULL result for the last query, if needed
  obtain_result();

  // Don't issue anything if we've encountered an error
  if (m_error < qid_limit()) return;

  // Start with oldest query (lowest id) not in previous issue range
  const QueryMap::iterator oldest = m_issuedrange.second;
  assert(oldest != m_queries.end());

  // Construct cumulative query string for entire batch
  string cum;
  int num_issued = 0;
  for (QueryMap::const_iterator i = oldest;
       i != m_queries.end();
       ++i, ++num_issued)
  {
    cum += i->second.get_query();
    cum += theSeparator;
  }
  cum.resize(cum.size() - theSeparator.size());
  const bool prepend_dummy = (num_issued > 1);
  if (prepend_dummy) cum = theDummyQuery + cum;

  m_Trans.start_exec(cum);

  // Since we managed to send out these queries, update state to reflect this
  m_dummy_pending = prepend_dummy;
  m_issuedrange.first = oldest;
  m_issuedrange.second = m_queries.end();
  m_num_waiting -= num_issued;

  invariant();
}


void pqxx::pipeline::internal_error(const string &err) throw (logic_error)
{
  set_error_at(0);
  throw logic_error(err);
}


bool pqxx::pipeline::obtain_result(bool expect_none)
{
  assert(!m_dummy_pending);
  assert(!m_queries.empty());
  invariant();

  internal::pq::PGresult *r = m_Trans.get_result();
  if (!r)
  {
    if (have_pending() && !expect_none)
    {
      set_error_at(m_issuedrange.first->first);
      m_issuedrange.second = m_issuedrange.first;
    }
    return false;
  }

  assert(r);
  const result res(r);

  if (!have_pending())
  {
    set_error_at(m_queries.begin()->first);
    throw logic_error("Got more results from pipeline than there were queries");
  }

  // Must be the result for the oldest pending query
  if (!m_issuedrange.first->second.get_result().empty())
    internal_error("libpqxx internal error: multiple results for one query");

  m_issuedrange.first->second.set_result(res);
  ++m_issuedrange.first;

  invariant();

  return true;
}


void pqxx::pipeline::obtain_dummy()
{
  assert(m_dummy_pending);
  internal::pq::PGresult *const r = m_Trans.get_result();
  m_dummy_pending = false;

  if (!r) 
    internal_error("libpqxx internal error: "
	  "pipeline got no result from backend when it expected one");

  result R(r);
  bool OK = false;
  try
  {
    R.CheckStatus("");
    OK = true;
  }
  catch (const sql_error &)
  {
  }
  if (OK)
  {
    if (R.size() > 1)
      internal_error("libpqxx internal error: "
	  "unexpected result for dummy query in pipeline");

    if (string(R.at(0).at(0).c_str()) != theDummyValue)
      internal_error("libpqxx internal error: "
	    "dummy query in pipeline returned unexpected value");
    return;
  }

  /* Since none of the queries in the batch were actually executed, we can
   * afford to replay them one by one until we find the exact query that
   * caused the error.  This gives us not only a more specific error message
   * to report, but also tells us which query to report it for.
   */
  // First, give the whole batch the same syntax error message, in case all else
  // is going to fail.
  for (QueryMap::iterator i = m_issuedrange.first;
       i != m_issuedrange.second;
       ++i) i->second.set_result(R);

  // Remember where the end of this batch was
  const QueryMap::iterator stop = m_issuedrange.second;

  // Retrieve that NULL result for the last query, if needed
  obtain_result(true);


  // Reset internal state to forget botched batch attempt
  m_num_waiting += distance(m_issuedrange.first, stop);
  m_issuedrange.second = m_issuedrange.first;

  assert(!m_dummy_pending);
  assert(!have_pending());
  assert(m_num_waiting > 0);

  // Issue queries in failed batch one at a time.
  unregister_me();
  try
  {
    do 
    {
      m_num_waiting--;
      const string &query = m_issuedrange.first->second.get_query();
      const result res(m_Trans.exec(query));
      m_issuedrange.first->second.set_result(res);
      res.CheckStatus(query);
      ++m_issuedrange.first;
    }
    while (m_issuedrange.first != stop);
  }
  catch (const exception &e)
  {
    assert(m_issuedrange.first != m_queries.end());

    const query_id thud = m_issuedrange.first->first;
    ++m_issuedrange.first;
    m_issuedrange.second = m_issuedrange.first;
    QueryMap::const_iterator q = m_issuedrange.first;
    set_error_at( (q == m_queries.end()) ?  thud + 1 : q->first);

    assert(m_num_waiting == distance(m_issuedrange.second, m_queries.end()));
  }

  assert(m_issuedrange.first != m_queries.end());
  assert(m_error <= m_q_id);
  invariant();
}


pair<pipeline::query_id, result>
pqxx::pipeline::retrieve(pipeline::QueryMap::iterator q)
{
  invariant();

  if (q == m_queries.end())
    throw logic_error("Attempt to retrieve result for unknown query");

  if (q->first >= m_error)
    throw runtime_error("Could not complete query in pipeline "
	"due to error in earlier query");

  // If query hasn't issued yet, do it now
  if (m_issuedrange.second != m_queries.end() && 
      (q->first >= m_issuedrange.second->first))
  {
    assert(distance(m_issuedrange.second, q) >= 0);

    if (have_pending()) receive(m_issuedrange.second);
    if (m_error == qid_limit()) issue();
  }

  // If result not in yet, get it; else get at least whatever's convenient
  if (have_pending())
  {
    if (q->first >= m_issuedrange.first->first)
    {
      QueryMap::const_iterator suc = q;
      ++suc;
      receive(suc);
    }
    else
    {
      receive_if_available();
    }
  }

  assert((m_error <= q->first) || (q != m_issuedrange.first));

  if (q->first >= m_error)
    throw runtime_error("Could not complete query in pipeline "
	"due to error in earlier query");

  // Don't leave the backend idle if there are queries waiting to be issued
  if (m_num_waiting && !have_pending() && (m_error==qid_limit())) issue();

  const string query(q->second.get_query());
  const result R = q->second.get_result();
  pair<query_id,result> P(make_pair(q->first, R));

  m_queries.erase(q);

  invariant();

  R.CheckStatus(query);
  return P;
}


void pqxx::pipeline::get_further_available_results()
{
  assert(!m_dummy_pending);
  while (!m_Trans.is_busy() && obtain_result()) m_Trans.consume_input();
}


void pqxx::pipeline::receive_if_available()
{
  invariant();

  m_Trans.consume_input();
  if (m_Trans.is_busy()) return;

  if (m_dummy_pending) obtain_dummy();
  if (have_pending()) get_further_available_results();

  invariant();
}


void pqxx::pipeline::receive(pipeline::QueryMap::const_iterator stop)
{
  invariant();
  assert(have_pending());

  if (m_dummy_pending) obtain_dummy();

  while (obtain_result() && (m_issuedrange.first != stop));

  // Also haul in any remaining "targets of opportunity"
  if (m_issuedrange.first == stop) get_further_available_results();
}


