/*-------------------------------------------------------------------------
 *
 *   FILE
 *	util.cxx
 *
 *   DESCRIPTION
 *      Various utility functions for libpqxx
 *
 * Copyright (c) 2003-2008, Jeroen T. Vermeulen <jtv@xs4all.nl>
 *
 * See COPYING for copyright license.  If you did not receive a file called
 * COPYING with this source code, please notify the distributor of this mistake,
 * or contact the author.
 *
 *-------------------------------------------------------------------------
 */
#include "pqxx/compiler-internal.hxx"

#include <cmath>
#include <cstring>

#ifdef PQXX_HAVE_LIMITS
#include <limits>
#endif

#ifdef PQXX_HAVE_LOCALE
#include <locale>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <cerrno>
#include <new>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

#include "libpq-fe.h"

#include "pqxx/except"
#include "pqxx/util"

using namespace PGSTD;
using namespace pqxx::internal;


const char pqxx::internal::sql_begin_work[] = "BEGIN",
      pqxx::internal::sql_commit_work[] = "COMMIT",
      pqxx::internal::sql_rollback_work[] = "ROLLBACK";


namespace
{
// It turns out that NaNs are pretty hard to do portably.  If the appropriate
// C++ traits functions are not available, C99 defines a NAN macro (also widely
// supported in other dialects, I believe) and some functions that can do the
// same work.  But if none of those are available, we need to resort to
// compile-time "0/0" expressions.  Most compilers won't crash while compiling
// those anymore, but there may be unneeded warnings--which are almost as bad.
template<typename T> void set_to_NaN(T &);

#if defined(PQXX_HAVE_QUIET_NAN)
template<typename T> inline void set_to_NaN(T &t)
	{ t = numeric_limits<T>::quiet_NaN(); }
#elif defined(PQXX_HAVE_C_NAN)
template<typename T> inline void set_to_NaN(T &t) { t = NAN; }
#elif defined(PQXX_HAVE_NAN)
template<> inline void set_to_NaN(float &t) { t = fnan(""); }
template<> inline void set_to_NaN(double &t) { t = nan(""); }
#ifdef PQXX_HAVE_LONG_DOUBLE
template<> inline void set_to_NaN(long double &t) { t = lnan(""); }
#endif
#else
const float nan_f(0.0/0.0);
template<> inline void set_to_NaN(float &t) { t = nan_f; }
const double nan_d(0.0/0.0);
template<> inline void set_to_NaN(double &t) { t = nan_d; }
#ifdef PQXX_HAVE_LONG_DOUBLE
const long double nan_ld(0.0/0.0);
template<> inline void set_to_NaN(long double &t) { t = nan_ld; }
#endif
#endif

// TODO: This may need tweaking for various compilers.
template<typename T> inline void set_to_Inf(T &t, int sign=1)
{
#ifdef PQXX_HAVE_LIMITS
  T value = numeric_limits<T>::infinity();
#else
  T value = INFINITY;
#endif
  if (sign < 0) value = -value;
  t = value;
}


/// For use in string parsing: add new numeric digit to intermediate value
template<typename L, typename R>
  inline L absorb_digit(L value, R digit) throw ()
{
  return L(L(10)*value + L(digit));
}


template<typename T> void from_string_signed(const char Str[], T &Obj)
{
  int i = 0;
  T result = 0;

  if (!isdigit(Str[i]))
  {
    if (Str[i] != '-')
      throw pqxx::failure("Could not convert string to integer: '" +
	string(Str) + "'");

    for (++i; isdigit(Str[i]); ++i)
    {
      const T newresult = absorb_digit(result, -digit_to_number(Str[i]));
      if (newresult > result)
        throw pqxx::failure("Integer too small to read: " + string(Str));
      result = newresult;
    }
  }
  else for (; isdigit(Str[i]); ++i)
  {
    const T newresult = absorb_digit(result, digit_to_number(Str[i]));
    if (newresult < result)
      throw pqxx::failure("Integer too large to read: " + string(Str));
    result = newresult;
  }

  if (Str[i])
    throw pqxx::failure("Unexpected text after integer: '" + string(Str) + "'");

  Obj = result;
}

template<typename T> void from_string_unsigned(const char Str[], T &Obj)
{
  int i = 0;
  T result = 0;

  if (!isdigit(Str[i]))
    throw pqxx::failure("Could not convert string to unsigned integer: '" +
	string(Str) + "'");

  for (; isdigit(Str[i]); ++i)
  {
    const T newres = absorb_digit(result, digit_to_number(Str[i]));
    if (newres < result)
      throw pqxx::failure("Unsigned integer too large to read: " + string(Str));

    result = newres;
  }

  if (Str[i])
    throw pqxx::failure("Unexpected text after integer: '" + string(Str) + "'");

  Obj = result;
}


bool valid_infinity_string(const char str[])
{
  // TODO: Also accept less sensible case variations.
  return
	strcmp("infinity", str) == 0 ||
	strcmp("Infinity", str) == 0 ||
	strcmp("INFINITY", str) == 0;
}


/* These are hard.  Sacrifice performance of specialized, nonflexible,
 * non-localized code and lean on standard library.  Some special-case code
 * handles NaNs.
 */
template<typename T> inline void from_string_float(const char Str[], T &Obj)
{
  bool ok = false;
  T result;

  switch (Str[0])
  {
  case 'N':
  case 'n':
    // Accept "NaN," "nan," etc.
    ok = ((Str[1]=='A'||Str[1]=='a') && (Str[2]=='N'||Str[2]=='n') && !Str[3]);
    set_to_NaN(result);
    break;

  case 'I':
  case 'i':
    ok = valid_infinity_string(Str);
    set_to_Inf(result);
    break;

  default:
    if (Str[0] == '-' && valid_infinity_string(&Str[1]))
    {
      ok = true;
      set_to_Inf(result, -1);
    }
    else
    {
      stringstream S(Str);
#if defined(PQXX_HAVE_IMBUE)
      S.imbue(locale("C"));
#endif
      ok = (S >> result);
    }
    break;
  }

  if (!ok)
    throw pqxx::failure("Could not convert string to numeric value: '" +
	string(Str) + "'");

  Obj = result;
}

template<typename T> inline string to_string_unsigned(T Obj)
{
  if (!Obj) return "0";

  // Every byte of width on T adds somewhere between 3 and 4 digits to the
  // maximum length of our decimal string.
  char buf[4*sizeof(T)+1];

  char *p = &buf[sizeof(buf)];
  *--p = '\0';
  while (Obj > 0)
  {
    *--p = number_to_digit(int(Obj%10));
    Obj /= 10;
  }
  return p;
}

template<typename T> inline string to_string_fallback(T Obj)
{
  stringstream S;
#ifdef PQXX_HAVE_IMBUE
  S.imbue(locale("C"));
#endif

// Provide enough precision.
#ifdef PQXX_HAVE_LIMITS
  // Kirit reports getting two more digits of precision than
  // numeric_limits::digits10 would give him, so we try not to make him lose
  // those last few bits.
  S.precision(numeric_limits<T>::digits10 + 2);
#else
  // Guess: enough for an IEEE 754 double-precision value.
  S.precision(16);
#endif
  S << Obj;
  return S.str();
}


template<typename T> inline bool is_NaN(T Obj)
{
  return
#if defined(PQXX_HAVE_C_ISNAN)
    isnan(Obj);
#elif defined(PQXX_HAVE_LIMITS)
    !(Obj <= Obj+numeric_limits<T>::max());
#else
    !(Obj <= Obj + 1000);
#endif
}


template<typename T> inline bool is_Inf(T Obj)
{
  return
#if defined(PQXX_HAVE_C_ISINF)
    isinf(Obj);
#else
    Obj >= Obj+1 && Obj <= 2*Obj && Obj >= 2*Obj;
#endif
}


template<typename T> inline string to_string_float(T Obj)
{
  // TODO: Omit this special case if NaN is output as "nan"/"NAN"/"NaN"
#ifndef PQXX_HAVE_NAN_OUTPUT
  if (is_NaN(Obj)) return "nan";
#endif
  // TODO: Omit this special case if infinity is output as "infinity"
#ifndef PQXX_HAVE_INF_OUTPUT
  if (is_Inf(Obj)) return Obj > 0 ? "infinity" : "-infinity";
#endif
  return to_string_fallback(Obj);
}


template<typename T> inline string to_string_signed(T Obj)
{
  if (Obj < 0)
  {
    // Remember--the smallest negative number for a given two's-complement type
    // cannot be negated.
#if PQXX_HAVE_LIMITS
    const bool negatable = (Obj != numeric_limits<T>::min());
#else
    T Neg(-Obj);
    const bool negatable = Neg > 0;
#endif
    if (negatable)
      return '-' + to_string_unsigned(-Obj);
    else
      return to_string_fallback(Obj);
  }

  return to_string_unsigned(Obj);
}

} // namespace


namespace pqxx
{

namespace internal
{
void throw_null_conversion(const PGSTD::string &type)
{
  throw conversion_error("Attempt to convert null to " + type);
}
} // namespace pqxx::internal

void string_traits<bool>::from_string(const char Str[], bool &Obj)
{
  bool OK, result=false;

  switch (Str[0])
  {
  case 0:
    result = false;
    OK = true;
    break;

  case 'f':
  case 'F':
    result = false;
    OK = !(Str[1] &&
	   (strcmp(Str+1, "alse") != 0) &&
	   (strcmp(Str+1, "ALSE") != 0));
    break;

  case '0':
    {
      int I;
      string_traits<int>::from_string(Str, I);
      result = (I != 0);
      OK = ((I == 0) || (I == 1));
    }
    break;

  case '1':
    result = true;
    OK = !Str[1];
    break;

  case 't':
  case 'T':
    result = true;
    OK = !(Str[1] &&
	   (strcmp(Str+1, "rue") != 0) &&
	   (strcmp(Str+1, "RUE") != 0));
    break;

  default:
    OK = false;
  }

  if (!OK)
    throw argument_error("Failed conversion to bool: '" + string(Str) + "'");

  Obj = result;
}

string string_traits<bool>::to_string(bool Obj)
{
  return Obj ? "true" : "false";
}

void string_traits<short>::from_string(const char Str[], short &Obj)
{
  from_string_signed(Str, Obj);
}

string string_traits<short>::to_string(short Obj)
{
  return to_string_signed(Obj);
}

void string_traits<unsigned short>::from_string(
	const char Str[],
	unsigned short &Obj)
{
  from_string_unsigned(Str, Obj);
}

string string_traits<unsigned short>::to_string(unsigned short Obj)
{
  return to_string_unsigned(Obj);
}

void string_traits<int>::from_string(const char Str[], int &Obj)
{
  from_string_signed(Str, Obj);
}

string string_traits<int>::to_string(int Obj)
{
  return to_string_signed(Obj);
}

void string_traits<unsigned int>::from_string(
	const char Str[],
	unsigned int &Obj)
{
  from_string_unsigned(Str, Obj);
}

string string_traits<unsigned int>::to_string(unsigned int Obj)
{
  return to_string_unsigned(Obj);
}

void string_traits<long>::from_string(const char Str[], long &Obj)
{
  from_string_signed(Str, Obj);
}

string string_traits<long>::to_string(long Obj)
{
  return to_string_signed(Obj);
}

void string_traits<unsigned long>::from_string(
	const char Str[],
	unsigned long &Obj)
{
  from_string_unsigned(Str, Obj);
}

string string_traits<unsigned long>::to_string(unsigned long Obj)
{
  return to_string_unsigned(Obj);
}

#ifdef PQXX_HAVE_LONG_LONG
void string_traits<long long>::from_string(const char Str[], long long &Obj)
{
  from_string_signed(Str, Obj);
}

string string_traits<long long>::to_string(long long Obj)
{
  return to_string_signed(Obj);
}

void string_traits<unsigned long long>::from_string(
	const char Str[],
	unsigned long long &Obj)
{
  from_string_unsigned(Str, Obj);
}

string string_traits<unsigned long long>::to_string(unsigned long long Obj)
{
  return to_string_unsigned(Obj);
}
#endif

void string_traits<float>::from_string(const char Str[], float &Obj)
{
  from_string_float(Str, Obj);
}

string string_traits<float>::to_string(float Obj)
{
  return to_string_float(Obj);
}

void string_traits<double>::from_string(const char Str[], double &Obj)
{
  from_string_float(Str, Obj);
}

string string_traits<double>::to_string(double Obj)
{
  return to_string_float(Obj);
}

#ifdef PQXX_HAVE_LONG_DOUBLE
void string_traits<long double>::from_string(const char Str[], long double &Obj)
{
  from_string_float(Str, Obj);
}

string string_traits<long double>::to_string(long double Obj)
{
  return to_string_float(Obj);
}
#endif

} // namespace pqxx


namespace
{
size_t pqxx_strnlen(const char s[], size_t max)
{
#if defined(PQXX_HAVE_STRNLEN)
  return strnlen(s,max);
#else
  size_t len;
  for (len=0; len<max && s[len]; ++len) ;
  return len;
#endif
}
} // namespace


void pqxx::internal::freemem_notif(pqxx::internal::pq::PGnotify *p) throw ()
{
#ifdef PQXX_HAVE_PQFREENOTIFY
  PQfreeNotify(p);
#else
  freepqmem(p);
#endif
}


pqxx::internal::refcount::refcount() :
  m_l(this),
  m_r(this)
{
}


pqxx::internal::refcount::~refcount()
{
  loseref();
}


void pqxx::internal::refcount::makeref(refcount &rhs) throw ()
{
  // TODO: Make threadsafe
  m_l = &rhs;
  m_r = rhs.m_r;
  m_l->m_r = m_r->m_l = this;
}


bool pqxx::internal::refcount::loseref() throw ()
{
  // TODO: Make threadsafe
  const bool result = (m_l == this);
  m_r->m_l = m_l;
  m_l->m_r = m_r;
  m_l = m_r = this;
  return result;
}


string pqxx::internal::namedclass::description() const
{
  try
  {
    string desc = classname();
    if (!name().empty()) desc += " '" + name() + "'";
    return desc;
  }
  catch (const exception &)
  {
    // Oops, string composition failed!  Probably out of memory.
    // Let's try something easier.
  }
  return name().empty() ? classname() : name();
}


void pqxx::internal::CheckUniqueRegistration(const namedclass *New,
    const namedclass *Old)
{
  if (!New)
    throw internal_error("NULL pointer registered");
  if (Old)
  {
    if (Old == New)
      throw usage_error("Started twice: " + New->description());
    throw usage_error("Started " + New->description() + " "
		      "while " + Old->description() + " still active");
  }
}


void pqxx::internal::CheckUniqueUnregistration(const namedclass *New,
    const namedclass *Old)
{
  if (New != Old)
  {
    if (!New)
      throw usage_error("Expected to close " + Old->description() + ", "
			"but got NULL pointer instead");
    if (!Old)
      throw usage_error("Closed while not open: " + New->description());
    throw usage_error("Closed " + New->description() + "; "
		      "expected to close " + Old->description());
  }
}


void pqxx::internal::freepqmem(void *p)
{
#ifdef PQXX_HAVE_PQFREEMEM
  PQfreemem(p);
#else
  free(p);
#endif
}


void pqxx::internal::sleep_seconds(int s)
{
  if (s <= 0) return;

#if defined(PQXX_HAVE_SLEEP)
  // Use POSIX.1 sleep() if available
  sleep(s);
#elif defined(_WIN32)
  // Windows has its own Sleep(), which speaks milliseconds
  Sleep(s*1000);
#else
  // If all else fails, use select() on nothing and specify a timeout
  fd_set F;
  FD_ZERO(&F);
  struct timeval timeout;
  timeout.tv_sec = s;
  timeout.tv_usec = 0;
  if (select(0, &F, &F, &F, &timeout) == -1) switch (errno)
  {
  case EINVAL:	// Invalid timeout
	throw range_error("Invalid timeout value: " + to_string(s));
  case EINTR:	// Interrupted by signal
	break;
  case ENOMEM:	// Out of memory
	throw bad_alloc();
  default:
    throw internal_error("select() failed for unknown reason");
  }
#endif
}


namespace
{

void cpymsg(char buf[], const char input[], size_t buflen) throw ()
{
#if defined(PQXX_HAVE_STRLCPY)
  strlcpy(buf, input, buflen);
#else
  strncpy(buf, input, buflen);
  if (buflen) buf[buflen-1] = '\0';
#endif
}

// Single Unix Specification version of strerror_r returns result code
const char *strerror_r_result(int sus_return, char buf[], size_t len) throw ()
{
  switch (sus_return)
  {
  case 0: break;
  case -1: cpymsg(buf, "Unknown error", len); break;
  default:
    cpymsg(buf,
	  "Unexpected result from strerror_r()!  Is it really SUS-compliant?",
	  len);
    break;
  }

  return buf;
}

// GNU version of strerror_r returns error string (which may be anywhere)
const char *strerror_r_result(const char gnu_return[], char[], size_t) throw ()
{
  return gnu_return;
}
}


const char *pqxx::internal::strerror_wrapper(int err, char buf[], size_t len)
	throw ()
{
  if (!buf || len <= 0) return "No buffer provided for error message!";

  const char *res = buf;

#if !defined(PQXX_HAVE_STRERROR_R)
  cpymsg(buf, strerror(err), len);
#else
  // This will pick the appropriate strerror_r() subwrapper using overloading
  // (an idea first suggested by Bart Samwel.  Thanks a bundle, Bart!)
  res = strerror_r_result(strerror_r(err,buf,len), buf, len);
#endif
  return res;
}

