/// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_LIB_UI_GFX_RESOURCES_NODES_ENTITY_NODE_H_
#define GARNET_LIB_UI_GFX_RESOURCES_NODES_ENTITY_NODE_H_

#include "garnet/lib/ui/gfx/resources/nodes/node.h"

#include "garnet/lib/ui/gfx/resources/drawables/drawable.h"

namespace scenic {
namespace gfx {

class EntityNode final : public Node {
 public:
  static const ResourceTypeInfo kTypeInfo;

  EntityNode(Session* session, scenic::ResourceId node_id);

  void Accept(class ResourceVisitor* visitor) override;

  bool AttachDrawable(DrawablePtr drawable);
  bool DetachDrawable(const DrawablePtr& drawable);

 private:
  // TODO(before-submit): document this hack and how it will go away when we
  // add a "Space ECS".
  std::set<DrawablePtr> drawables_;
};

}  // namespace gfx
}  // namespace scenic

#endif  // GARNET_LIB_UI_GFX_RESOURCES_NODES_ENTITY_NODE_H_
