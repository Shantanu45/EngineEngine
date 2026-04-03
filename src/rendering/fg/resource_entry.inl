#include <cassert>

//
// ResourceEntry class:
//

inline void ResourceEntry::create(void *allocator) {
  assert(is_transient());
  m_concept->create(allocator);
}
inline void ResourceEntry::destroy(void *allocator) {
  assert(is_transient());
  m_concept->destroy(allocator);
}

template <typename T> inline T &ResourceEntry::get() {
  return _get_model<T>()->resource;
}
template <typename T>
inline const typename T::Desc &ResourceEntry::get_descriptor() const {
  return _get_model<T>()->descriptor;
}

//
// (private):
//

template <typename T>
inline ResourceEntry::ResourceEntry(const Type type, uint32_t id,
                                    const typename T::Desc &desc, T &&obj)
    : m_type{type}, m_id{id}, m_version{kInitialVersion},
      m_concept{std::make_unique<Model<T>>(desc, std::forward<T>(obj))} {}

template <typename T> inline auto *ResourceEntry::_get_model() const {
  auto *model = dynamic_cast<Model<T> *>(m_concept.get());
  assert(model && "Invalid type");
  return model;
}

//
// ResourceEntry::Model class:
//

template <typename T>
inline ResourceEntry::Model<T>::Model(const typename T::Desc &desc, T &&obj)
    : descriptor{desc}, resource{std::move(obj)} {}

template <typename T>
inline void ResourceEntry::Model<T>::create(void *allocator) {
  resource.create(descriptor, allocator);
}
template <typename T>
inline void ResourceEntry::Model<T>::destroy(void *allocator) {
  resource.destroy(descriptor, allocator);
}

template <typename T>
inline std::string ResourceEntry::Model<T>::to_string() const {
#if __cplusplus >= 202002L
  if constexpr (has_to_string<T>)
#else
  if constexpr (has_to_string<T>::value)
#endif
    return T::to_string(descriptor);
  else
    return "";
}
