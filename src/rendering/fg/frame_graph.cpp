#include "frame_graph.h"
#include "graphviz_writer.h"
#include <stack>

//
// FrameGraph class:
//

void FrameGraph::reserve(uint32_t numPasses, uint32_t numResources) {
  m_passNodes.reserve(numPasses);
  m_resourceNodes.reserve(numResources);
  m_resourceRegistry.reserve(numResources);
}

bool FrameGraph::isValid(FrameGraphResource id) const {
  const auto &node = _get_resource_node(id);
  return node.getVersion() == _get_resource_entry(node).get_version();
}

void FrameGraph::compile() {
  for (auto &pass : m_passNodes) {
    pass.m_refCount = static_cast<int32_t>(pass.m_writes.size());
    for (const auto [id, _] : pass.m_reads) {
      auto &consumed = m_resourceNodes[id];
      consumed.m_refCount++;
    }
    for (const auto [id, _] : pass.m_writes) {
      auto &written = m_resourceNodes[id];
      written.m_producer = &pass;
    }
  }

  // -- Culling:

  std::stack<ResourceNode *> unreferencedResources;
  for (auto &node : m_resourceNodes) {
    if (node.m_refCount == 0) unreferencedResources.push(&node);
  }
  while (!unreferencedResources.empty()) {
    auto *unreferencedResource = unreferencedResources.top();
    unreferencedResources.pop();
    PassNode *producer{unreferencedResource->m_producer};
    if (producer == nullptr || producer->hasSideEffect()) continue;

    assert(producer->m_refCount >= 1);
    if (--producer->m_refCount == 0) {
      for (const auto [id, _] : producer->m_reads) {
        auto &node = m_resourceNodes[id];
        if (--node.m_refCount == 0) unreferencedResources.push(&node);
      }
    }
  }

  // -- Calculate resources lifetime:

  for (auto &pass : m_passNodes) {
    //if (pass.m_refCount == 0) continue;       // test
    if (pass.m_refCount == 0 && !pass.hasSideEffect()) continue;


    for (const auto id : pass.m_creates)
      _get_resource_entry(id).m_producer = &pass;
    for (const auto [id, _] : pass.m_writes)
      _get_resource_entry(id).m_last = &pass;
    for (const auto [id, _] : pass.m_reads)
      _get_resource_entry(id).m_last = &pass;
  }
}
void FrameGraph::execute(void *context, void *allocator) {
  for (const auto &pass : m_passNodes) {
    if (!pass.canExecute()) continue;

    for (const auto id : pass.m_creates)
      _get_resource_entry(id).create(allocator);

    for (const auto [id, flags] : pass.m_reads) {
      if (flags != kFlagsIgnored) _get_resource_entry(id).pre_read(flags, context);
    }
    for (const auto [id, flags] : pass.m_writes) {
      if (flags != kFlagsIgnored)
        _get_resource_entry(id).pre_write(flags, context);
    }
    FrameGraphPassResources resources{*this, pass};
    std::invoke(*pass.m_exec, resources, context);

    for (auto &entry : m_resourceRegistry) {
        if (entry.m_last == &pass && entry.is_transient())
        {
			entry.destroy(allocator);
        }
    }
  }
}

//
// (private):
//

PassNode &
FrameGraph::_create_pass_node(const std::string_view name,
                            std::unique_ptr<FrameGraphPassConcept> &&base) {
  const auto id = static_cast<uint32_t>(m_passNodes.size());
  m_passNodes.emplace_back(PassNode{name, id, std::move(base)});
  return m_passNodes.back();
}

ResourceNode &FrameGraph::_create_resource_node(const std::string_view name,
                                              uint32_t resourceId,
                                              uint32_t version) {
  const auto id = static_cast<uint32_t>(m_resourceNodes.size());
  m_resourceNodes.emplace_back(ResourceNode{name, id, resourceId, version});
  return m_resourceNodes.back();
}
FrameGraphResource FrameGraph::_clone(FrameGraphResource id) {
  const auto &node = _get_resource_node(id);
  auto &entry = _get_resource_entry(node);
  entry.m_version++;

  const auto &clone = _create_resource_node(node.getName(), node.getResourceId(),
                                          entry.get_version());
  return clone.getId();
}

const ResourceNode &FrameGraph::_get_resource_node(FrameGraphResource id) const {
  assert(id < m_resourceNodes.size());
  return m_resourceNodes[id];
}
const ResourceEntry &
FrameGraph::_get_resource_entry(FrameGraphResource id) const {
  return _get_resource_entry(_get_resource_node(id));
}
const ResourceEntry &
FrameGraph::_get_resource_entry(const ResourceNode &node) const {
  assert(node.m_resourceId < m_resourceRegistry.size());
  return m_resourceRegistry[node.m_resourceId];
}

// ---

std::ostream &operator<<(std::ostream &os, const FrameGraph &fg) {
  return fg.debugOutput(os, graphviz::Writer{});
}

//
// FrameGraph::Builder class:
//

FrameGraphResource FrameGraph::Builder::read(FrameGraphResource id,
                                             uint32_t flags) {
  assert(m_frameGraph.isValid(id));
  return m_passNode._read(id, flags);
}
FrameGraphResource FrameGraph::Builder::write(FrameGraphResource id,
                                              uint32_t flags) {
  assert(m_frameGraph.isValid(id));
  if (m_frameGraph._get_resource_entry(id).is_imported()) set_side_effect();

  if (m_passNode.creates(id)) {
    return m_passNode._write(id, flags);
  } else {
    // Writing to a texture produces a renamed handle.
    // This allows us to catch errors when resources are modified in
    // undefined order (when same resource is written by different passes).
    // Renaming resources enforces a specific execution order of the render
    // passes.
    m_passNode._read(id, kFlagsIgnored);
    return m_passNode._write(m_frameGraph._clone(id), flags);
  }
}
