// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_LIB_UI_GFX_ENGINE_ENGINE_RENDERER_H_
#define GARNET_LIB_UI_GFX_ENGINE_ENGINE_RENDERER_H_

#include "lib/escher/escher.h"

namespace escher {
namespace hmd {
class PoseBufferLatchingShader;
}  // namespace hmd
}  // namespace escher

namespace scenic_impl {
namespace gfx {

class Layer;

class EngineRenderer {
 public:
  explicit EngineRenderer(escher::EscherWeakPtr weak_escher);
  ~EngineRenderer();

  void RenderLayers(const escher::FramePtr& frame,
                    const escher::ImagePtr& output_image,
                    const std::vector<Layer*>& layers);

 private:
  // Draws all the overlays to textures, which are then drawn using the
  // returned model. "Overlays" are all the layers except the bottom one.
  std::unique_ptr<escher::Model> DrawOverlaysToModel(
      const std::vector<Layer*>& drawable_layers, const escher::FramePtr& frame,
      zx_time_t target_presentation_time);
  escher::ImagePtr GetLayerFramebufferImage(uint32_t width, uint32_t height);

  void DrawLayer(const escher::FramePtr& frame,
                 zx_time_t target_presentation_time, Layer* layer,
                 const escher::ImagePtr& output_image,
                 const escher::Model* overlay_model);

  const escher::EscherWeakPtr escher_;
  escher::PaperRendererPtr paper_renderer_;
  escher::ShadowMapRendererPtr shadow_renderer_;
  std::unique_ptr<escher::hmd::PoseBufferLatchingShader>
      pose_buffer_latching_shader_;
};

}  // namespace gfx
}  // namespace scenic_impl

#endif  // GARNET_LIB_UI_GFX_ENGINE_ENGINE_RENDERER_H_
