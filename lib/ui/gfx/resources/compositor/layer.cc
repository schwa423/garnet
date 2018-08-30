// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/lib/ui/gfx/resources/compositor/layer.h"

// TODO(before-submit): remove unnecessary headers
#include "garnet/lib/ui/gfx/engine/hit_tester.h"
#include "garnet/lib/ui/gfx/resources/camera.h"
#include "garnet/lib/ui/gfx/resources/compositor/layer_stack.h"
#include "garnet/lib/ui/gfx/resources/renderers/renderer.h"
#include "garnet/lib/ui/scenic/util/error_reporter.h"
#include "garnet/public/lib/escher/util/type_utils.h"

namespace scenic {
namespace gfx {

const ResourceTypeInfo Layer::kTypeInfo = {ResourceType::kLayer, "Layer"};

Layer::Layer(Session* session, scenic::ResourceId id,
             const ResourceTypeInfo& type_info)
    : Resource(session, id, type_info) {}

Layer::~Layer() = default;

bool Layer::Detach() {
  if (layer_stack_) {
    layer_stack_->RemoveLayer(this);
    FXL_DCHECK(!layer_stack_);  // removed by RemoveLayer().
  }
  return true;
}

}  // namespace gfx
}  // namespace scenic
