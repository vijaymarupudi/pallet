#pragma once

#include <utility>
#include <memory>
#include <type_traits>

namespace pallet {
namespace detail {
template <class T>
struct UniqueResourceDefaultCleanup {
  void operator()(T& x) {(void)x;}
};
}

template <class ObjectType,
          class D = detail::UniqueResourceDefaultCleanup<ObjectType>>
class UniqueResource : private D {

  static constexpr bool _nonthrowing_move =
    std::is_nothrow_move_constructible_v<D> &&
    std::is_nothrow_move_assignable_v<D> &&
    std::is_nothrow_move_constructible_v<ObjectType> &&
    std::is_nothrow_move_assignable_v<ObjectType>;
    

 public:

  ObjectType object;
  bool valid = false;

  UniqueResource(const UniqueResource& other) = delete;
  UniqueResource(UniqueResource& other) = delete;
  UniqueResource& operator=(const UniqueResource& other) = delete;
  UniqueResource& operator=(UniqueResource& other) = delete;

  template <class Arg>
  UniqueResource(Arg&& value)
    : D{}, object{std::forward<Arg>(value)}, valid{true} {}  
  
  template <class Arg, class DArg>
  UniqueResource(Arg&& value, DArg&& destructor)
    : D{std::forward<DArg>(destructor)}, object{std::forward<Arg>(value)}, valid{true} {
  }

  UniqueResource(UniqueResource&& other) noexcept(_nonthrowing_move)
    : D{std::move(other)},
      object{std::move(other.object)}, valid(other.valid) {
    other.valid = false;
  }

  UniqueResource& operator=(UniqueResource&& other) noexcept(_nonthrowing_move) {
    D::operator=(other);
    std::swap(valid, other.valid);
    std::swap(object, other.object);
  }

  ObjectType& operator*() noexcept {
    return object;
  }

  const ObjectType& operator*() const noexcept {
    return object;
  }

  ObjectType* operator->() noexcept {
    return object;
  }

  const ObjectType* operator->() const noexcept  {
    return object;
  }

  ~UniqueResource() {
    if (valid) {
      D::operator()(object);
    }
  }
};

template <class Arg, class DArg>
UniqueResource(Arg&& value, DArg&& destructor) -> UniqueResource<std::remove_cvref_t<Arg>, std::remove_cvref_t<DArg>>;

template <class Arg>
UniqueResource(Arg&& value) -> UniqueResource<std::remove_cvref_t<Arg>>;

static_assert(!std::is_copy_constructible_v<UniqueResource<int>> &&
              !std::is_copy_assignable_v<UniqueResource<int>> &&
              std::is_nothrow_move_constructible_v<UniqueResource<int>> &&
              std::is_nothrow_move_assignable_v<UniqueResource<int>>);
}
