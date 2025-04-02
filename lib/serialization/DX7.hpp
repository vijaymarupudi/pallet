#pragma once

#include "../DX7.hpp"
#include "cereal/types/array.hpp"
#include "cereal/types/utility.hpp"

template <class Archive>
void serialize(Archive& ar, DX7Operator& op) {
  ar(op.env, op.ratio, op.detune, op.amp);
}

template <class Archive>
void serialize(Archive& ar, DX7Params& params) {
  ar(params.operators, params.algorithm, params.feedback);
}
