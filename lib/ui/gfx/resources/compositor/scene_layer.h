// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_LIB_UI_GFX_RESOURCES_COMPOSITOR_SCENE_LAYER_H_
#define GARNET_LIB_UI_GFX_RESOURCES_COMPOSITOR_SCENE_LAYER_H_

#include <set>

#include "garnet/lib/ui/gfx/resources/compositor/layer.h"

#include "lib/escher/geometry/types.h"
#include "lib/escher/scene/viewing_volume.h"

namespace scenic {
namespace gfx {

class Camera;
class Layer;
class LayerStack;
class Renderer;
class Scene;
using CameraPtr = fxl::RefPtr<Camera>;
using LayerPtr = fxl::RefPtr<Layer>;
using RendererPtr = fxl::RefPtr<Renderer>;
using ScenePtr = fxl::RefPtr<Scene>;

// TODO(before-submit): document
class SceneLayer final : public Layer {
 public:
  static const ResourceTypeInfo kTypeInfo;

  SceneLayer(Session* session, scenic::ResourceId id);

  ~SceneLayer() override;

  // | Resource |
  void Accept(class ResourceVisitor* visitor) override;

  // SetSizeCmd.
  bool SetSize(const escher::vec2& size);
  const escher::vec2& size() const { return size_; }

  // SetColorCmd.
  bool SetColor(const escher::vec4& color);
  const escher::vec4& color() const { return color_; }

  // | Layer |
  void CollectScenes(std::set<Scene*>* scenes_out) override;

  // |Layer|
  void GetDrawableLayers(std::vector<SceneLayer*>* layers_out) override;

  const escher::vec3& translation() const { return translation_; }
  uint32_t width() const { return static_cast<uint32_t>(size_.x); }
  uint32_t height() const { return static_cast<uint32_t>(size_.y); }

  // TODO(MZ-250): support detecting and/or setting layer opacity.
  bool opaque() const { return false; }

  // Performs a hit test into scene, along the provided ray in the layer's
  // coordinate system.
  //
  // The hit collection behavior depends on the hit tester.
  std::vector<Hit> HitTest(const escher::ray4& ray,
                           HitTester* hit_tester) const override;

  // Nothing will be rendered unless a camera, a scene, and a renderer have been
  // set.
  void SetCamera(CameraPtr camera);
  void SetScene(ScenePtr scene);
  void SetRenderer(RendererPtr renderer);

  // Returns the current viewing volume of the layer. Used by the compositor
  // when initializing the stage, as well as for hit testing.
  escher::ViewingVolume GetViewingVolume() const;

  const CameraPtr& camera() const { return camera_; }
  const ScenePtr& scene() const { return scene_; }
  const RendererPtr& renderer() const { return renderer_; }

 private:
  friend class LayerStack;

  // TODO(before-submit): support camera and scene.
  CameraPtr camera_;
  ScenePtr scene_;
  RendererPtr renderer_;

  escher::vec3 translation_ = escher::vec3(0, 0, 0);
  escher::vec2 size_ = escher::vec2(0, 0);
  escher::vec4 color_ = escher::vec4(1, 1, 1, 1);
};

}  // namespace gfx
}  // namespace scenic

#endif  // GARNET_LIB_UI_GFX_RESOURCES_COMPOSITOR_SCENE_LAYER_H_
