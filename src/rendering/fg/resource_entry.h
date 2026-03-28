#pragma once

#include "type_traits.h"
#include <memory>

// Wrapper around a virtual resource.
class ResourceEntry final {
  friend class FrameGraph;

  enum class Type : uint8_t { Transient, Imported };

public:
  ResourceEntry() = delete;
  ResourceEntry(const ResourceEntry &) = delete;
  ResourceEntry(ResourceEntry &&) noexcept = default;

  ResourceEntry &operator=(const ResourceEntry &) = delete;
  ResourceEntry &operator=(ResourceEntry &&) noexcept = delete;

  static constexpr auto kInitialVersion{1u};

  [[nodiscard]] auto to_string() const { return m_concept->to_string(); }

  void create(void *allocator);
  void destroy(void *allocator);

  void pre_read(uint32_t flags, void *context) {
    m_concept->pre_read(flags, context);
  }
  void pre_write(uint32_t flags, void *context) {
    m_concept->pre_write(flags, context);
  }

  [[nodiscard]] auto get_id() const { return m_id; }
  [[nodiscard]] auto get_version() const { return m_version; }
  [[nodiscard]] auto is_imported() const { return m_type == Type::Imported; }
  [[nodiscard]] auto is_transient() const { return m_type == Type::Transient; }

  template <typename T> [[nodiscard]] T &get();
  template <typename T>
  [[nodiscard]] const typename T::Desc &get_descriptor() const;

private:
  template <typename T>
  ResourceEntry(const Type, uint32_t id, const typename T::Desc &, T &&);

  // http://www.cplusplus.com/articles/oz18T05o/
  // https://www.modernescpp.com/index.php/c-core-guidelines-type-erasure-with-templates

  struct Concept {
    virtual ~Concept() = default;

    virtual void create(void *) = 0;
    virtual void destroy(void *) = 0;

    virtual void pre_read(uint32_t flags, void *) = 0;
    virtual void pre_write(uint32_t flags, void *) = 0;

    virtual std::string to_string() const = 0;
  };
  template <typename T> struct Model final : Concept {
    Model(const typename T::Desc &, T &&);

    void create(void *allocator) override;
    void destroy(void *allocator) override;

    void pre_read(uint32_t flags, void *context) override {
#if __cplusplus >= 202002L
      if constexpr (has_pre_read<T>)
#else
      if constexpr (has_pre_read<T>::value)
#endif
        resource.pre_read(descriptor, flags, context);
    }
    void pre_write(uint32_t flags, void *context) override {
#if __cplusplus >= 202002L
      if constexpr (has_pre_write<T>)
#else
      if constexpr (has_pre_write<T>::value)
#endif
        resource.pre_write(descriptor, flags, context);
    }

    std::string to_string() const override;

    const typename T::Desc descriptor;
    T resource;
  };

  template <typename T> [[nodiscard]] auto *_get_model() const;

private:
  const Type m_type;
  const uint32_t m_id;
  uint32_t m_version; // Incremented on each (unique) write declaration.
  std::unique_ptr<Concept> m_concept;

  PassNode *m_producer{nullptr};
  PassNode *m_last{nullptr};
};

#include "resource_entry.inl"
