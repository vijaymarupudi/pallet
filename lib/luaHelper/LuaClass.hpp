#pragma once

#include "concepts.hpp"
#include "LuaTable.hpp"
#include "pallet/functional.hpp"
#include "finalizable.hpp"
#include "type_traits.hpp"
#include <memory>

namespace pallet::luaHelper {
template <class T>
class LuaClass {
  static constexpr inline const char key = 'k';
  static constexpr inline const void* const classKey = &key;
  lua_State* L;
  const char* name;
public:
  RegistryIndex metatableRef;

  static inline LuaClass& create(lua_State* L, const char* name) {
    return createFinalizableUserData<LuaClass>(L, PrivateTag, L, name);
  }

  static inline LuaClass& from(lua_State* L) {
    return *pull<LuaClass*>(L, classKey);
  }

  LuaClass(PrivateTagType, lua_State* L,
           const char* name) : L(L),
                               name(name) {

    // store pointer to self
    store(L, classKey, this);

    // Create metatable
    auto metatable = LuaTable::create(L);
    metatableRef = store(L, metatable);
    
    
    if constexpr (!std::is_trivially_destructible_v<T>) {
      metatable.rawset("__gc", [](T* ptr) {
        ptr->~T();
      });
    }

    metatable.rawset("__index", metatable);
    lua_pop(L, 1); // the metatable
  }

  LuaClass(LuaClass&& other) : L(other.L),
                               metatableRef(std::exchange(other.metatableRef, nullptr)) {}

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
                return val;
              }
            }
            }));

    lua_pop(L, 1); // the table
  }

  template <class... Args>
  T& pushObject(lua_State* L, Args&&... args) {
    T* storage = reinterpret_cast<T*>(lua_newuserdatauv(L, sizeof(T), 0));
    luaHelper::push(L, metatableRef);
    lua_setmetatable(L, -2);
    auto ptr = std::construct_at(storage, std::forward<Args>(args)...);
    return *ptr;
  }

  template <auto memberFunc>
  void addMethodBatch(StackIndex index, const char* name, constant_wrapper<memberFunc>) {
    auto metatable = LuaTable{L, index};

    auto action = [&]<class... A>() {
      metatable.rawset(name, [](T* ptr, A... args) {
        return (ptr->*memberFunc)(std::forward<A>(args)...);
      });
    };

    (pallet::overloaded {
      [&]<class R, class L, class... A>(R(L::*)(A...) const) {
        action.template operator()<A...>();
      },
        [&]<class R, class L, class... A>(R(L::*)(A...)) {
          action.template operator()<A...>();
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
    free(L, metatableRef);
    free(L, classKey);
  }


  struct LuaTraits {
    static inline void push(lua_State* L, T value) {
      auto& cls = LuaClass::from(L);
      cls.pushObject(L, std::move(value));
    }
  };

};

template <class T>
struct LuaTraits<LuaClass<T>> {
  static inline void push(lua_State* L, LuaClass<T>& cls) {
    luaHelper::push(L, cls.metatableRef);
  }
};

}
