#pragma once

namespace pallet {
template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };
}

