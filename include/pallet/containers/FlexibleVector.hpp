#include "pallet/variant.hpp"
#include "pallet/containers/StaticVector.hpp"

namespace pallet::containers {

template <class T, size_t N>
class FlexibleVector {
  Variant<StaticVector<T, N>,
          std::vector<T>> vec;

public:

  void pop_back() {
    return pallet::visit([&](auto&& vec) {
      return vec.pop_back();
    }, vec);
  }

  size_t size() const {
    return pallet::visit([&](const auto& vec) {
      return vec.size();
    }, vec);
  }

  template <class... Args>
  T& emplace_back(Args&&... args) {
    pallet::visit(overloads {
        [&](StaticVector<T, N>& vec) {
          if (vec.capacity() == this->size()) {
            // can't use this one anymore
            std::vector<T> newvec;
            newvec.reserve(vec.size() + 1);
            for (size_t i = 0; i < vec.size(); i++) {
              newvec.push_back(std::move(vec[i]));
            }
            this->vec = std::move(newvec);
            // clear old vec
            vec.clear();
          }
        },
          [&](std::vector<T>& vec) {
            // don't do anything
            (void)vec;
          }}, vec);

    return pallet::visit([&](auto&& vec) -> T& {
      return vec.emplace_back(std::forward<Args>(args)...);
    }, vec);
  };

  T& back() {
    return pallet::visit([&](auto&& vec) -> T& {
      return vec.back();
    }, vec);
  };

  T& operator[](size_t n) {
    return pallet::visit([&](auto&& vec) -> T& {
      return vec[n];
    }, vec);
  }

  T* begin() {
    return pallet::visit([&](auto&& vec) {
      return &(vec[0]);
    }, vec);
  }

  T* end() {
    return pallet::visit([&](auto&& vec) {
      return &vec[0] + vec.size();
    }, vec);
  }

  void clear() {
    return pallet::visit([&](auto&& vec) {
      return vec.clear();
    }, vec);
  }
};

}
