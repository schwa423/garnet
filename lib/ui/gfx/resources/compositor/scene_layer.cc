// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/lib/ui/gfx/resources/compositor/scene_layer.h"

// TODO(before-submit): remove unnecessary headers
#include "garnet/lib/ui/gfx/engine/hit_tester.h"
#include "garnet/lib/ui/gfx/resources/camera.h"
#include "garnet/lib/ui/gfx/resources/compositor/layer_stack.h"
#include "garnet/lib/ui/gfx/resources/nodes/scene.h"
#include "garnet/lib/ui/gfx/resources/renderers/renderer.h"
#include "garnet/lib/ui/scenic/util/error_reporter.h"
#include "garnet/public/lib/escher/util/type_utils.h"

namespace scenic {
namespace gfx {

const ResourceTypeInfo SceneLayer::kTypeInfo = {
    ResourceType::kLayer | ResourceType::kSceneLayer, "SceneLayer"};

SceneLayer::SceneLayer(Session* session, scenic::ResourceId id)
    : Layer(session, id, SceneLayer::kTypeInfo), translation_(0) {}

SceneLayer::~SceneLayer() = default;

bool SceneLayer::SetSize(const escher::vec2& size) {
  if (size.x <= 0 || size.y <= 0) {
    if (size != escher::vec2(0, 0)) {
      error_reporter()->ERROR()
          << "scenic::gfx::SceneLayer::SetSize(): size must be positive";
      return false;
    }
  }
  size_ = size;
  return true;
}

bool SceneLayer::SetColor(const escher::vec4& color) {
  color_ = color;
  return true;
}

void SceneLayer::SetCamera(CameraPtr camera) { camera_ = std::move(camera); }

void SceneLayer::SetScene(ScenePtr scene) { scene_ = std::move(scene); }

void SceneLayer::SetRenderer(RendererPtr renderer) {
  renderer_ = std::move(renderer);
}

void SceneLayer::CollectScenes(std::set<Scene*>* scenes_out) {
  // TODO(before-submit): do we need to consider camera_ here?  Who calls this,
  // for what purpose?
  // TODO(before-submit): can CollectScenes() be replaced by ResourceVisitor?
  if (camera_ && scene_) {
    scenes_out->insert(scene_.get());
  }
}

void SceneLayer::GetDrawableLayers(std::vector<SceneLayer*>* layers_out) {
  if (size_ != escher::vec2(0, 0) && camera_ && scene_) {
    layers_out->push_back(this);
  }
}

std::vector<Hit> SceneLayer::HitTest(const escher::ray4& ray,
                                     HitTester* hit_tester) const {
  FXL_CHECK(hit_tester);

  if (width() == 0.f || height() == 0.f) {
    return std::vector<Hit>();
  }

  // Normalize the origin of the ray with respect to the width and height of the
  // layer before passing it to the camera.
  escher::mat4 layer_normalization =
      glm::scale(glm::vec3(1.f / width(), 1.f / height(), 1.f));

  auto local_ray = layer_normalization * ray;

  // Transform the normalized ray by the camera's transformation.
  std::pair<escher::ray4, escher::mat4> camera_projection_pair =
      camera_->ProjectRayIntoScene(local_ray, GetViewingVolume());

  std::vector<Hit> hits =
      hit_tester->HitTest(scene_.get(), camera_projection_pair.first);

  escher::mat4 inverse_layer_transform =
      glm::inverse(camera_projection_pair.second * layer_normalization);

  // Take the camera's transformation into account; after this the hit's
  // inverse_transform goes from the passed in ray's coordinate system to the
  // hit nodes' coordinate system.
  for (auto& hit : hits) {
    hit.inverse_transform =
        inverse_layer_transform * glm::inverse(hit.inverse_transform);
  }

  return hits;
}

escher::ViewingVolume SceneLayer::GetViewingVolume() const {
  // TODO(MZ-194): Define these properties somewhere better (perhaps Scene?)
  // instead of hardcoding them here.
  constexpr float kTop = 1000;
  constexpr float kBottom = 0;
  return escher::ViewingVolume(size_.x, size_.y, kTop, kBottom);
}

}  // namespace gfx
}  // namespace scenic
