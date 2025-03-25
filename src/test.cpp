#include <cstdio>
#include <algorithm>
#include <utility>
#include <cstring>

#include "Platform.hpp"
#include "Clock.hpp"
#include "time.hpp"
#include "containers/containerUtils.hpp"

template <class T>
class UniqueResourceDefaultCleanup {
  void operator()(const T& x) {}
};

// = UniqueResourceDefaultCleanup<T>
template <class T, class D  = UniqueResourceDefaultCleanup<T>>
class UniqueResource : private D {
  void _init_value(T* value) {
    std::memcpy(memberSpace.ptr(), value, sizeof(T));
  }
  
public:
  bool valid = false;
  pallet::containers::Space<T> memberSpace;

  // template <class Arg>
  // UniqueResource(Arg&& value)
  //   : D{}, valid{true} { _init_value(&value); }

  UniqueResource(T value)
    : D{}, valid{true} { _init_value(&value); }

  UniqueResource(T& value)
    : D{}, valid{true} { _init_value(&value); }
  
  UniqueResource(T&& value)
    : D{}, valid{true} { _init_value(&value); }

  UniqueResource(const UniqueResource& other) = delete;
  
  UniqueResource& operator=(const UniqueResource& other) = delete;

  // UniqueResource(T value)
  //   : D{}, valid{true} { _init_value(&value); }


  // template <class DArg>
  // UniqueResource(T value, DArg&& destructor)
  //   : D{std::forward<DArg>(destructor)}, valid{true} {
  //   _init_value(&value);
  // }

  template <class Arg, class DArg>
  UniqueResource(Arg&& value, DArg&& destructor)
    : D{std::forward<DArg>(destructor)}, valid{true} {
    _init_value(&value);
  }

  UniqueResource(UniqueResource&& other) noexcept
    : D{std::move(other)}, valid(other.valid)  {
    other.valid = false;
    if (this->valid) {
      std::memcpy(memberSpace.ptr(), other.memberSpace.ptr(),
                  sizeof(T));
    }
  }

  UniqueResource& operator=(UniqueResource&& other) noexcept {
    D::operator=(other);
    std::swap(valid, other.valid);
    std::swap(memberSpace, other.memberSpace);
  }

  ~UniqueResource() {
    if (valid) {
      this->operator()(*memberSpace.ptr());
      memberSpace.destroy();
    }
  }
};

template <class T, class DArg>
UniqueResource(T value, DArg&& destructor) -> UniqueResource<T, std::remove_cvref_t<DArg>>;

// template <class T, class... Args>
// UniqueResource(auto&& cleanup, Args... args) ->
//   UniqueResource<T, std::remove_cvref<decltype(cleanup)>>;

// template <class... Args>
// auto createUniqueResource(Args... args)


int main()

{

  // [](int v) {(void)v;}, 3
  auto v1 = UniqueResource(3, [](int& v){(void)v;});
  int n = 1;
  auto v2 = UniqueResource(n, [](int& v){(void)v;});
  // auto v2 = v1;
  printf("%d\n", v1.valid);
  (void)v2;
  
  // auto platform = pallet::LinuxPlatform();
  // auto clock = pallet::Clock(platform);

  // while (true) {
  //   platform.loopIter();
  // }

  // Wow var;
  return 0;
}
