#include <system_error>

#include "pallet/io.hpp"
#include "pallet/utils.hpp"

namespace pallet {

Result<std::string> readFile(const char* name) {

  FILE* f = fopen(name, "r");
  if (!f) {
    return pallet::error(std::system_category().default_error_condition(errno));
  }

  pallet::Defer _ ([&]() {
    fclose(f);
  });

  std::string ret;
  size_t amtRead;
  char buf[8192];
  do {
    amtRead = fread(buf, 1, sizeof(buf), f);
    ret.append(buf, amtRead);
  } while (amtRead == sizeof(buf));

  if (ferror(f)) {
    return pallet::error(std::system_category().default_error_condition(errno));
  } else {
    // eof here
    return ret;
  }
}

}
