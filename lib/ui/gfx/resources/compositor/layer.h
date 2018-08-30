// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_LIB_UI_GFX_RESOURCES_COMPOSITOR_LAYER_H_
#define GARNET_LIB_UI_GFX_RESOURCES_COMPOSITOR_LAYER_H_

#include <set>

#include "garnet/lib/ui/gfx/engine/hit.h"
#include "garnet/lib/ui/gfx/engine/hit_tester.h"
#include "garnet/lib/ui/gfx/resources/resource.h"

namespace scenic {
namespace gfx {

class Layer;
class LayerStack;
class SceneLayer;
using LayerPtr = fxl::RefPtr<Layer>;

// A Layer can appear in a LayerStack, and be displayed by a Compositor.
// TODO(MZ-249): Layers can currently only use a rendered scene as content, but
// should also be able to directly use an Image/ImagePipe.
class Layer : public Resource {
 public:
  static const ResourceTypeInfo kTypeInfo;

  // | Resource |
  void Accept(class ResourceVisitor* visitor) override;

  // |Resource|, DetachCmd.
  bool Detach() override;

  // Add the scene rendered by this layer, if any, to |scenes_out|.
  virtual void CollectScenes(std::set<Scene*>* scenes_out) = 0;

  virtual void GetDrawableLayers(std::vector<SceneLayer*>* layers_out) = 0;

  // Performs a hit test into the scene of renderer, along the provided ray in
  // the layer's coordinate system.
  //
  // The hit collection behavior depends on the hit tester.
  virtual std::vector<Hit> HitTest(const escher::ray4& ray,
                                   HitTester* hit_tester) const = 0;

 protected:
  Layer(Session* session, scenic::ResourceId id,
        const ResourceTypeInfo& type_info);

  ~Layer() override;

 private:
  friend class LayerStack;

  LayerStack* layer_stack_ = nullptr;
};

}  // namespace gfx
}  // namespace scenic

#endif  // GARNET_LIB_UI_GFX_RESOURCES_COMPOSITOR_LAYER_H_
