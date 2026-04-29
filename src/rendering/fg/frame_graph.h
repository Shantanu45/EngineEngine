#pragma once

#include "pass_node.h"
#include "resource_node.h"
#include "resource_entry.h"

class FrameGraph {
  friend class FrameGraphPassResources;

public:
  FrameGraph() = default;
  FrameGraph(const FrameGraph &) = delete;
  FrameGraph(FrameGraph &&) noexcept = delete;

  FrameGraph &operator=(const FrameGraph &) = delete;
  FrameGraph &operator=(FrameGraph &&) noexcept = delete;

  friend std::ostream &operator<<(std::ostream &, const FrameGraph &);

  static constexpr auto kFlagsIgnored = ~0;

  class Builder final {
    friend class FrameGraph;

  public:
    Builder() = delete;
    Builder(const Builder &) = delete;
    Builder(Builder &&) noexcept = delete;

    Builder &operator=(const Builder &) = delete;
    Builder &operator=(Builder &&) noexcept = delete;

    template <_VIRTUALIZABLE_CONCEPT(T)>
    /** Declares the creation of a resource. */
    [[nodiscard]] FrameGraphResource create(const std::string_view name,
                                            const typename T::Desc &);
    /** Declares read operation. */
    FrameGraphResource read(FrameGraphResource id,
                            uint32_t flags = kFlagsIgnored);
    /**
     * Declares write operation.
     * @remark Writing to imported resource counts as side-effect.
     */
    [[nodiscard]] FrameGraphResource write(FrameGraphResource id,
                                           uint32_t flags = kFlagsIgnored);

    /** Ensures that this pass is not culled during the compilation phase. */
    Builder &set_side_effect() {
      m_passNode.m_hasSideEffect = true;
      return *this;
    }

  private:
    Builder(FrameGraph &fg, PassNode &node)
        : m_frameGraph{fg}, m_passNode{node} {}

  private:
    FrameGraph &m_frameGraph;
    PassNode &m_passNode;
  };

  void reserve(uint32_t numPasses, uint32_t numResources);

  struct NoData {};
  /**
   * @param setup Callback (lambda, may capture by reference), invoked
   * immediately, declare operations here.
   * @param exec Execution of this lambda is deferred until execute() phase
   * (must capture by value due to this).
   */
  template <typename Data = NoData, typename Setup, typename Execute>
  const Data &add_callback_pass(const std::string_view name, Setup &&setup,
                              Execute &&exec);

  template <_VIRTUALIZABLE_CONCEPT(T)>
  [[nodiscard]] const typename T::Desc &
  get_descriptor(FrameGraphResource id) const;

  template <_VIRTUALIZABLE_CONCEPT(T)>
  [[nodiscard]] T &get_resource(FrameGraphResource id);

  template <_VIRTUALIZABLE_CONCEPT(T)>
  /** Imports the given resource T into FrameGraph. */
  [[nodiscard]] FrameGraphResource import(const std::string_view name,
                                          const typename T::Desc &, T &&);

  /** @return True if the given resource is valid for read/write operation. */
  [[nodiscard]] bool isValid(FrameGraphResource id) const;

  /** Culls unreferenced resources and passes. */
  void compile();
  /** Invokes execution callbacks. */
  void execute(void *context = nullptr, void *allocator = nullptr);

  /**
   * resets nodes.
   * 
   */
  void reset() {
	  m_passNodes.clear();
	  m_resourceNodes.clear();
	  m_resourceRegistry.clear();
  }

  template <typename Writer>
  std::ostream &debugOutput(std::ostream &, Writer &&) const;

private:
  [[nodiscard]] PassNode &
  _create_pass_node(const std::string_view name,
                  std::unique_ptr<FrameGraphPassConcept> &&);

  template <_VIRTUALIZABLE_CONCEPT(T)>
  [[nodiscard]] FrameGraphResource _create(const ResourceEntry::Type,
                                           const std::string_view name,
                                           const typename T::Desc &, T &&);

  [[nodiscard]] ResourceNode &
  _create_resource_node(const std::string_view name, uint32_t resourceId,
                      uint32_t version = ResourceEntry::kInitialVersion);
  /** Increments ResourceEntry version and produces a renamed handle. */
  [[nodiscard]] FrameGraphResource _clone(FrameGraphResource id);

  [[nodiscard]] const ResourceNode &
  _get_resource_node(FrameGraphResource id) const;
  [[nodiscard]] const ResourceEntry &
  _get_resource_entry(FrameGraphResource id) const;
  [[nodiscard]] const ResourceEntry &
  _get_resource_entry(const ResourceNode &) const;

  [[nodiscard]] decltype(auto) _get_resource_node(FrameGraphResource id) {
    return const_cast<ResourceNode &>(
      const_cast<const FrameGraph *>(this)->_get_resource_node(id));
  }
  [[nodiscard]] decltype(auto) _get_resource_entry(FrameGraphResource id) {
    return const_cast<ResourceEntry &>(
      const_cast<const FrameGraph *>(this)->_get_resource_entry(id));
  }
  [[nodiscard]] decltype(auto) _get_resource_entry(const ResourceNode &node) {
    return const_cast<ResourceEntry &>(
      const_cast<const FrameGraph *>(this)->_get_resource_entry(node));
  }

private:
  std::vector<PassNode> m_passNodes;
  std::vector<ResourceNode> m_resourceNodes;
  std::vector<ResourceEntry> m_resourceRegistry;
};


/**
 * FrameGraphPassResources is the lookup interface the graph hands you inside the execute lambda. 
 * It is the thing that takes a FrameGraphResource handle and gives you back the real GPU object.
 * 
 * It is essentially a scoped view into the graph's internal resource table, restricted to only the resources this pass declared in its setup lambda. 
 * You can only get resources that this pass registered a read or write for - anything else is inaccessible.
 */
class FrameGraphPassResources {
  friend class FrameGraph;

public:
  FrameGraphPassResources() = delete;
  FrameGraphPassResources(const FrameGraphPassResources &) = delete;
  FrameGraphPassResources(FrameGraphPassResources &&) noexcept = delete;
  ~FrameGraphPassResources() = default;

  FrameGraphPassResources &operator=(const FrameGraphPassResources &) = delete;
  FrameGraphPassResources &
  operator=(FrameGraphPassResources &&) noexcept = delete;

  /**
   * @note Causes runtime-error with:
   * - Attempt to use obsolete handle (the one that has been renamed before)
   * - Incorrect resource type T
   */
  /**
   * Give it the handle, you get back the real object.
   * FrameGraphPassResources is only valid inside the execute lambda it was given to. 
   * You cannot store it, return it, or use it after the lambda returns:
   * 
   * \param id
   * \return 
   */
  template <_VIRTUALIZABLE_CONCEPT(T)> 
  [[nodiscard]] T &get(FrameGraphResource id);

  template <_VIRTUALIZABLE_CONCEPT(T)>
  [[nodiscard]] const typename T::Desc& get_descriptor(FrameGraphResource id) const;

private:
  FrameGraphPassResources(FrameGraph &fg, const PassNode &node)
      : m_frameGraph{fg}, m_passNode{node} {}

private:
  FrameGraph &m_frameGraph;
  const PassNode &m_passNode;
};

#include <fstream>

static inline void save_graph_to_file(const FrameGraph& fg, const std::string& filename) {
	// Open a file stream for writing
	std::ofstream file(filename);

	if (file.is_open()) {
		// Use the overloaded operator<< to write the Graphviz data to the file
		file << fg;

		file.close();
	}
}

#include "frame_graph.inl"
