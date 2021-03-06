/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pqxx/cursor.h
 *
 *   DESCRIPTION
 *      definition of the pqxx::Cursor class.
 *   pqxx::Cursor represents a database cursor.
 *
 * Copyright (c) 2001-2004, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#ifndef PQXX_CURSOR_H
#define PQXX_CURSOR_H

#include "pqxx/result.h"
#include "pqxx/transaction_base.h"
#include "pqxx/util.h"

/* Methods tested in eg. self-test program test001 are marked with "//[t1]"
 */

namespace pqxx
{
class result;


// Work around bug in CodeWarrior 8.3
#ifdef __MWERKS__
#pragma defer_defarg_parsing on
#endif


/// SQL cursor class.
/** Cursor behaves as an output stream generating result objects.  It may
 * be used to fetch rows individually or in blocks, in which case each result
 * coming out of the stream may contain more than one Tuple.  A cursor may be
 * positioned on any row of data, or on an "imaginary" row before the first
 * actual row, or on a similar imaginary row beyond the last one.  This differs
 * from standard C++ practice, where a container only has an imaginary element
 * (called end()) beyond its last actual element.
 *
 * When data is fetched, the cursor is moved before data is collected, so that
 * afterwards the cursor will be positioned on the last row it returned.  A
 * freshly created cursor is positioned on the imaginary row (numbered 0) before
 * its first * actual one (which is numbered 1).  Thus, a simple Fetch(1) will 
 * then return the first actual row, i.e. row 1.
 *
 * Postgres does not currently support modification of data through a cursor.
 * Also, not all queries support cursors that go backwards.  Unfortunately there
 * is no documentation on which queries do and which queries don't so you may
 * have to experiment before using cursors for anything but plain forward-only
 * result set iteration.
 *
 * @warning A Cursor is only valid within the transaction in which it was 
 * created.  The result set must not change during its existence, or the 
 * cursor's positioning logic will get horribly confused.  For this reason, 
 * Cursor should only be used inside serializable transactions.  This class is
 * to be replaced by a C++-style iterator interface.
 *
 * @deprecated This class was not ported to the 2.x libpqxx API because it is to
 * be replaced by a container/iterator interface.
 */

class PQXX_LIBEXPORT Cursor
{
  enum { dist_next=1 };

public:
  typedef result::size_type size_type;

  enum pos { pos_unknown = -1, pos_start = 0 };

  /// Exception thrown when cursor position is requested, but is unknown
  struct PQXX_LIBEXPORT unknown_position : PGSTD::runtime_error
  {
    unknown_position(const PGSTD::string &cursorname) :
      PGSTD::runtime_error("Position for cursor '" + cursorname + "' "
	                   "is unknown") 
    {
    }
  };

  /// Constructor.  Creates a cursor.
  /** 
   * @param T is the transaction that this cursor lives in.
   * @param Query defines a data set that the cursor should traverse.
   * @param BaseName optional name for the cursor, must begin with a letter 
   * and contain letters and digits only.  
   * @param Count the stride of the cursor, ie. the number of rows fetched at a
   * time.  This defaults to 1.
   */
  template<typename TRANSACTION> 
    Cursor(TRANSACTION &T,
           const char Query[], 
	   const PGSTD::string &BaseName="cur",
	   size_type Count=dist_next) :					//[t3]
      m_Trans(T),
      m_Name(),
      m_Count(Count),
      m_Done(false),
      m_Pos(pos_start),
      m_Size(pos_unknown)
  {
    // Trigger build error if T has insufficient isolation level
    error_permitted_isolation_level(PQXX_TYPENAME TRANSACTION::isolation_tag());
    init(BaseName, Query);
  }

  /// Special-purpose constructor.  Adopts existing SQL cursor.  Use with care.
  /**
   * Forms a Cursor object around an existing SQL cursor, as returned by e.g.
   * a function.  The SQL cursor will be destroyed by the Cursor destructor as
   * if it had been created by the Cursor object.  The cursor can only be used
   * inside the transaction that created it.
   *
   * Use this creation method only with great care.  Read on for important 
   * caveats.
   *
   * The Cursor object does not know the current position of the SQL cursor. 
   * For complicated technical reasons, this will cause trouble when the cursor
   * reaches the end of the result set.  Therefore, before you ever move the 
   * resulting Cursor forward, you should always move it backwards with a
   * Move(Cursor::BACKWARD_ALL()).  This will reset the internal position
   * counter to the beginning of the result set.
   *
   * Once the Cursor object is created, never try to access the underlying SQL
   * cursor in any way.  There is no way for libpqxx to check for this, and it
   * could potentially undermine operation of the Cursor object in unexpected
   * ways.
   *
   * Passing the name of the cursor as a string is not allowed, both to avoid
   * confusion with the constructor that creates its own SQL cursor, and to
   * discourage unnecessary manual cursor creation.
   *
   * @param T must be the transaction in which the cursor was created.
   * @param Name query result field with the name of the existing SQL cursor.
   * @param Count the stride of the cursor, ie. the number of rows fetched at a
   * time.  This defaults to 1.
   */
  template<typename TRANSACTION>
    Cursor(TRANSACTION &T,
           const result::field &Name,
	   size_type Count=dist_next) :					//[t45]
      m_Trans(T),
      m_Name(Name.c_str()),
      m_Count(Count),
      m_Done(false),
      m_Pos(pos_unknown),
      m_Size(pos_unknown)
  {
    // Trigger build error if T has insufficient isolation level
    error_permitted_isolation_level(PQXX_TYPENAME TRANSACTION::isolation_tag());
  }

  /// Set new stride, ie. the number of rows to fetch at a time.
  size_type SetCount(size_type);					//[t19]

  /// Fetch Count rows of data.
  /** The first row returned, if any, will be the one directly before (if Count
   * is negative) or after (if Count is positive) the cursor's current position.
   * If an exception occurs during the operation, the cursor will be left in an
   * unknown position.  Fetching from a cursor in an unknown position may still
   * work, but libpqxx cannot determine its current position until the cursor is
   * moved off either edge of the result set.
   *
   * The number of rows fetched will not exceed Count, but it may be lower.
   */
  result Fetch(size_type Count);					//[t19]

  /// Move forward by Count rows (negative for backwards) through the data set.
  /** Returns the number of rows skipped.  This need not be the same number
   * reported by PostgreSQL, which has a different but deceptively similar
   * meaning.
   *
   * Also, note that cursors may reside on nonexistant rows, and that any
   * exception during the operation will leave the cursor in an unknown 
   * position.
   */
  size_type Move(size_type Count);					//[t42]

  void MoveTo(size_type);						//[t44]

  /// Constant: "next fetch/move should span as many rows as possible."
  /** If the number of rows ahead exceeds the largest number your machine can
   * comfortably conceive, this may not actually be all remaining rows in the 
   * result set.
   */
  static size_type ALL() throw ();					//[t3]

  /// Constant: "next fetch/move should cover just the next row."
  static size_type NEXT() throw () { return dist_next; }		//[t19]

  /// Constant: "next fetch/move should go back one row."
  static size_type PRIOR() throw () { return -1; }			//[t19]

  /// Constant: "next fetch/move goes backwards, spanning as many rows as 
  /// possible.
  /** If the number of rows behind the cursor exceeds the largest number your
   * machine can comfortably conceive, this may not bring you all the way back
   * to the beginning.
   */
  static size_type BACKWARD_ALL() throw ();				//[t19]

  /// Fetch rows.
  /** The number of rows retrieved will be no larger than (but may be lower
   * than) the rowcount set using the SetCount() function, or passed to the
   * constructor as the Count argument.  The default is 1.
   * This operator lends itself to "while (Cur >> R) { ... }"-style use; the
   * Cursor's conversion to bool tests whether it has arrived at the end of its
   * data set.
   */
  Cursor &operator>>(result &);						//[t3]

  /// May there be more rows coming?
  operator bool() const throw () { return !m_Done; }			//[t3]
  /// Are we done?
  bool operator!() const throw () { return m_Done; }			//[t3]

  /// Move N rows forward.
  Cursor &operator+=(size_type N) { Move(N); return *this;}		//[t19]
  /// Move N rows backward.
  Cursor &operator-=(size_type N) { Move(-N); return *this;}		//[t19]

  /// Number of actual tuples, or pos_unknown if not currently known.
  /** Size will become known when the cursor passes the end of the result set, 
   * either by moving past it or by attempting to fetch more rows than are
   * available.  The count only includes actual tuples of data, but not the
   * two "imaginary" rows before the first actual row and after the last actual
   * row, respectively.
   *
   * If an exception occurs while moving or fetching, the cursor's position may
   * be left in unknown state, and in this condition the cursor will not be able
   * to compute its size until it has been moved to one of the two imaginary
   * rows on either side of the actual result set.
   */
  size_type size() const throw () { return m_Size; }			//[t44]

  /// Current cursor position.
  /** If an exception occurs while moving or fetching, the cursor's position may
   * be left in an unknown state.  Moving the cursor off either edge of its
   * result set will bring it back to a known position.
   *
   * Requesting a cursor's position while it is in an unknown state will cause
   * an unknown_position exception to be thrown.
   */
  size_type Pos() const throw (unknown_position)			//[t43]
  { if (m_Pos==pos_unknown) throw unknown_position(m_Name); return m_Pos; }


private:
  static PGSTD::string OffsetString(size_type);
  void init(const PGSTD::string &BaseName, const char Query[]);
  PGSTD::string MakeFetchCmd(size_type) const;
  size_type NormalizedMove(size_type Intended, size_type Actual);

#ifndef PQXX_WORKAROUND_VC7
  /// Only defined for permitted isolation levels (in this case, serializable)
  /** If you get a link or compile error saying this function is not defined,
   * that means a Cursor is being created on a transaction that doesn't have a
   * sufficient isolation level to support the Cursor's reliable operation.
   */
  template<typename ISOLATIONTAG> 
    static inline void error_permitted_isolation_level(ISOLATIONTAG) throw();

#if defined(__SUNPRO_CC)
  // Incorrect, but needed to compile with Sun CC
  template<> static void 
    error_permitted_level(isolation_traits<serializable>) throw() {}
#endif	// __SUNPRO_CC
#else
  // Incorrect, but needed to compile with Visual C++ 7
  template<> static inline void 
    error_permitted_isolation_level(isolation_traits<serializable>) throw ();
#endif

  transaction_base &m_Trans;
  PGSTD::string m_Name;
  size_type m_Count;
  bool m_Done;
  size_type m_Pos;
  size_type m_Size;

  // Not allowed:
  Cursor(const Cursor &);
  Cursor &operator=(const Cursor &);
};

template<> inline void 
Cursor::error_permitted_isolation_level(isolation_traits<serializable>) throw ()
	{}

} // namespace pqxx

#endif

