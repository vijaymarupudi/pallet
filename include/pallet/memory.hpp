#pragma once

#include <utility>
#include <memory>
#include <type_traits>

#include "types.hpp"

namespace pallet {

template <class T>
concept isMovable = std::is_move_constructible_v<T> &&
  std::is_move_assignable_v<T>;


template <class T>
concept isNothrowMovable = std::is_nothrow_move_constructible_v<T> &&
  std::is_nothrow_move_assignable_v<T>;

namespace detail {

  template <class T, class ObjT>
  concept hasValidityChecks = requires(T& t, ObjT& obj) {
    { t.isValid(obj) } -> std::convertible_to<bool>;
    t.setValid(obj, false);
  };

  template <class T, class ObjT>
  concept hasDestructor = requires(T& t, ObjT& obj) {
    t(obj);
  };



  template <class T>
  struct UniqueResourceDefaultPropertiesType
  {
    bool valid = false;
    bool isValid(const T&) const {return valid;}
    void setValid(T&, bool val) {valid = val;}
  };

  template <class UserProvidedPropertiesType, class ObjectType>
  struct PropertiesType;

  template <class UserProvidedPropertiesType, class ObjectType>
  requires (hasValidityChecks<UserProvidedPropertiesType, ObjectType>)
  struct PropertiesType<UserProvidedPropertiesType, ObjectType> : public UserProvidedPropertiesType {};

  template <class UserProvidedPropertiesType, class ObjectType>
  requires (!hasValidityChecks<UserProvidedPropertiesType, ObjectType>)
  struct PropertiesType<UserProvidedPropertiesType, ObjectType> : public UniqueResourceDefaultPropertiesType<ObjectType> {};

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
class UniqueResource : private detail::PropertiesType<UserPropertiesType, ObjectType> {

 public:

  UniqueResource(const UniqueResource& other) = delete;
  UniqueResource(UniqueResource& other) = delete;
  UniqueResource& operator=(const UniqueResource& other) = delete;
  UniqueResource& operator=(UniqueResource& other) = delete;

  UniqueResource()
    : PropertiesType{}, object{} {}

  template <class Arg>
  UniqueResource(Arg&& value)
    : PropertiesType{}, object{std::forward<Arg>(value)} {}

  template <class Arg, class PropertiesArg>
  UniqueResource(Arg&& value, PropertiesArg&& properties)
    : PropertiesType{std::forward<PropertiesArg>(properties)},
      object{std::forward<Arg>(value)} {}

  UniqueResource(UniqueResource&& other) noexcept(_nonthrowing_move)
    : PropertiesType{std::move(other)}, object{std::move(other.object)}
  { other.setValid(other.object, false); }

  UniqueResource& operator=(UniqueResource&& other) noexcept(_nonthrowing_move) {
    PropertiesType::operator=(std::move(other));
    std::swap(object, other.object);
    return *this;
  }

  ObjectType& operator*() noexcept {return object;}
  const ObjectType& operator*() const noexcept {return object;}
  ObjectType* operator->() noexcept {return &object;}
  const ObjectType* operator->() const noexcept  {return &object;}

  void cleanup() noexcept(detail::isDestructorNoExcept<PropertiesType, ObjectType>())
  {
    if (this->isValid(object)) {
      if constexpr (hasDestructor) {
        PropertiesType::operator()(object);
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
  using PropertiesType = detail::PropertiesType<UserPropertiesType, ObjectType>;
  static constexpr bool _nonthrowing_move =
    std::is_nothrow_move_constructible_v<PropertiesType> &&
    std::is_nothrow_move_assignable_v<PropertiesType> &&
    std::is_nothrow_move_constructible_v<ObjectType> &&
    std::is_nothrow_move_assignable_v<ObjectType>;
  static constexpr bool hasDestructor = detail::hasDestructor<PropertiesType, ObjectType>;

};

template <class Arg, class PropertiesArg>
UniqueResource(Arg&& value, PropertiesArg&& properties) -> UniqueResource<std::remove_cvref_t<Arg>, std::remove_cvref_t<PropertiesArg>>;

template <class Arg>
UniqueResource(Arg&& value) -> UniqueResource<std::remove_cvref_t<Arg>>;

static_assert(!std::is_copy_constructible_v<UniqueResource<int>> &&
              !std::is_copy_assignable_v<UniqueResource<int>> &&
              isNothrowMovable<UniqueResource<int>>);
}
