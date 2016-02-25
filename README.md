# pqxx
libpqxx speedup:  COPY (SELECT ...) support to the tablereader

Usage:
  ```
          pqxx::connection c(env);
          pqxx::work w(c);
          pqxx::tablereader tr(w, "copy (select md5, id from malwares order by md5 limit 10000000) to stdout", true);
          std::vector<std::string> line;
          while(tr >> line){
               std::cout << line[0] << "\t" << line[1] << std::endl;
               line.clear();
          }
