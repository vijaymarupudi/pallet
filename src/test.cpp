#include <cstdio>
#include <utility>
#include <type_traits>
#include <print>
#include "memory.hpp"

// #include "Platform.hpp"
// #include "Clock.hpp"
// #include "time.hpp"
// #include "containers/containerUtils.hpp"

template <class T>
struct A {
  bool valid = 0;
};

template <class T>
requires (std::is_trivially_copyable_v<T>)
struct A<T> {
  
};

// template <class ObjectType, class... Args>
// UniqueResource(auto&& cleanup, Args... args) ->
//   UniqueResource<ObjectType, std::remove_cvref<decltype(cleanup)>>;

// template <class... Args>
// auto createUniqueResource(Args... args)

class X {
  ~X() {
    
  }
};

template <class T>
class B : A<T> {
  T member;
};

int main()

{
  std::println("size: {}, {}", sizeof(B<int>), sizeof(B<X>));
  
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
