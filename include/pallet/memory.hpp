#pragma once

#include <utility>
#include <memory>
#include <type_traits>

#include "pallet/types.hpp"

namespace pallet {

template <class T>
concept isMovable = std::is_move_constructible_v<T> &&
  std::is_move_assignable_v<T>;


template <class T>
concept isNothrowMovable = std::is_nothrow_move_constructible_v<T> &&
  std::is_nothrow_move_assignable_v<T>;

namespace detail {

  template <class PropertiesType, class ObjectType>
  concept hasValidityChecks = requires(PropertiesType t, ObjectType obj) {
    { t.isValid(obj) } -> std::convertible_to<bool>;
    t.setValid(obj, false);
  };

  template <class PropertiesType, class ObjectType>
  concept hasDestructor = requires(PropertiesType props, ObjectType obj) {
    props(obj);
  };

template <class UserProvidedPropertiesType, class ObjectType>
struct UniqueResourceDefaultPropertiesType : public UserProvidedPropertiesType
  {
    bool valid = true;
    bool isValid(const ObjectType&) const {return valid;}
    void setValid(ObjectType&, bool val) {valid = val;}
  };

  template <class UserProvidedPropertiesType, class ObjectType>
  struct GeneratePropertiesType
    : public UniqueResourceDefaultPropertiesType<UserProvidedPropertiesType, ObjectType> {};

  template <class UserProvidedPropertiesType, class ObjectType>
  requires hasValidityChecks<UserProvidedPropertiesType, ObjectType>
  struct GeneratePropertiesType<UserProvidedPropertiesType, ObjectType>
    : public UserProvidedPropertiesType {};

  template <class T, class O>
  consteval bool isDestructorNoExcept() {
    if constexpr (!hasDestructor<T, O>) { return true; }
    else {
      return noexcept(std::declval<T&>()(std::declval<O&>()));
    }
  }
}

template <class ObjectType,
          class UserPropertiesType = pallet::Blank>
class UniqueResource :
    private detail::GeneratePropertiesType<UserPropertiesType,
                                           ObjectType> {

  
  using SuperClass = detail::GeneratePropertiesType<UserPropertiesType,
                                                    ObjectType>;

public:
  
  static constexpr bool hasDestructor =
    detail::hasDestructor<SuperClass, ObjectType>;
  static constexpr bool hasValidityChecks =
    detail::hasValidityChecks<UserPropertiesType, ObjectType>;

  UniqueResource(const UniqueResource& other) = delete;
  UniqueResource(UniqueResource& other) = delete;
  UniqueResource& operator=(const UniqueResource& other) = delete;
  UniqueResource& operator=(UniqueResource& other) = delete;

  UniqueResource()
    : SuperClass{}, object{} {}

  template <class Arg>
  UniqueResource(Arg&& value)
    : SuperClass{}, object{std::forward<Arg>(value)} {}

  template <class Arg, class PropertiesArg>
  UniqueResource(Arg&& value, PropertiesArg&& properties)
    : SuperClass{std::forward<PropertiesArg>(properties)},
      object{std::forward<Arg>(value)} {}

  UniqueResource(UniqueResource&& other) noexcept(_nonthrowing_move)
    : SuperClass{std::move(other)}, object{std::move(other.object)}
  { other.setValid(other.object, false); }

  UniqueResource& operator=(UniqueResource&& other) noexcept(_nonthrowing_move) {
    SuperClass::operator=(std::move(other));
    std::swap(object, other.object);
    return *this;
  }

  ObjectType& operator*() noexcept {return object;}
  const ObjectType& operator*() const noexcept {return object;}
  ObjectType* operator->() noexcept {return &object;}
  const ObjectType* operator->() const noexcept  {return &object;}

  void cleanup() noexcept(detail::isDestructorNoExcept<SuperClass, ObjectType>())
  {
    if (this->isValid(object)) {
      if constexpr (hasDestructor) {
        SuperClass::operator()(object);
      }
      this->setValid(object, false);
    }
  }

  ~UniqueResource() noexcept(noexcept(this->cleanup())) {
    this->cleanup();
  }

protected:
  ObjectType object;

private:

  static constexpr bool _nonthrowing_move =
    std::is_nothrow_move_constructible_v<SuperClass> &&
    std::is_nothrow_move_assignable_v<SuperClass> &&
    std::is_nothrow_move_constructible_v<ObjectType> &&
    std::is_nothrow_move_assignable_v<ObjectType>;

};

template <class Arg, class PropertiesArg>
UniqueResource(Arg&& value, PropertiesArg&& properties) -> UniqueResource<std::remove_cvref_t<Arg>, std::remove_cvref_t<PropertiesArg>>;

template <class Arg>
UniqueResource(Arg&& value) -> UniqueResource<std::remove_cvref_t<Arg>>;

static_assert(!std::is_copy_constructible_v<UniqueResource<int>> &&
              !std::is_copy_assignable_v<UniqueResource<int>> &&
              isNothrowMovable<UniqueResource<int>>);
}
