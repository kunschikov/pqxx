/*-------------------------------------------------------------------------
 *
 *   FILE
 *	pg_result.h
 *
 *   DESCRIPTION
 *      definitions for the Pg::Result class and support classes.
 *   Pg::Result represents the set of result tuples from a database query
 *
 * Copyright (c) 2001, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 *-------------------------------------------------------------------------
 */
#ifndef PG_RESULT_H
#define PG_RESULT_H

#include <iterator>
#include <stdexcept>

#include "pg_util.h"

// TODO: Support postgres arrays
// TODO: Cursor-based bidirectional iterators for hidden incremental retrieval

namespace Pg
{

// Query or command result set.  This behaves pretty much as a container (as 
// defined by the C++ standard library) and provides random access const
// iterators to iterate over its tuples.  A tuple can also be accessed by
// indexing a Result R by the tuple's zero-based number:
//
//	for (Result::size_type i=0; i < R.size(); ++i) Process(R[i]);
//
class Result
{
public:
  Result() : m_Result(0), m_Refcount(0) {}
  Result(const Result &rhs) : m_Result(0), m_Refcount(0) { MakeRef(rhs); }
  ~Result() { LoseRef(); }
  
  Result &operator=(const Result &);

  typedef Result_size_type size_type;
  class Field;

  // A Tuple is really a reference to one entry in a Result.  It also acts as a
  // container mapping column numbers or names to Field values (see below):
  //
  // 	cout << Tuple["date"].c_str() << ": " << Tuple["name"].c_str() << endl;
  //
  // The fields in a Tuple can not currently be iterated over.
  // TODO: Field iterators
  class Tuple
  {
  public:
    typedef Tuple_size_type size_type;
    Tuple(const Result *r, size_type i) : m_Home(r), m_Index(i) {}
    ~Tuple() {} // Yes Scott Meyers, you're absolutely right[1]

    Field operator[](size_type i) const { return Field(*this, i); }
    Field operator[](const char[]) const;
    Field operator[](PGSTD::string s) const {return operator[](s.c_str());}
    Field at(size_type) const;
    Field at(const char[]) const;
    Field at(PGSTD::string s) const { return at(s.c_str()); }

    size_type size() const { return PQnfields(Result_c_ptr()); }

    Result::size_type Row() const { return m_Index; }

  protected:
    const PGresult *Result_c_ptr() const { return m_Home->c_ptr(); }
    const Result *m_Home;
    Result::size_type m_Index;
  };


  // A Field is one entry in a Tuple.  It represents an actual value in the
  // result set, and can be converted to various types.
  class Field : private Tuple
  {
  public:
    Field(const Tuple &R, Tuple::size_type C) : Tuple(R), m_Col(C) {}

    const char *c_str() const;	// Read as plain C string
    const char *name() const;	// Column name

    // Read value into Obj; or leave Obj untouched & return false if null
    template<typename T> bool to(T &Obj) const
    {
      if (is_null())
	return false;

      try
      {
        FromString(c_str(), Obj);
      }
      catch (const PGSTD::exception &e)
      {
	throw PGSTD::runtime_error("Error reading field " + 
			         PGSTD::string(name()) +
				 ": " +
				 e.what());
      }
      return true;
    }

    // Read value into Obj; or use Default & return false if null
    template<typename T> bool to(T &Obj, const T &Default) const
    {
      const bool NotNull = to(Obj);
      if (!NotNull)
	Obj = Default;
      return NotNull;
    }

    bool is_null() const;

    int size() const;

  private:

    Tuple::size_type m_Col;
  };


  // A Result, once obtained, cannot be modified.  However its const_iterator
  // type can be used to inspect its Tuples without changing them.
  class const_iterator : 
    public PGSTD::iterator<PGSTD::random_access_iterator_tag, 
                         const Tuple,
                         Result::size_type>, 
    public Tuple
  {
  public:

    // The iterator "points to" its own Tuple, which is also itself.  This 
    // allows a Result to be addressed as a two-dimensional container without 
    // going through the intermediate step of dereferencing the iterator.  I 
    // hope this works out to be similar to C pointer/array semantics in useful 
    // cases[2].
    pointer operator->()  const { return this; }
    reference operator*() const { return *operator->(); }

    const_iterator operator++(int);
    const_iterator &operator++() { ++m_Index; return *this; }
    const_iterator operator--(int);
    const_iterator &operator--() { --m_Index; return *this; }

    const_iterator &operator+=(difference_type i) { m_Index+=i; return *this; }
    const_iterator &operator-=(difference_type i) { m_Index-=i; return *this; }

    bool operator==(const const_iterator &i) const {return m_Index==i.m_Index;}
    bool operator!=(const const_iterator &i) const {return m_Index!=i.m_Index;}
    bool operator<(const const_iterator &i) const   {return m_Index<i.m_Index;}
    bool operator<=(const const_iterator &i) const {return m_Index<=i.m_Index;}
    bool operator>(const const_iterator &i) const   {return m_Index>i.m_Index;}
    bool operator>=(const const_iterator &i) const {return m_Index>=i.m_Index;}

    const_iterator operator+(difference_type o) const
    {
      return const_iterator(m_Home, m_Index + o);
    }
    friend const_iterator operator+(difference_type o, const_iterator i)
    {
      return i + o;
    }

    const_iterator operator-(difference_type o) const
    {
      return const_iterator(m_Home, m_Index - o);
    }

    difference_type operator-(const_iterator i) const { return num()-i.num(); }

     Result::size_type num() const { return Row(); }

  private:
    friend class Result;
    const_iterator(const Result *r, Result::size_type i) : Tuple(r, i) {}
  };

  const_iterator begin() const { return const_iterator(this, 0); }
  const_iterator end() const { return const_iterator(this, size()); }
  // TODO: Handle reverse iterators

  size_type size() const { return m_Result ? PQntuples(m_Result) : 0; }
  bool empty() const { return !m_Result || !PQntuples(m_Result); }

  const Tuple operator[](size_type i) const { return Tuple(this, i); }
  const Tuple at(size_type i) const;

private:
  friend class Result::Tuple;
  const PGresult *c_ptr() const { return m_Result; }

  PGresult *m_Result;
  mutable int *m_Refcount;

  friend class Connection;
  explicit Result(PGresult *rhs) : m_Result(rhs), m_Refcount(0) {MakeRef(rhs);}
  Result &operator=(PGresult *);
  void CheckStatus() const;


  void MakeRef(PGresult *);
  void MakeRef(const Result &);
  void LoseRef() throw ();
};




template<> inline bool Result::Field::to(PGSTD::string &Obj) const
{
  if (is_null())
    return false;
  Obj = PGSTD::string(c_str());
  return true;
}

template<> inline bool Result::Field::to(const char *&Obj) const
{
  if (is_null()) 
    return false;
  Obj = c_str();
  return true;
}


} // namespace


/* 
[1] Scott Meyers, in one of his essential books, "Effective C++" and "More 
Effective C++", points out that it is good style to have any class containing 
a member of pointer type define its own destructor--just to show that it knows
what it is doing.  This helps prevent nasty memory leak / double deletion bugs
typically resulting from programmers' omission to deal with such issues in
their destructors.

The -Weffc++ option in gcc generates warnings for noncompliance with Scott's
style guidelines, and hence necessitates the definition of this destructor,\
trivial as it may be.

[2] IIRC Alex Stepanov, the inventor of the STL, once remarked that having
this as standard behaviour for pointers would be useful in some algorithms.
So even if this makes me look foolish, I would seem to be in distinguished 
company.
*/

#endif

