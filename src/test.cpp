#include "../lib/luaHelper.hpp"
#include "../lib/luaHelper/LuaClass.hpp"
#include "pallet/functional.hpp"
#include <type_traits>
#include <new>


struct TestType {
  int x;
  int getX() const {
    return x;
  }
};


void other(lua_State*) {
  // pallet::luaHelper::LuaClass<TestType> cls(L, "cls");
  // cls.addConstructor([&](int x) {
  //   return TestType{x};
  // });
  // cls.addMethod<&TestType::getX>("getX");
  // cls.pushObject(L, 32);
  // const char name[] = "hello!";
  // const char* another = name;
  // pallet::luaHelper::push(L, another);
}

int main() {

}
