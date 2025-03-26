#include <cstdio>
#include <utility>
#include <memory>
#include <type_traits>

#include "memory.hpp"

// #include "Platform.hpp"
// #include "Clock.hpp"
// #include "time.hpp"
// #include "containers/containerUtils.hpp"



// template <class ObjectType, class... Args>
// UniqueResource(auto&& cleanup, Args... args) ->
//   UniqueResource<ObjectType, std::remove_cvref<decltype(cleanup)>>;

// template <class... Args>
// auto createUniqueResource(Args... args)


int main()

{

  auto v1 = pallet::UniqueResource(std::make_unique<int>(3));
  auto v2 = std::move(v1);
  auto v3 = std::move(v2);
  printf("%d: value %lu\n", **v3, sizeof(decltype(v1)));
  
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
