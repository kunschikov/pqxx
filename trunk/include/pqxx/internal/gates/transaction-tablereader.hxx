#include <pqxx/internal/callgate.hxx>

namespace pqxx
{
namespace internal
{
namespace gate
{
class PQXX_PRIVATE transaction_tablereader : callgate<transaction_base>
{
  friend class pqxx::tablereader;

  transaction_tablereader(reference x) : super(x) {}

  void BeginCopyRead(const std::string &table, const std::string &columns, const std::string &query)
       { home().BeginCopyRead(table, columns, query); }

  bool ReadCopyLine(std::string &line) { return home().ReadCopyLine(line); }
};
} // namespace pqxx::internal::gate
} // namespace pqxx::internal
} // namespace pqxx
