#pragma once

namespace pallet {
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
}

