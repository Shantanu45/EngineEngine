#pragma once

#include <string_view>

// https://www.bfilipek.com/2016/02/notes-on-c-sfinae.html

#if __cplusplus >= 202002L
#  include <concepts>

template <typename T>
concept Virtualizable = requires(T t) {
  requires std::conjunction_v<std::is_default_constructible<T>,
                              std::is_move_constructible<T>>;

  typename T::Desc;
  { t.create(typename T::Desc{}, (void *)nullptr) } -> std::same_as<void>;
  { t.destroy(typename T::Desc{}, (void *)nullptr) } -> std::same_as<void>;
};

#  define _VIRTUALIZABLE_CONCEPT(T) Virtualizable T
#  define _VIRTUALIZABLE_CONCEPT_IMPL(T) _VIRTUALIZABLE_CONCEPT(T)

template <typename T>
concept has_pre_read = requires(T t) {
  { t.pre_read(typename T::Desc{}, 0u, (void *)nullptr) } -> std::same_as<void>;
};
template <typename T>
concept has_pre_write = requires(T t) {
  { t.pre_write(typename T::Desc{}, 0u, (void *)nullptr) } -> std::same_as<void>;
};

template <typename T>
concept has_to_string = requires() {
  { T::to_string(typename T::Desc{}) } -> std::convertible_to<std::string_view>;
};
#else
// https://en.cppreference.com/w/cpp/types/enable_if
// https://levelup.gitconnected.com/c-detection-idiom-explained-5cc7207a0067

template <typename T, typename = void> struct has_Desc : std::false_type {};
template <typename T>
struct has_Desc<T, std::void_t<typename T::Desc>> : std::true_type {};

#  define DETECT_FUNCTION(function, ...)                                       \
    template <typename T> struct has_##function {                              \
      template <typename U> static constexpr std::false_type test(...) {       \
        return {};                                                             \
      }                                                                        \
      template <typename U>                                                    \
      static constexpr auto test(U *u) ->                                      \
        typename std::is_same<void,                                            \
                              decltype(u->function(__VA_ARGS__))>::type {      \
        return {};                                                             \
      }                                                                        \
      static constexpr bool value{test<T>(nullptr)};                           \
    };

DETECT_FUNCTION(create, typename T::Desc{}, (void *)nullptr)
DETECT_FUNCTION(destroy, typename T::Desc{}, (void *)nullptr)

template <typename T>
inline constexpr bool is_resource =
  std::conjunction_v<std::is_default_constructible<T>,
                     std::is_move_constructible<T>, has_Desc<T>, has_create<T>,
                     has_destroy<T>>;

#  define _VIRTUALIZABLE_CONCEPT_IMPL(T)                                       \
    typename T, std::enable_if_t<is_resource<T>, bool>
#  define _VIRTUALIZABLE_CONCEPT(T) _VIRTUALIZABLE_CONCEPT_IMPL(T) = true

DETECT_FUNCTION(pre_read, typename T::Desc{}, 0u, (void *)nullptr)
DETECT_FUNCTION(pre_write, typename T::Desc{}, 0u, (void *)nullptr)

#  undef DETECT_FUNCTION

template <typename T, typename = void> struct has_to_string : std::false_type {};
template <typename T>
struct has_to_string<T, std::void_t<decltype(T::to_string)>>
    : std::is_convertible<decltype(T::to_string(typename T::Desc{})),
                          std::string_view> {};

#endif
