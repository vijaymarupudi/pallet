#pragma once

#include <vector>
#include <algorithm>
#include <type_traits>

namespace vjcontainers {

  template <class T, class V>
  using binary_operation_return_type_t = decltype(std::declval<T>()(std::declval<V>(), std::declval<V>()));

  template <class T, class V, class Z = void>
  constexpr bool is_compare_named_requirement = false;

  template <class T, class V>
  constexpr bool is_compare_named_requirement<T, V, std::void_t<binary_operation_return_type_t<T, V>,
                                                                std::enable_if_t<std::is_convertible_v<binary_operation_return_type_t<T, V>, bool>, void>>> = true;

  template <class T, class Z = void>
  constexpr bool is_container_v = false;

  template <class T>
  constexpr bool is_container_v<T, std::enable_if_t<std::is_same_v<decltype(std::declval<T>().size()), typename T::size_type>, void>> = true;

  template <class Container, class Compare>
  class PriorityQueueCompare : public Compare {
  public:
    Container cont;
    template <class ContIn>
    PriorityQueueCompare(ContIn&& cont, Compare compare) : Compare(compare), cont(std::forward<ContIn>(cont)) {}
    PriorityQueueCompare(Compare compare) : Compare(compare) {}
  };

  template <class T, class Container = std::vector<T>, class Compare = std::less<T>>
  class PriorityQueue {
    PriorityQueueCompare<Container, Compare> compare;
    std::add_lvalue_reference_t<Container> getCont() {
      return compare.cont;
    }

    std::add_lvalue_reference_t<std::add_const_t<Container>> getCont() const {
      return compare.cont;
    }
  public:
    template<class InputCont, class CmpFunc, class = std::enable_if_t<is_compare_named_requirement<CmpFunc, T>>>
    PriorityQueue(InputCont&& cont, CmpFunc compare) : compare{std::forward<InputCont>(cont), compare} {}


    template<class CmpFunc, class = std::enable_if_t<is_compare_named_requirement<CmpFunc, T>>>
    PriorityQueue(CmpFunc compare) : compare{compare} {}
    template<class InputCont, class = std::enable_if_t<is_container_v<InputCont>>>
    PriorityQueue(InputCont&& cont) : compare{std::forward<InputCont>(cont), Compare()} {}


    PriorityQueue() : compare{Compare()} {}

    template <class In>
    void push(In&& in) {
      auto cmp = [&](const T& a, const T& b) -> bool {
        return this->compare(a, b);
      };
      this->getCont().push_back(std::forward<In>(in));
      std::push_heap(this->getCont().begin(), this->getCont().end(), cmp);
    }

    template <class... Args>
    void emplace(Args&&... in) {
      auto cmp = [&](const T& a, const T& b) -> bool {
        return this->compare(a, b);
      };
      this->getCont().emplace_back(std::forward<Args>(in)...);
      std::push_heap(this->getCont().begin(), this->getCont().end(), cmp);
    }

    size_t size() const {
      return this->getCont().size();
    }

    T& top() {
      return this->getCont()[0];
    }

    void pop() {
      auto cmp = [&](const T& a, const T& b) -> bool {
        return this->compare(a, b);
      };
      std::pop_heap(this->getCont().begin(), this->getCont().end(), cmp);
      this->getCont().pop_back();
    }
  };
}
