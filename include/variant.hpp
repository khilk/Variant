#pragma once

#include <cassert>
#include <cstddef>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>
// util::variant header-only implementation.

namespace util {

template <typename...>
class variant;

class bad_get : public std::bad_cast {
 public:
  const char* what() const noexcept override {
    return "util::bad_get exception";
  }
};

static constexpr std::size_t variant_npos = -1;

namespace details {
template <std::size_t arg1, std::size_t... others>
struct find_max;

template <std::size_t arg>
struct find_max<arg> {
  static constexpr std::size_t value = arg;
};

template <size_t arg1, size_t arg2, size_t... others>
struct find_max<arg1, arg2, others...> {
  static constexpr size_t value = arg1 >= arg2 ? find_max<arg1, others...>::value : find_max<arg2, others...>::value;
};

template <std::size_t i, typename Type1, typename... Types>
struct get_index;

template <std::size_t i, typename Type1, typename Type2, typename... Types>
struct get_index<i, Type1, Type2, Types...> {
  static constexpr size_t value = std::is_same<Type1, Type2>::value ? i : get_index<i + 1, Type1, Types...>::value;
};

template <std::size_t i, typename Type1>
struct get_index<i, Type1> {
  static constexpr size_t value = variant_npos;
};

template <std::size_t i, typename Type1, typename... Types>
struct get_conv_index;

template <std::size_t i, typename Type1, typename Type2, typename... Types>
struct get_conv_index<i, Type1, Type2, Types...> {
  static constexpr size_t value =
    std::is_convertible<Type1, Type2>::value ? i : get_conv_index<i + 1, Type1, Types...>::value;
};

template <std::size_t i, typename Type1>
struct get_conv_index<i, Type1> {
  static constexpr size_t value = variant_npos;
};

// clang-format off
template <typename Type, typename... Types>
constexpr std::size_t get_index_v =
  (std::is_same_v<Type, Types> || ...) ? get_index<0, Type, Types...>::value : get_conv_index<0, Type, Types...>::value;
// clang-format on

template <std::size_t index, typename... Types>
struct get_type;

template <std::size_t index, typename Type, typename... Types>
struct get_type<index, Type, Types...> {
  using type = typename get_type<index - 1, Types...>::type;
};

template <typename Type, typename... Types>
struct get_type<0, Type, Types...> {
  using type = Type;
};

template <size_t index>
struct get_type<index> {
  static_assert(index == -1, "Incorrent index");
};

template <typename Type>
void destroy(std::size_t, void* element) {
  reinterpret_cast<Type*>(element)->~Type();
}

template <typename Type1, typename Type2, typename... Types>
void destroy(std::size_t index, void* element) {
  if (index == 0) {
    reinterpret_cast<Type1*>(element)->~Type1();
  } else {
    destroy<Type2, Types...>(index - 1, element);
  }
}

template <bool Copy, typename Type>
void create(std::size_t index, void* to, void* from) {
  if (index != 0) {
    throw std::runtime_error("unable to create");
  }
  if constexpr (Copy) {
    new (to) Type(*reinterpret_cast<Type*>(from));
  } else {
    new (to) Type(std::move(*reinterpret_cast<Type*>(from)));
  }
}

template <bool Copy, typename Type1, typename Type2, typename... Types>
void create(std::size_t index, void* to, void* from) {
  if (index == 0) {
    if constexpr (Copy) {
      new (to) Type1(*reinterpret_cast<Type1*>(from));
    } else {
      new (to) Type1(std::move(*reinterpret_cast<Type1*>(from)));
    }
  } else {
    create<Copy, Type2, Types...>(index - 1, to, from);
  }
}
}  // namespace details

template <typename... Types>
void swap(variant<Types...>& lhs, variant<Types...>& rhs) noexcept(noexcept(lhs.swap(rhs))) {
  lhs.swap(rhs);
}

template <typename... Args>
class variant {
 public:
  variant() : index_{variant_npos}, hash_(-1) {
  }

  template <typename T, std::enable_if_t<!std::is_same_v<std::decay_t<T>, variant>, bool> = true>
  variant(T&& value) noexcept(std::is_nothrow_constructible_v<T>) {
    static_assert((std::is_same_v<T, Args> || ...) || (std::is_convertible_v<T, Args> || ...), "invalid parametr type");
    constexpr std::size_t index = details::get_index_v<T, Args...>;
    new (buffer_) typename details::get_type<index, Args...>::type{std::forward<T>(value)};
    index_ = index;
    hash_ = typeid(typename details::get_type<index, Args...>::type).hash_code();
  }

  variant(const variant& other) noexcept((... && std::is_nothrow_copy_constructible_v<Args>))
    : index_(other.index_), hash_(other.hash_) {
    if (!empty()) {
      details::create<true, const Args...>(index_, &buffer_,
                                           const_cast<void*>(static_cast<const void*>(&other.buffer_)));
    }
  }

  variant(variant&& other) noexcept((... && std::is_nothrow_move_constructible_v<Args>))
    : index_(other.index_), hash_(other.hash_) {
    if (!empty()) {
      details::create<false, Args...>(index_, &buffer_, const_cast<void*>(static_cast<const void*>(&other.buffer_)));
    }
  }

  variant& operator=(const variant& other) noexcept(
    std::is_nothrow_copy_constructible_v<variant> &&
    (... && std::is_nothrow_swappable_v<Args>)&&(... && std::is_nothrow_move_assignable_v<Args>)) {
    if (this == &other) {
      return *this;
    }
    try {
      auto tmp{other};
      swap(tmp);
    } catch (...) {
      throw;
    }
    return *this;
  }

  variant& operator=(variant&& other) noexcept((... && std::is_nothrow_move_assignable_v<Args>)) {
    if (this == &other) {
      return *this;
    }
    if (!empty()) {
      clear();
    }
    if (!other.empty()) {
      details::create<false, Args...>(other.index_, &buffer_, other.buffer_);
      index_ = other.index_;
      hash_ = other.hash_;
    }
    return *this;
  }

  template <typename Type, std::enable_if_t<!std::is_same_v<std::decay_t<Type>, variant>, bool> = true>
  variant& operator=(Type&& object) noexcept(
    std::is_nothrow_copy_constructible_v<Type>&& std::is_nothrow_move_constructible_v<Type>&&
      std::is_nothrow_move_assignable_v<Type>) {
    variant tmp{std::forward<Type>(object)};
    *this = std::move(tmp);
    return *this;
  }

  auto& get() {
    return buffer_;
  }

  const auto& get() const {
    return buffer_;
  }

  void swap(variant& other) noexcept(
    (... && std::is_nothrow_move_assignable_v<Args>)&&(...&& std::is_nothrow_swappable_v<Args>)) {
    if (empty() && other.empty()) {
      return;
    }
    if (other.empty()) {
      other = std::move(*this);
      clear();
      return;
    }
    if (empty()) {
      *this = std::move(other);
      other.clear();
      return;
    }
    std::swap(*this, other);
  }

  bool empty() const noexcept {
    return index_ == variant_npos;
  }

  void clear() {
    if (index_ != variant_npos) {
      details::destroy<Args...>(index_, static_cast<void*>(&buffer_));
      index_ = variant_npos;
      hash_ = -1;
    }
  }

  std::size_t index() const noexcept {
    return index_;
  }

  ~variant() {
    if (!empty()) {
      clear();
    }
  }

  size_t hash_type() const {
    return hash_;
  }

 private:
  std::size_t index_ = variant_npos;
  std::size_t hash_ = -1;
  static constexpr std::size_t alignas_of_data_ = details::find_max<alignof(Args)...>::value;
  static constexpr std::size_t size_of_data_ = details::find_max<sizeof(Args)...>::value;
  alignas(alignas_of_data_) unsigned char buffer_[size_of_data_]{};
};

template <typename Type, typename... Types>
const Type* get(const variant<Types...>* object) {
  if (object != nullptr && !object->empty() && (object->index() != variant_npos) &&
      object->index() == details::get_index_v<Type, Types...>) {
    return reinterpret_cast<const Type*>(&object->get());
  }
  return nullptr;
}

template <typename Type, typename... Types>
Type* get(variant<Types...>* object) {
  return const_cast<Type*>(get<Type>(const_cast<const variant<Types...>*>(object)));
}

template <typename Type, typename... Types>
const Type& get(const variant<Types...>& object) {
  const Type* ptr = get<Type>(&object);
  if (ptr != nullptr) {
    return *ptr;
  }
  throw bad_get();
}

template <typename Type, typename... Types>
Type& get(variant<Types...>& object) {
  Type* ptr = get<Type>(&object);
  if (ptr != nullptr) {
    return *ptr;
  }
  throw bad_get();
}

template <typename Type, typename... Types>
Type&& get(variant<Types...>&& object) {
  auto* ptr = get<Type>(&object);
  if (ptr != nullptr) {
    return std::move(*ptr);
  }
  throw bad_get();
}

template <std::size_t index, typename... Types>
const typename details::get_type<index, Types...>::type* get(const variant<Types...>* object) {
  if (object == nullptr || object->empty() || index > sizeof...(Types)) {
    return nullptr;
  }
  return get<typename details::get_type<index, Types...>::type>(object);
}

template <std::size_t index, typename... Types>
typename details::get_type<index, Types...>::type* get(variant<Types...>* object) {
  if (object == nullptr || object->empty() || index > sizeof...(Types)) {
    return nullptr;
  }
  using type = typename details::get_type<index, Types...>::type;
  return get<type>(object);
}

template <std::size_t index, typename... Types>
const typename details::get_type<index, Types...>::type& get(const variant<Types...>& object) {
  if (object.empty() || index > sizeof...(Types)) {
    throw bad_get();
  }
  const auto* type_ptr = get<index>(&object);
  return *type_ptr;
}

template <std::size_t index, typename... Types>
typename details::get_type<index, Types...>::type& get(variant<Types...>& object) {
  if (object.empty() || index > sizeof...(Types)) {
    throw bad_get();
  }
  auto* type_ptr = get<index>(&object);
  return *type_ptr;
}

template <std::size_t index, typename... Types>
typename details::get_type<index, Types...>::type&& get(variant<Types...>&& object) {
  if (object.empty() || index > sizeof...(Types)) {
    throw bad_get();
  }
  auto* type_ptr = get<index>(&object);
  return std::move(*type_ptr);
}

namespace details {

template <typename...>
struct VisitorImpl;

template <class Head, class... Tail>
struct VisitorImpl<variant<Head, Tail...>> {
  template <class Functor, class Variant>
  [[maybe_unused]] static decltype(auto) visit(Functor&& func, Variant&& var) {
    if (var.hash_type() == typeid(Head).hash_code())
      return func(get<Head>(std::forward<Variant>(var)));
    else
      return VisitorImpl<variant<Tail...>>::visit(std::forward<Functor>(func), std::forward<Variant>(var));
  }
};

template <class Head>
struct VisitorImpl<variant<Head>> {
  template <class T, class Variant>
  static decltype(auto) visit(T&& func, Variant&& var) {
    if (var.hash_type() == typeid(Head).hash_code())
      return func(get<Head>(std::forward<Variant>(var)));
    else
      return func();
  }
};
}  // namespace details

template <class Functor, class Variant>
decltype(auto) apply_visitor(Functor&& func, Variant&& var) {
  return details::VisitorImpl<std::decay_t<Variant>>::visit(std::forward<Functor>(func), std::forward<Variant>(var));
}

}  // namespace util
