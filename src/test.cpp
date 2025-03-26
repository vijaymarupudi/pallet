#include <cstdio>
#include <algorithm>
#include <utility>
#include <cstring>

#include "Platform.hpp"
#include "Clock.hpp"
#include "time.hpp"
#include "containers/containerUtils.hpp"

namespace detail {
template <class T>
struct UniqueResourceDefaultCleanup {
  void operator()(T& x) {(void)x;}
};
}

template <class ObjectType,
          class D = detail::UniqueResourceDefaultCleanup<ObjectType>>
class UniqueResource : private D {

  static constexpr bool _nonthrowing_move = std::is_nothrow_move_constructible_v<D> &&
    std::is_nothrow_move_assignable_v<D> &&
    std::is_nothrow_move_constructible_v<ObjectType> &&
    std::is_nothrow_move_assignable_v<ObjectType>;
    
public:
  
  bool valid = false;
  ObjectType object;

  UniqueResource(const UniqueResource& other) = delete;
  UniqueResource(UniqueResource& other) = delete;
  UniqueResource& operator=(const UniqueResource& other) = delete;
  UniqueResource& operator=(UniqueResource& other) = delete;

  template <class Arg>
  UniqueResource(Arg&& value)
    : D{}, valid{true}, object{std::forward<Arg>(value)} {}  
  
  template <class Arg, class DArg>
  UniqueResource(Arg&& value, DArg&& destructor)
    : D{std::forward<DArg>(destructor)}, valid{true}, object{std::forward<Arg>(value)} {
  }

  UniqueResource(UniqueResource&& other) noexcept(_nonthrowing_move)
    : D{std::move(other)},
      valid(other.valid), object{std::move(other.object)}  {
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

// template <class ObjectType, class... Args>
// UniqueResource(auto&& cleanup, Args... args) ->
//   UniqueResource<ObjectType, std::remove_cvref<decltype(cleanup)>>;

// template <class... Args>
// auto createUniqueResource(Args... args)


int main()

{

  auto v1 = UniqueResource(3, [](int& v){(void)v;
      printf("Cleaning up!\n");});
  auto v2 = std::move(v1);
  auto v3 = std::move(v2);
  printf("%d: value\n", *v3);
  // [](int v) {(void)v;}, 3
  // auto v1 = UniqueResource(3, [](int& v){(void)v;});
  // (void)v1;
  // auto v2 = v1;
  // (void)v2;
  // int n = 1;
  // auto v2 = UniqueResource(n, [](int& v){(void)v;});
  // // auto v2 = v1;
  // printf("%d\n", v1.valid);
  // (void)v2;
  // auto v3 = v2;
  
  // auto platform = pallet::LinuxPlatform();
  // auto clock = pallet::Clock(platform);

  // while (true) {
  //   platform.loopIter();
  // }

  // Wow var;
  return 0;
}
