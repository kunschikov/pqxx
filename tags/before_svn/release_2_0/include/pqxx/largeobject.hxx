/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pqxx/largeobject.hxx
 *
 *   DESCRIPTION
 *      libpqxx's Large Objects interface
 *   Allows access to large objects directly, or through I/O streams
 *   DO NOT INCLUDE THIS FILE DIRECTLY; include pqxx/largeobject instead.
 *
 * Copyright (c) 2003, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#include "pqxx/libcompiler.h"

#ifdef PQXX_HAVE_STREAMBUF
#include <streambuf>
#else
#include <streambuf.h>
#endif

#include "pqxx/dbtransaction"


namespace pqxx
{

class largeobjectaccess;

/// Identity of a large object
/** This class encapsulates the identity of a large object.  To access the
 * contents of the object, create a largeobjectaccess, a largeobject_streambuf,
 * or an ilostream, an olostream or a lostream around the largeobject.
 *
 * A largeobject must be accessed only from within a backend transaction, but
 * the object's identity remains valid as long as the object exists.
 */
class PQXX_LIBEXPORT largeobject
{
public:
  typedef long size_type;

  /// Refer to a nonexistent large object (similar to what a null pointer does)
  largeobject();							//[t48]

  /// Create new large object
  /** @param T backend transaction in which the object is to be created
   */
  explicit largeobject(dbtransaction &T);				//[t48]

  /// Wrap object with given oid
  /** Convert combination of a transaction and object identifier into a
   * large object identity.  Does not affect the database.
   * @param O object identifier for the given object
   */
  explicit largeobject(oid O) : m_ID(O) {}				//[t48]

  /// Import large object from a local file
  /** Creates a large object containing the data found in the given file.
   * @param T the backend transaction in which the large object is to be created
   * @param File a filename on the client program's filesystem
   */
  largeobject(dbtransaction &T, const PGSTD::string &File);		//[t53]

  /// Take identity of an opened large object
  /** Copy identity of already opened large object.  Note that this may be done
   * as an implicit conversion.
   * @param O already opened large object to copy identity from
   */
  largeobject(const largeobjectaccess &O);				//[t50]

  /// Object identifier
  /** The number returned by this function identifies the large object in the
   * database we're connected to (or oid_none is returned if we refer to the
   * null object).
   */
  oid id() const throw () { return m_ID; }				//[t48]

  /// Comparison is only valid between large objects in the same database.
  bool operator==(const largeobject &other) const 			//[t51]
	  { return m_ID == other.m_ID; }
  /// Comparison is only valid between large objects in the same database.
  bool operator!=(const largeobject &other) const 			//[t51]
	  { return m_ID != other.m_ID; }
  /// Comparison is only valid between large objects in the same database.
  bool operator<=(const largeobject &other) const 			//[t51]
	  { return m_ID <= other.m_ID; }
  /// Comparison is only valid between large objects in the same database.
  bool operator>=(const largeobject &other) const 			//[t51]
	  { return m_ID >= other.m_ID; }
  /// Comparison is only valid between large objects in the same database.
  bool operator<(const largeobject &other) const 			//[t51]
	  { return m_ID < other.m_ID; }
  /// Comparison is only valid between large objects in the same database.
  bool operator>(const largeobject &other) const 			//[t51]
	  { return m_ID > other.m_ID; }

  /// Export large object's contents to a local file
  /** Writes the data stored in the large object to the given file.
   * @param T the transaction in which the object is to be accessed
   * @param File a filename on the client's filesystem
   */
  void to_file(dbtransaction &T, const PGSTD::string &File) const; 	//[t52]

  /// Delete large object from database
  /** Unlike its low-level equivalent cunlink, this will throw an exception if 
   * deletion fails.
   * @param T the transaction in which the object is to be deleted
   */
  void remove(dbtransaction &T) const;					//[t48]

protected:
  static PGconn *RawConnection(const dbtransaction &T)
  {
    return T.conn().RawConnection();
  }

  PGSTD::string Reason() const;

private:
  oid m_ID;
};


// TODO: New hierarchy with separate read / write / mixed-mode access

/// Accessor for large object's contents.
class PQXX_LIBEXPORT largeobjectaccess : private largeobject
{
public:
  using largeobject::size_type;
  typedef long off_type;
  typedef size_type pos_type;

  /// Open mode: in, out (can be combined with the "or" operator)
  /** According to the C++ standard, these should be in std::ios_base.  We take
   * them from std::ios instead, which should be safe because it inherits the
   * same definition, to accommodate gcc 2.95 & 2.96.
   */
  typedef PGSTD::ios::openmode openmode;

  /// Seek direction: beg, cur, end
  /** According to the C++ standard, these should be in std::ios_base.  We take
   * them from std::ios instead, which should be safe because it inherits the
   * same definition, to accommodate gcc 2.95 & 2.96.
   */
  typedef PGSTD::ios::seekdir seekdir;

  /// Create new large object and open it
  /** 
   * @param T backend transaction in which the object is to be created
   * @param mode access mode, defaults to ios_base::in | ios_base::out
   */
  explicit largeobjectaccess(dbtransaction &T, 
			     openmode mode = 
				PGSTD::ios::in | 
				PGSTD::ios::out);			//[t51]

  /// Open large object with given oid
  /** Convert combination of a transaction and object identifier into a
   * large object identity.  Does not affect the database.
   * @param T the transaction in which the object is to be accessed
   * @param O object identifier for the given object
   * @param mode access mode, defaults to ios_base::in | ios_base::out
   */
  largeobjectaccess(dbtransaction &T, 
		    oid O,
		    openmode mode = 
			PGSTD::ios::in | 
			PGSTD::ios::out);				//[t52]

  /// Open given large object
  /** Open a large object with the given identity for reading and/or writing
   * @param T the transaction in which the object is to be accessed
   * @param O identity for the large object to be accessed
   * @param mode access mode, defaults to ios_base::in | ios_base::out
   */
  largeobjectaccess(dbtransaction &T, 
		    largeobject O,
		    openmode mode = PGSTD::ios::in | PGSTD::ios::out);	//[t50]

  /// Import large object from a local file and open it
  /** Creates a large object containing the data found in the given file.
   * @param T the backend transaction in which the large object is to be created
   * @param File a filename on the client program's filesystem
   * @param mode access mode, defaults to ios_base::in | ios_base::out
   */
  largeobjectaccess(dbtransaction &T, 
		    const PGSTD::string &File,
		    openmode mode = 
			PGSTD::ios::in | PGSTD::ios::out);		//[t55]

  ~largeobjectaccess() { close(); }

  /// Object identifier
  /** The number returned by this function identifies the large object in the
   * database we're connected to.
   */
  using largeobject::id;

  /// Export large object's contents to a local file
  /** Writes the data stored in the large object to the given file.
   * @param File a filename on the client's filesystem
   */
  void to_file(const PGSTD::string &File) const 			//[t54]
  { 
    largeobject::to_file(m_Trans, File); 
  }

#ifdef PQXX_BROKEN_USING_DECL
  /// Export large object's contents to a local file
  /** Writes the data stored in the large object to the given file.
   * @param T the transaction in which the object is to be accessed
   * @param File a filename on the client's filesystem
   */
  void to_file(dbtransaction &T, const PGSTD::string &F) const
  	{ largeobject::to_file(T, F); }
#else
  using largeobject::to_file;
#endif


  /// Write data to large object
  /** If not all bytes could be written, an exception is thrown.
   * @param Buf the data to write
   * @param Len the number of bytes from Buf to write
   */
  void write(const char Buf[], size_type Len);				//[t51]

  /// Write string to large object
  /** If not all bytes could be written, an exception is thrown.
   * @param Buf the data to write; no terminating zero is written
   */
  void write(const PGSTD::string &Buf)					//[t50]
  	{ write(Buf.c_str(), Buf.size()); }

  /// Read data from large object
  /** Returns the number of bytes read, which may be less than the number of
   * bytes requested if the end of the large object is reached.  Throws an
   * exception if an error occurs while reading.
   * @param Buf the location to store the read data in
   * @param Len the number of bytes to try and read
   */
  size_type read(char Buf[], size_type Len);				//[t50]

  /// Seek in large object's data stream
  /** Returns the new position in the large object.  Throws an exception if an
   * error occurs.
   */
  size_type seek(size_type dest, seekdir dir);				//[t51]

  /// Seek in large object's data stream
  /** Does not throw exception in case of error; inspect return value and errno
   * instead.
   * Returns new position in large object, or -1 if an error occurred.
   * @param dest offset to go to
   * @param dir origin to which dest is relative: ios_base::beg (from beginning 
   *        of the object), ios_base::cur (from current access position), or
   *        ios_base;:end (from end of object)
   */
  pos_type cseek(off_type dest, seekdir dir) throw ();			//[t50]
    
  /// Write to large object's data stream
  /** Does not throw exception in case of error; inspect return value and errno
   * instead.
   * Returns number of bytes actually written, or -1 if an error occurred.
   * @param Buf bytes to write
   * @param Len number of bytes to write
   */
  off_type cwrite(const char Buf[], size_type Len) throw ();		//[t50]

  /// Read from large object's data stream
  /** Does not throw exception in case of error; inspect return value and errno
   * instead.
   * Returns number of bytes actually read, or -1 if an error occurred.
   * @param Buf area where bytes should be stored
   * @param Len number of bytes to read
   */
  off_type cread(char Buf[], size_type Len) throw ();			//[t50]

  using largeobject::remove;

  using largeobject::operator==;
  using largeobject::operator!=;
  using largeobject::operator<;
  using largeobject::operator<=;
  using largeobject::operator>;
  using largeobject::operator>=;

private:
  PGSTD::string Reason() const;
  PGconn *RawConnection() { return largeobject::RawConnection(m_Trans); }

  void open(openmode mode);
  void close();

  dbtransaction &m_Trans;
  int m_fd;

  // Not allowed:
  largeobjectaccess();
  largeobjectaccess(const largeobjectaccess &);
  largeobjectaccess operator=(const largeobjectaccess &);
};


/// Streambuf to use large objects in standard I/O streams
/** The standard streambuf classes provide uniform access to data storage such
 * as files or string buffers, so they can be accessed using standard input or
 * output streams.  This streambuf implementation provides similar access to
 * large objects, so they can be read and written using the same stream classes.
 * 
 * @warning This class may not work properly in compiler environments that don't
 * fully support Standard-compliant streambufs, such as g++ 2.95 or older.
 */
template<typename CHAR=char, typename TRAITS=PGSTD::char_traits<CHAR> >
  class largeobject_streambuf :
#ifdef PQXX_HAVE_STREAMBUF
    public PGSTD::basic_streambuf<CHAR, TRAITS>
#else
    public PGSTD::streambuf
#endif
{
  typedef long size_type;
public:
  typedef CHAR   char_type;
  typedef TRAITS traits_type;
  typedef typename traits_type::int_type int_type;
#ifdef PQXX_HAVE_STREAMBUF
  typedef typename traits_type::pos_type pos_type;
  typedef typename traits_type::off_type off_type;
#else
  typedef streamoff off_type;
  typedef streampos pos_type;
#endif
  typedef largeobjectaccess::openmode openmode;
  typedef largeobjectaccess::seekdir seekdir;

  largeobject_streambuf(dbtransaction &T,
			largeobject O,
			openmode mode = in | out,
			size_type BufSize=512) : 			//[t48]
    m_BufSize(BufSize),
    m_Obj(T, O),
    m_G(0),
    m_P(0)
  {
    initialize(mode);
  }

  largeobject_streambuf(dbtransaction &T,
			oid O,
			openmode mode = in | out,
			size_type BufSize=512) : 			//[t48]
    m_BufSize(BufSize),
    m_Obj(T, O),
    m_G(0),
    m_P(0)
  {
    initialize(mode);
  }

  virtual ~largeobject_streambuf()
  {
    delete [] m_P;
    delete [] m_G;
  }


#ifdef PQXX_HAVE_STREAMBUF
protected:
#endif
  virtual int sync()
  {
    // setg() sets eback, gptr, egptr
    setg(eback(), eback(), egptr());
    return overflow(EoF());
  }

protected:
  virtual pos_type seekoff(off_type offset, 
			   seekdir dir,
			   openmode mode = in|out)
  {
    if (!mode) {}	// Quench "unused parameter" warning
    return AdjustEOF(m_Obj.cseek(offset, dir));
  }

  virtual pos_type seekpos(pos_type pos, 
			   openmode mode = in|out)
  {
    if (!mode) {}	// Quench "unused parameter" warning
    return AdjustEOF(m_Obj.cseek(pos, PGSTD::ios::beg));
  }

  virtual int_type overflow(int_type ch = EoF())
  {
    char *const pp = pptr();
    if (!pp) return EoF();
    char *const pb = pbase();
    int_type res = 0;

    if (pp > pb) res = AdjustEOF(m_Obj.cwrite(pb, pp-pb));
    setp(m_P, m_P + m_BufSize);

    // Write that one more character, if it's there.
    if (ch != EoF())
    {
      *pptr() = char(ch);
      pbump(1);
    }
    return res;
  }

  virtual int_type underflow()
  {
    if (!gptr()) return EoF();
    char *const eb = eback();
    const int res = AdjustEOF(m_Obj.cread(eback(), m_BufSize));
    setg(eb, eb, eb + ((res==EoF()) ? 0 : res));
    return (!res || (res == EoF())) ? EoF() : *eb;
  }

private:
  /// Shortcut for traits_type::eof()
  static int_type EoF() { return traits_type::eof(); }

  /// Helper: change error position of -1 to EOF (probably a no-op)
  static PGSTD::streampos AdjustEOF(int pos)
  {
    return (pos == -1) ? EoF() : pos;
  }

  void initialize(openmode mode)
  {
    if (mode & PGSTD::ios::in) 
    {
      m_G = new char_type[m_BufSize];
      setg(m_G, m_G, m_G);
    }
    if (mode & PGSTD::ios::out)
    {
      m_P = new char_type[m_BufSize];
      setp(m_P, m_P + m_BufSize);
    }
  }

  const size_type m_BufSize;
  largeobjectaccess m_Obj;

  // Get & put buffers
  char_type *m_G, *m_P;
};


/// Input stream that gets its data from a large object
/** Use this class exactly as you would any other istream to read data from a
 * large object.  All formatting and streaming operations of std::istream are 
 * supported.  What you'll typically want to use, however, is the ilostream
 * typedef (which defines a basic_ilostream for char).  This is similar to how
 * e.g. std::ifstream relates to std::basic_ifstream.
 *
 * Currently only works for <char, std::char_traits<char> >.
 */
template<typename CHAR=char, typename TRAITS=PGSTD::char_traits<CHAR> > 
  class basic_ilostream :
#ifdef PQXX_HAVE_STREAMBUF
    public PGSTD::basic_istream<CHAR, TRAITS>
#else
    public PGSTD::istream
#endif
{
#ifdef PQXX_HAVE_STREAMBUF
  typedef PGSTD::basic_istream<CHAR, TRAITS> super;
#else
  typedef PGSTD::istream super;
#endif

public:
  typedef CHAR char_type;
  typedef TRAITS traits_type;
  typedef typename traits_type::int_type int_type;
  typedef typename traits_type::pos_type pos_type;
  typedef typename traits_type::off_type off_type;

  /// Create a basic_ilostream
  /** @param T transaction in which this stream is to exist
   * @param O a large object to access
   * @param BufSize size of buffer to use internally (optional)
   */
  basic_ilostream(dbtransaction &T, 
                  largeobject O, 
		  largeobject::size_type BufSize=512) :			//[t57]
    super(&m_Buf),
    m_Buf(T, O, in, BufSize) 
  { 
  }

  /// Create a basic_ilostream
  /** @param T transaction in which this stream is to exist
   * @param O identifier of a large object to access
   * @param BufSize size of buffer to use internally (optional)
   */
  basic_ilostream(dbtransaction &T, 
                  oid O, 
		  largeobject::size_type BufSize=512) :			//[t48]
    super(&m_Buf),
    m_Buf(T, O, in, BufSize) 
  { 
  }

private:
  largeobject_streambuf<CHAR,TRAITS> m_Buf;
};

typedef basic_ilostream<char> ilostream;


/// Output stream that writes data back to a large object
/** Use this class exactly as you would any other ostream to write data to a
 * large object.  All formatting and streaming operations of std::ostream are 
 * supported.  What you'll typically want to use, however, is the olostream
 * typedef (which defines a basic_olostream for char).  This is similar to how 
 * e.g. std::ofstream is related to std::basic_ofstream.
 *
 * Currently only works for <char, std::char_traits<char> >.
 */
template<typename CHAR=char, typename TRAITS=PGSTD::char_traits<CHAR> > 
  class basic_olostream : 
#ifdef PQXX_HAVE_STREAMBUF
    public PGSTD::basic_ostream<CHAR, TRAITS>
#else
    public PGSTD::ostream
#endif
{
#ifdef PQXX_HAVE_STREAMBUF
  typedef PGSTD::basic_ostream<CHAR, TRAITS> super;
#else
  typedef PGSTD::ostream super;
#endif
public:
  typedef CHAR char_type;
  typedef TRAITS traits_type;
  typedef typename traits_type::int_type int_type;
  typedef typename traits_type::pos_type pos_type;
  typedef typename traits_type::off_type off_type;

  /// Create a basic_olostream
  /** @param T transaction in which this stream is to exist
   * @param O a large object to access
   * @param BufSize size of buffer to use internally (optional)
   */
  basic_olostream(dbtransaction &T, 
                  largeobject O,
		  largeobject::size_type BufSize=512) :			//[t48]
    super(&m_Buf),
    m_Buf(T, O, out, BufSize) 
  { 
  }

  /// Create a basic_olostream
  /** @param T transaction in which this stream is to exist
   * @param O a large object to access
   * @param BufSize size of buffer to use internally (optional)
   */
  basic_olostream(dbtransaction &T, 
      		  oid O,
		  largeobject::size_type BufSize=512) :			//[t57]
    super(&m_Buf),
    m_Buf(T, O, out, BufSize) 
  { 
  }

  ~basic_olostream() 
  { 
#ifdef PQXX_HAVE_STREAMBUF
    m_Buf.pubsync(); m_Buf.pubsync(); 
#else
    m_Buf.sync(); m_Buf.sync();
#endif
  }

private:
  largeobject_streambuf<CHAR,TRAITS> m_Buf;
};

typedef basic_olostream<char> olostream;


/// Stream that reads and writes a large object
/** Use this class exactly as you would a std::iostream to read data from, or
 * write data to a large object.  All formatting and streaming operations of 
 * std::iostream are supported.  What you'll typically want to use, however, is
 * the lostream typedef (which defines a basic_lostream for char).  This is 
 * similar to how e.g. std::fstream is related to std::basic_fstream.
 *
 * Currently only works for <char, std::char_traits<char> >.
 */
template<typename CHAR=char, typename TRAITS=PGSTD::char_traits<CHAR> > 
  class basic_lostream :
#ifdef PQXX_HAVE_STREAMBUF
    public PGSTD::basic_iostream<CHAR, TRAITS>
#else
    public PGSTD::iostream
#endif
{
#ifdef PQXX_HAVE_STREAMBUF
  typedef PGSTD::basic_iostream<CHAR, TRAITS> super;
#else
  typedef PGSTD::iostream super;
#endif

public:
  typedef CHAR char_type;
  typedef TRAITS traits_type;
  typedef typename traits_type::int_type int_type;
  typedef typename traits_type::pos_type pos_type;
  typedef typename traits_type::off_type off_type;

  /// Create a basic_lostream
  /** @param T transaction in which this stream is to exist
   * @param O a large object to access
   * @param BufSize size of buffer to use internally (optional)
   */
  basic_lostream(dbtransaction &T, 
      		 largeobject O,
		 largeobject::size_type BufSize=512) :			//[t59]
    super(&m_Buf),
    m_Buf(T, O, in | out, BufSize) 
  { 
  }

  /// Create a basic_lostream
  /** @param T transaction in which this stream is to exist
   * @param O a large object to access
   * @param BufSize size of buffer to use internally (optional)
   */
  basic_lostream(dbtransaction &T, 
      		 oid O,
		 largeobject::size_type BufSize=512) :			//[t59]
    super(&m_Buf),
    m_Buf(T, O, in | out, BufSize) 
  { 
  }

  ~basic_lostream() 
  { 
#ifdef PQXX_HAVE_STREAMBUF
    m_Buf.pubsync(); m_Buf.pubsync(); 
#else
    m_Buf.sync(); m_Buf.sync();
#endif
  }

private:
  largeobject_streambuf<CHAR,TRAITS> m_Buf;
};

typedef basic_lostream<char> lostream;

}


