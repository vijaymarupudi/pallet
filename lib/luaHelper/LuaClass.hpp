#pragma once

#include "../luaHelper.hpp"
#include "concepts.hpp"
#include "LuaTable.hpp"

namespace pallet::luaHelper {
template <class T>
class LuaClass {
  lua_State* L;
  RegistryIndex metatableRef;
  const char* name;
public:

  LuaClass(lua_State* L, const char* name) : L(L), name(name) {

    // Create metatable
    auto metatable = LuaTable::create(L);
    metatableRef = store(L, metatable);

    if constexpr (!std::is_trivially_destructible_v<T>) {
      metatable.rawset("__gc", [](T* ptr) {
        ptr->~T();
      });
    }

    metatable.rawset("__index", metatable);
  }

  LuaClass(LuaClass&& other) : L(other.L),
                               metatableRef(std::exchange(other.metatableRef, nullptr)) {}

  template <class FunctionType>
  requires (concepts::Stateless<std::remove_cvref_t<FunctionType>> &&
            concepts::HasCallOperator<std::remove_cvref_t<FunctionType>>)

  void addConstructor(FunctionType&&) {
    
    auto func = ([&]<class... A>(T(FunctionType::*)(A...) const) -> lua_CFunction {

      return [](lua_State* L) -> int {

        LuaClass<T>& cls = *luaHelper::pull<LuaClass*>(L, lua_upvalueindex(1));

        std::apply([&](auto&&... args) {
          auto lambdaPtr = static_cast<FunctionType*>(nullptr);
          auto&& obj = lambdaPtr->operator()(std::forward<decltype(args)>(args)...);
          cls.pushObject(L, std::forward<decltype(obj)>(obj));
        }, checkedPullMultiple<A...>(L, 1));

        return 1;
      };

    })(&FunctionType::operator());

    auto stackTop = lua_gettop(L);
    // Used by the closure function
    luaHelper::push(L, this);
    
    lua_pushcclosure(L, func, 1);
    auto closureFunction = StackIndex{lua_gettop(L)};
    
    auto metatable = LuaTable::from(L, metatableRef);
    metatable.rawset("new", closureFunction);
 
    lua_settop(L, stackTop);
  }

  template <class... Args>
  T& pushObject(lua_State* L, Args&&... args) {
    void* storage = lua_newuserdatauv(L, sizeof(T), 0);
    lua_rawgeti(L, LUA_REGISTRYINDEX, metatableRef.getIndex());
    lua_setmetatable(L, -2);
    auto ptr = ::new (storage) T (std::forward<Args>(args)...);
    return *ptr;
  }

  template <auto memberFunc>
  void addMethodBatch(StackIndex index, const char* name) {
    auto metatable = LuaTable{L, index};
    (pallet::overloads {
      [&]<class R, class... A>(R(T::*)(A...)) {
        metatable.rawset(name, [](T* ptr, A... args) {
          return (ptr->*memberFunc)(std::move(args)...);
        });
      },
      [&]<class R, class... A>(R(T::*)(A...) const) {
        metatable.rawset(name, [](T* ptr, A... args) {
          return (ptr->*memberFunc)(std::move(args)...);
        });
      }
    })(memberFunc);
  }

  StackIndex beginBatch() {
    return LuaTable::from(L, metatableRef);
  }

  void endBatch(StackIndex index) {
    lua_settop(L, (index.getIndex()) - 1);
  }

  template <auto memberFunc>
  void addMethod(const char* name) {
    auto b = beginBatch();
    addMethodBatch<memberFunc>(b, name);
    endBatch(b);
  }

  ~LuaClass() {
    if (metatableRef) {
      free(L, metatableRef);
      // luaL_unref(L, LUA_REGISTRYINDEX, metatableRef.getIndex());
    }
  }
};

}
