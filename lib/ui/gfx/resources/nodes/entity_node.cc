// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/lib/ui/gfx/resources/nodes/entity_node.h"

namespace scenic {
namespace gfx {

const ResourceTypeInfo EntityNode::kTypeInfo = {
    ResourceType::kNode | ResourceType::kEntityNode, "EntityNode"};

EntityNode::EntityNode(Session* session, scenic::ResourceId node_id)
    : Node(session, node_id, EntityNode::kTypeInfo) {}

bool EntityNode::AttachDrawable(DrawablePtr drawable) {
  drawables_.insert(std::move(drawable));
  return true;
}

bool EntityNode::DetachDrawable(const DrawablePtr& drawable) {
  error_reporter()->ERROR() << "DetachDrawable not implemented.";
  return false;
}

}  // namespace gfx
}  // namespace scenic
