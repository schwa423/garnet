// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/lib/ui/gfx/resources/compositor/compositor.h"

#include "garnet/lib/ui/gfx/engine/session.h"
#include "garnet/lib/ui/gfx/resources/compositor/layer.h"
#include "garnet/lib/ui/gfx/resources/compositor/layer_stack.h"
#include "garnet/lib/ui/gfx/swapchain/swapchain.h"
#include "lib/escher/renderer/semaphore.h"

namespace scenic_impl {
namespace gfx {

const ResourceTypeInfo Compositor::kTypeInfo = {ResourceType::kCompositor,
                                                "Compositor"};

CompositorPtr Compositor::New(Session* session, ResourceId id) {
  return fxl::AdoptRef(
      new Compositor(session, id, Compositor::kTypeInfo, nullptr));
}

Compositor::Compositor(Session* session, ResourceId id,
                       const ResourceTypeInfo& type_info,
                       std::unique_ptr<Swapchain> swapchain)
    : Resource(session, id, type_info), swapchain_(std::move(swapchain)) {
  session->engine()->AddCompositor(this);
}

Compositor::~Compositor() { session()->engine()->RemoveCompositor(this); }

void Compositor::CollectScenes(std::set<Scene*>* scenes_out) {
  if (layer_stack_) {
    for (auto& layer : layer_stack_->layers()) {
      layer->CollectScenes(scenes_out);
    }
  }
}

bool Compositor::SetLayerStack(LayerStackPtr layer_stack) {
  layer_stack_ = std::move(layer_stack);
  return true;
}

std::pair<uint32_t, uint32_t> Compositor::GetBottomLayerSize() const {
  std::vector<Layer*> drawable_layers = GetDrawableLayers();
  FXL_CHECK(!drawable_layers.empty()) << "No drawable layers";
  return {drawable_layers[0]->width(), drawable_layers[0]->height()};
}

int Compositor::GetNumDrawableLayers() const {
  return GetDrawableLayers().size();
}

HardwareLayerAssignments Compositor::GetHardwareLayerAssignments() {
  // Generate a mapping between Scenic Layer resources and hardware layers.
  // NOTE: this is a placeholder; currently only a single hardware layer is
  // supported (layer 0).  Eventually this shouldn't be something done by
  // Compositor, but by a separate object that inspects the Compositor and its
  // layers.  TODO(before-submit): ?
  return HardwareLayerAssignments{
      .items = {{
          .hardware_layer_id = 0,
          .layers = GetDrawableLayers(),
      }},
      .swapchain = swapchain_.get(),
  };
}

std::vector<Layer*> Compositor::GetDrawableLayers() const {
  if (!layer_stack_) {
    return std::vector<Layer*>();
  }
  std::vector<Layer*> drawable_layers;
  for (auto& layer : layer_stack_->layers()) {
    if (layer->IsDrawable()) {
      drawable_layers.push_back(layer.get());
    }
  }

  // Sort the layers from bottom to top.
  std::sort(drawable_layers.begin(), drawable_layers.end(), [](auto a, auto b) {
    return a->translation().z < b->translation().z;
  });

  return drawable_layers;
}

}  // namespace gfx
}  // namespace scenic_impl
