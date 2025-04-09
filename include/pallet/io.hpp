#pragma once

#include <string>
#include "pallet/error.hpp"

namespace pallet {

Result<std::string> readFile(const char* name);

}

