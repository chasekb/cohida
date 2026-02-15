#ifndef FILE_WRITER_H
#define FILE_WRITER_H

#include "models/DataPoint.h"
#include <string>
#include <vector>

namespace data {

class FileWriter {
public:
  static void write_csv(const std::vector<models::CryptoPriceData> &data,
                        const std::string &filename);
  static void write_json(const std::vector<models::CryptoPriceData> &data,
                         const std::string &filename);
  static void write_json(const models::SymbolInfo &info,
                         const std::string &filename);
  static void write_json(const std::vector<models::SymbolInfo> &infos,
                         const std::string &filename);
};

} // namespace data

#endif // FILE_WRITER_H
