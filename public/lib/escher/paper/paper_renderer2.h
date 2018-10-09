// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_ESCHER_PAPER_PAPER_RENDERER2_H_
#define LIB_ESCHER_PAPER_PAPER_RENDERER2_H_

#include "lib/escher/paper/paper_readme.h"

#include "lib/escher/paper/paper_draw_call_factory.h"
#include "lib/escher/paper/paper_drawable.h"
#include "lib/escher/paper/paper_light.h"
#include "lib/escher/paper/paper_render_queue.h"
#include "lib/escher/paper/paper_renderer_config.h"
#include "lib/escher/paper/paper_shape_cache.h"
#include "lib/escher/paper/paper_transform_stack.h"
#include "lib/escher/renderer/renderer.h"
#include "lib/escher/renderer/uniform_binding.h"
#include "lib/fxl/logging.h"

namespace escher {

// |PaperRenderer2| provides a convenient and flexible interface for rendering
// shapes in a 3D space, as required by Scenic.  Clients achieve this primarily
// by passing instances of |PaperDrawable| to the |Draw()| method, using either
// pre-existing drawable types or their own subclasses.  For convenience, other
// drawing methods are provided, such as |DrawCircle()|.
//
// These draw methods are legal only between |BeginFrame()| and |EndFrame()|.
// Respectively, these two methods prepare the renderer to render a frame, and
// generate the Vulkan commands which actually perform the rendering.
//
// All other public methods must *not* be called between |BeginFrame()| and
// |EndFrame()|.  For example, |SetConfig()| can be used to choose a different
// shadow algorithm; changing this during the frame would cause incompatibility
// between the |PaperDrawCalls| previously and subsequently enqueued by the
// |PaperDrawCallFactory|.
//
// Implementation details follow...
//
// |PaperRenderer2| is responsible for coordinating its sub-components:
//   - |PaperDrawCallFactory|
//   - |PaperShapeCache|
//   - |PaperRenderQueue|
// See their class comments for details.
//
// Clients call |SetConfig()| to specify the coordination policies that will be
// used to render subsequent frames.  When the config changes, the renderer
// applies the appropriate changes to its sub-components.
//
// When |BeginFrame()| is called, each sub-component is made ready to render the
// new frame.  This depends on both the policies specified by |SetConfig()|, as
// well as the |PaperScene|, |Camera|, and |output_image| parameters.  Together,
// these determine how:
//   - shader data is encoded in the draw calls built by |PaperDrawCallFactory|
//   - tessellated meshes are post-processed before they are cached/uploaded
// ... and so forth.
//
// During |EndFrame()| the renderer first builds |RenderPassInfo| descriptions
// of the Vulkan render passes necessary to render the scene.  During each of
// these render passes, the renderer directs the render-queue to iterate over
// its draw calls and emit Vulkan commands into a |CommandBuffer|.  This is
// controlled by two parameters passed to the queue:
//   - |PaperRenderQueueFlags|, to control iteration over draw calls.
//   - |PaperRenderQueueContext|, used by draw calls to emit Vulkan commands.
class PaperRenderer2 final : public Renderer {
 public:
  static PaperRenderer2Ptr New(
      EscherWeakPtr escher, const PaperRendererConfig& config = {
                                .shadow_type = PaperRendererShadowType::kNone});
  ~PaperRenderer2() override;

  // Set configuration parameters which affect how the renderer will render
  // subsequent frames.
  void SetConfig(const PaperRendererConfig& config);
  const PaperRendererConfig& config() const { return config_; }

  // Does the renderer support the specified shadow type?
  bool SupportsShadowType(PaperRendererShadowType shadow_type) const;

  // Configures the renderer to render a frame into |output_image|.  The
  // renderer configures its sub-components to render the frame based on the
  // |scene| and |camera| parameters, along with the configuration params
  // previously set by |SetConfig()|.
  //
  // |PaperScene| describes aspects of the scene that affect the appearance of
  // scene object (e.g. lighting parameters), but does not provide the list of
  // scene objects to be rendered.  To render the scene, clients should follow
  // these steps:
  //   - |BeginFrame()|
  //   - |Draw()| each object in the scene.
  //   - |EndFrame()| emits the Vulkan commands that actually render the scene.
  void BeginFrame(const FramePtr& frame, const PaperScenePtr& scene,
                  const escher::Camera& camera,
                  const escher::ImagePtr& output_image);

  // See |BeginFrame()|.  After telling the renderer to draw the scene content,
  // |EndFrame()| emits commands into a Vulkan command buffer.  Submitting this
  // command buffer causes the scene to be rendered into |output_image|.
  void EndFrame();

  // The following methods may only be used between during a frame, i.e. between
  // calls to |BeginFrame()| and |EndFrame()|.

  // Return the transform stack, which affects the transform and clipping that
  // is applied to subsequently-drawn |PaperDrawables|.
  PaperTransformStack* transform_stack() {
    FXL_DCHECK(frame_data_) << "transform_stack only accessible during frame.";
    return &transform_stack_;
  }

  // Invokes DrawInScene() on the drawable object to generate and enqueue the
  // draw-calls that be transformed into Vulkan commands during EndFrame().
  void Draw(PaperDrawable* drawable, PaperDrawableFlags flags = {},
            mat4* matrix = nullptr);

  // Draw predefined shapes: circles, rectangles, and rounded-rectangles.
  // Generates and enqueues draw-calls that will emit Vulkan commands during
  // EndFrame().
  void DrawCircle(float radius, const PaperMaterialPtr& material,
                  PaperDrawableFlags flags = {}, mat4* matrix = nullptr);
  void DrawRect(vec2 min, vec2 max, const PaperMaterialPtr& material,
                PaperDrawableFlags flags = {}, mat4* matrix = nullptr);
  void DrawRoundedRect(const RoundedRectSpec& spec,
                       const PaperMaterialPtr& material,
                       PaperDrawableFlags flags = {}, mat4* matrix = nullptr);

  // Convenient way to draw "legacy" escher::Objects.  Generates and enqueues
  // draw-calls that will emit Vulkan commands during EndFrame().
  void DrawLegacyObject(const Object& obj, PaperDrawableFlags flags = {});

 private:
  explicit PaperRenderer2(EscherWeakPtr escher,
                          const PaperRendererConfig& config);
  PaperRenderer2(const PaperRenderer2&) = delete;

  // Stores all per-frame data in one place.
  struct FrameData {
    FrameData(const FramePtr& frame, const PaperScenePtr& scene,
              const ImagePtr& output_image,
              std::pair<TexturePtr, TexturePtr> depth_and_msaa_textures);
    ~FrameData();
    FramePtr frame;
    PaperScenePtr scene;
    ImagePtr output_image;
    TexturePtr depth_texture;
    TexturePtr msaa_texture;

    size_t num_lights;

    // UniformBindings returned by PaperDrawCallFactory::BeginFrame().  These
    // contain camera and lighting parameters that are shared between draw
    // calls.  The contents are opaque to the PaperRenderer, who trusts that
    // the PaperDrawCallFactory will generate DrawCalls that are compatible with
    // these UniformBindings.
    std::vector<UniformBinding> scene_uniform_bindings;

    BatchGpuUploaderPtr gpu_uploader;
  };

  // Called in BeginFrame() to obtain suitable render targets.
  // NOTE: call only once per frame.
  std::pair<TexturePtr, TexturePtr> ObtainDepthAndMsaaTextures(
      const FramePtr& frame, const ImageInfo& info);

  // Called during EndFrame().
  void GenerateCommandsForNoShadows();
  void GenerateCommandsForShadowVolumes();
  void InitRenderPassInfo(RenderPassInfo* render_pass_info);

  PaperRendererConfig config_;

  PaperDrawCallFactory draw_call_factory_;
  PaperRenderQueue render_queue_;
  PaperShapeCache shape_cache_;
  PaperTransformStack transform_stack_;

  std::vector<TexturePtr> depth_buffers_;
  std::vector<TexturePtr> msaa_buffers_;

  std::unique_ptr<FrameData> frame_data_;

  ShaderProgramPtr ambient_light_program_;
  ShaderProgramPtr point_light_program_;
  ShaderProgramPtr shadow_volume_geometry_program_;
  ShaderProgramPtr shadow_volume_geometry_debug_program_;
  ShaderProgramPtr shadow_volume_lighting_program_;
};

}  // namespace escher

#endif  // LIB_ESCHER_PAPER_PAPER_RENDERER2_H_
