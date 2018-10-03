// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_LIB_UI_GFX_ENGINE_HARDWARE_LAYER_ASSIGNMENTS_H_
#define GARNET_LIB_UI_GFX_ENGINE_HARDWARE_LAYER_ASSIGNMENTS_H_

#include <vector>

namespace scenic_impl {
namespace gfx {

class Layer;
class Swapchain;

// Layer IDs must be non-decreasing.  Adjacent layers with the same
// hardware_layer_id are intended to be flattened into a single image.
struct HardwareLayerAssignments {
  struct Item {
    // Must be unique within a HardwareLayerAssignment struct (no two Items can
    // share the same |hardware_layer_id|).
    uint8_t hardware_layer_id;
    // TODO(before-submit): use LayerPtr?
    std::vector<Layer*> layers;
  };

  std::vector<Item> items;
  Swapchain* swapchain = nullptr;

  bool empty() const { return items.empty(); }
};

}  // namespace gfx
}  // namespace scenic_impl

#endif  // GARNET_LIB_UI_GFX_ENGINE_HARDWARE_LAYER_ASSIGNMENTS_H_
