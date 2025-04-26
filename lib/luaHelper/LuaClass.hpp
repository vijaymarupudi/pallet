#pragma once

#include "concepts.hpp"
#include "LuaTable.hpp"
#include "pallet/functional.hpp"
#include "finalizable.hpp"
#include "type_traits.hpp"

namespace pallet::luaHelper {
template <class T>
class LuaClass {
  lua_State* L;
  const char* name;
public:
  RegistryIndex metatableRef;

  static LuaClass& create(lua_State* L, const char* name) {
    return createFinalizableUserData<LuaClass>(L, PrivateTag, L, name);
  }

  LuaClass(PrivateTagType, lua_State* L, const char* name) : L(L), name(name) {

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

  // template <class FunctionType>
  // requires (concepts::Stateless<std::remove_cvref_t<FunctionType>> &&
  //           concepts::HasCallOperator<std::remove_cvref_t<FunctionType>>)

  // void addConstructor(FunctionType&&) {
    
  //   auto func = ([&]<class... A>(T(FunctionType::*)(A...) const) -> lua_CFunction {

  //     return [](lua_State* L) -> int {

  //       LuaClass<T>& cls = *luaHelper::pull<LuaClass*>(L, lua_upvalueindex(1));

  //       return std::apply([&](auto&&... args) -> int {
  //         auto lambda = std::decay_t<FunctionType>{};
  //         auto&& obj = std::move(lambda)(std::forward<decltype(args)>(args)...);
  //         cls.pushObject(L, std::forward<decltype(obj)>(obj));
  //         return 1;
  //       }, checkedPullMultiple<A...>(L, 1));
        
  //     };

  //   })(&FunctionType::operator());

  //   auto stackTop = lua_gettop(L);
  //   // Used by the closure function
  //   luaHelper::push(L, this);
    
  //   lua_pushcclosure(L, func, 1);
  //   auto closureFunction = StackIndex{lua_gettop(L)};
    
  //   auto metatable = LuaTable::from(L, metatableRef);
  //   metatable.rawset("new", closureFunction);
 
  //   lua_settop(L, stackTop);
  // }

  void addStaticMethod(const char* name, auto&& function) {
    auto table = LuaTable::from(L, metatableRef);
    table.rawset(name, luaClosureChain(std::forward<decltype(function)>(function), pallet::overloaded {
        [](lua_State* L) {
          (void)L;
          return;
        },
          [this](lua_State* L, auto&& val) {
            if constexpr (std::same_as<std::decay_t<decltype(val)>, T>) {
              this->pushObject(L, std::forward<decltype(val)>(val));
              return ReturnStackTop{};
            } else {
              return std::forward<decltype(val)>(val);
            }
          }
          }));
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
  void addMethodBatch(StackIndex index, const char* name, constant_wrapper<memberFunc>) {
    auto metatable = LuaTable{L, index};
    (pallet::overloaded {
      [&]<class R, class... A>(R(T::*)(A...)) {
        metatable.rawset(name, [](T* ptr, A... args) {
          return (ptr->*memberFunc)(std::forward<A>(args)...);
        });
      },
      [&]<class R, class... A>(R(T::*)(A...) const) {
        metatable.rawset(name, [](T* ptr, A... args) {
          return (ptr->*memberFunc)(std::forward<A>(args)...);
        });
      }
    })(memberFunc);
  }

  void addMethodBatch(StackIndex index, const char* name, concepts::StatelessLambdaLike auto&& function) {
    auto metatable = LuaTable{L, index};

    auto action = [&]<class R, class L, class... A>() {
      metatable.rawset(name, [](T* ptr, A... args) {
        static_cast<L*>(nullptr)->operator()(ptr, std::forward<A>(args)...);
      });
    };
    
    (pallet::overloaded {
      [&]<class R, class L, class... A>(R(L::*)(T*, A...)) {
        action.template operator()<R, L, A...>();
      },
        [&]<class R, class L, class... A>(R(L::*)(T*, A...) const) {
          action.template operator()<R, L, A...>();
        }
    })(&std::remove_reference<decltype(function)>::type::operator());

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
    }
  }
};

template <class T>
struct LuaTraits<LuaClass<T>> {
  static inline void push(lua_State* L, LuaClass<T>& cls) {
    luaHelper::push(L, cls.metatableRef);
  }
};

}
