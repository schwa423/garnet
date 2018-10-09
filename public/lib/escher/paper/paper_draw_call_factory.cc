// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "lib/escher/paper/paper_draw_call_factory.h"

#include <glm/gtc/matrix_access.hpp>

#include "lib/escher/escher.h"
#include "lib/escher/paper/paper_material.h"
#include "lib/escher/paper/paper_render_funcs.h"
#include "lib/escher/paper/paper_render_queue.h"
#include "lib/escher/paper/paper_render_queue_flags.h"
#include "lib/escher/paper/paper_scene.h"
#include "lib/escher/paper/paper_shader_structs.h"
#include "lib/escher/paper/paper_shape_cache.h"
#include "lib/escher/paper/paper_transform_stack.h"
#include "lib/escher/renderer/frame.h"
#include "lib/escher/renderer/render_queue_item.h"
#include "lib/escher/shape/mesh.h"
#include "lib/escher/util/hasher.h"
#include "lib/escher/util/trace_macros.h"

namespace escher {

namespace {

// Default 1x1 texture for Materials that have no texture.  See header file
// |white_texture_| comment.
TexturePtr CreateWhiteTexture(Escher* escher) {
  FXL_DCHECK(escher);
  uint8_t channels[4];
  channels[0] = channels[1] = channels[2] = channels[3] = 255;
  auto image = escher->NewRgbaImage(1, 1, channels);
  return escher->NewTexture(std::move(image), vk::Filter::eNearest);
}

}  // anonymous namespace

PaperDrawCallFactory::PaperDrawCallFactory(EscherWeakPtr weak_escher,
                                           const PaperRendererConfig& config)
    : white_texture_(CreateWhiteTexture(weak_escher.get())) {}

PaperDrawCallFactory::~PaperDrawCallFactory() { FXL_DCHECK(!frame_); }

void PaperDrawCallFactory::DrawCircle(float radius,
                                      const PaperMaterial& material,
                                      PaperDrawableFlags flags) {
  FXL_DCHECK(frame_);

  // We aim to improve cache hit rate by using a circle of radius 1.  This
  // requires us to push a new transform.

  const bool scale_radius = (radius != 1.f);
  const auto& transform = scale_radius ? transform_stack_->PushScale(radius)
                                       : transform_stack_->Top();
  const auto& entry = shape_cache_->GetCircleMesh(
      1.f, transform.clip_planes.data(), transform.clip_planes.size());

  EnqueueDrawCalls(entry, material, flags);

  if (scale_radius)
    transform_stack_->Pop();
}

void PaperDrawCallFactory::DrawRect(vec2 min, vec2 max,
                                    const PaperMaterial& material,
                                    PaperDrawableFlags flags) {
  FXL_DCHECK(frame_);

  const auto& transform = transform_stack_->Top();
  const auto& entry = shape_cache_->GetRectMesh(
      min, max, transform.clip_planes.data(), transform.clip_planes.size());
  EnqueueDrawCalls(entry, material, flags);
}

void PaperDrawCallFactory::DrawRoundedRect(const RoundedRectSpec& spec,
                                           const PaperMaterial& material,
                                           PaperDrawableFlags flags) {
  FXL_DCHECK(frame_);

  const auto& transform = transform_stack_->Top();
  const auto& entry = shape_cache_->GetRoundedRectMesh(
      spec, transform.clip_planes.data(), transform.clip_planes.size());
  EnqueueDrawCalls(entry, material, flags);
}

void PaperDrawCallFactory::EnqueueDrawCalls(
    const PaperShapeCacheEntry& cache_entry, const PaperMaterial& material,
    PaperDrawableFlags drawable_flags) {
  FXL_DCHECK(frame_);
  if (!cache_entry)
    return;

  TRACE_DURATION("gfx", "PaperDrawCallFactory::EnqueueDrawCalls");

  auto* mesh = cache_entry.mesh.get();
  const auto& texture =
      material.texture() ? material.texture() : white_texture_;
  const auto& transform = transform_stack_->Top();
  const uint32_t num_indices = cache_entry.num_indices;
  const uint32_t num_shadow_volume_indices =
      cache_entry.num_shadow_volume_indices;

  Hash pipeline_hash;
  Hash mesh_hash;

  // Only the program goes into the pipeline hash.  If we also wanted e.g. some
  // objects to be stencil-tested and others not, this info would be included.
  {
    Hasher h;
    // TODO(ES-150): add this back in some way, with a more abstract pipeline
    // identifier instead of the actual program uid (which can change from pass
    // to pass). h.u64(shadow_volume_program_->uid());
    pipeline_hash = h.value();
  }

  // The object-hash is used to look up an existing MeshData for this
  // Mesh/Material pair, and is also used as part of the sort-key below.
  // We don't need to take opacity into account because separate RenderQueues
  // are used for opaque vs. translucent objects.
  {
    Hasher h;
    h.u64(mesh->uid());
    h.u64(texture->uid());
    mesh_hash = h.value();
  }

  PaperRenderFuncs::MeshData* mesh_data;
  auto it = object_data_.find(mesh_hash);
  if (it != object_data_.end()) {
    mesh_data = reinterpret_cast<PaperRenderFuncs::MeshData*>(it->second);
  } else {
    mesh_data = PaperRenderFuncs::NewMeshData(
        frame_, mesh, texture, num_indices, num_shadow_volume_indices);
    object_data_.insert(it, std::make_pair(mesh_hash, mesh_data));
  }

  // Allocate and initialize per-instance data.
  PaperRenderFuncs::MeshDrawData* draw_data = PaperRenderFuncs::NewMeshDrawData(
      frame_, transform.matrix, material.color(), drawable_flags);

  // TODO(ES-103): avoid reaching in to impl::CommandBuffer for keep-alive.
  frame_->command_buffer()->KeepAlive(texture.get());

  // Compute a depth metric for sorting objects.
#if 1
  // As long as the camera is above the top of the viewing volume and the scene
  // is composed of parallel-planar surfaces, we can simply subtract the
  // object's elevation from the camera's elevation.  Given these constraints,
  // this metric is superior to the alternate one below, which can provide
  // incorrect results at glancing angles (i.e. where the center of one object
  // is closer to the camera than the other, but is nevertheless partly behind
  // the other object from the camera's perspective.
  float depth = camera_pos_.z - transform.matrix[3][2];
#else
  // Compute the vector from the camera to the object, and project it against
  // the camera's direction to obtain the depth.
  float depth = glm::dot(vec3(transform[3]) - camera_pos_, camera_dir_);
#endif

  auto sort_key =
      material.opaque()
          ? SortKey::NewOpaque(pipeline_hash, mesh_hash, depth).key()
          : SortKey::NewTranslucent(pipeline_hash, mesh_hash, depth).key();

  auto queue_flags = material.opaque() ? PaperRenderQueueFlagBits::kOpaque
                                       : PaperRenderQueueFlagBits::kTranslucent;

  render_queue_->PushDrawCall(
      {.render_queue_item =
           {.sort_key = sort_key,
            .object_data = mesh_data,
            .instance_data = draw_data,
            .render_queue_funcs = {PaperRenderFuncs::RenderMesh}},
       .render_queue_flags = queue_flags});
}

void PaperDrawCallFactory::SetConfig(const PaperRendererConfig& config) {
  // NOTE: nothing currently to do here.  This will change, e.g. when we
  // add other shadow techniques.
}

std::vector<UniformBinding> PaperDrawCallFactory::BeginFrame(
    const FramePtr& frame, PaperScene* scene,
    PaperTransformStack* transform_stack, PaperRenderQueue* render_queue,
    PaperShapeCache* shape_cache, const Camera& camera) {
  FXL_DCHECK(!frame_ && frame && transform_stack && render_queue &&
             shape_cache);
  frame_ = frame;
  transform_stack_ = transform_stack;
  render_queue_ = render_queue;
  shape_cache_ = shape_cache;

  // A camera's transform doesn't move the camera; it is applied to the rest of
  // the scene to "move it away from the camera".  Therefore, the camera's
  // position in the scene can be obtained by inverting it and applying it to
  // the origin, or equivalently by inverting the transform and taking the
  // rightmost (translation) column.
  camera_pos_ = vec3(glm::column(glm::inverse(camera.transform()), 3));

  // The camera points down the negative-Z axis, so its world-space direction
  // can be obtained by applying the camera transform to the direction vector
  // [0, 0, -1, 0] (remembering that directions vectors have a w-coord of 0, vs.
  // 1 for postion vectors).  This is equivalent to taking the negated third
  // column of the transform.
  camera_dir_ = -vec3(glm::column(camera.transform(), 2));

  // Populated below, then returned.
  std::vector<UniformBinding> scene_uniforms;

  // Generate a UniformBinding for the camera and ambient light data.
  {
    auto writable_binding =
        NewPaperShaderUniformBinding<PaperShaderCameraAmbient>(frame);
    writable_binding.first->vp_matrix =
        camera.projection() * camera.transform();
    writable_binding.first->ambient_light_color = scene->ambient_light.color;
    scene_uniforms.push_back(writable_binding.second);
  }

  // Generate a uniform binding containing data for all point lights, if any.
  num_lights_ = scene->num_point_lights();
  if (num_lights_ > 0) {
    auto writable_binding =
        NewPaperShaderUniformBinding<PaperShaderPointLight>(frame, num_lights_);
    auto* point_lights = writable_binding.first;
    for (size_t i = 0; i < num_lights_; ++i) {
      point_lights[i].position = vec4(scene->point_lights[i].position, 1);
      point_lights[i].color = vec4(scene->point_lights[i].color, 1);
    }
    scene_uniforms.push_back(writable_binding.second);
  }

  return scene_uniforms;
}

void PaperDrawCallFactory::EndFrame() {
  FXL_DCHECK(frame_);
  frame_ = nullptr;
  transform_stack_ = nullptr;
  render_queue_ = nullptr;
  shape_cache_ = nullptr;
  object_data_.clear();

  camera_pos_ = vec3(0, 0, 0);
  camera_dir_ = vec3(0, 0, 0);
  num_lights_ = 0;
}

PaperDrawCallFactory::SortKey PaperDrawCallFactory::SortKey::NewOpaque(
    Hash pipeline_hash, Hash draw_hash, float depth) {
  // Depth must be non-negative, otherwise comparing the bit representations
  // won't work.
  FXL_DCHECK(depth >= 0) << " is: " << depth;

  // Prioritize minimizing pipeline changes over depth-sorting; both are more
  // important than minimizing mesh/texture state changes (in practice, almost
  // every draw call uses a separate mesh/texture anyway).
  // TODO(ES-105): We currently don't have multiple pipelines used in the opaque
  // pass, so we sort primarily by depth.  However, when we eventually do have
  // multiple pipelines, we may want to rewrite the pipeline hashes with a value
  // that reflects whether objects drawn using that pipeline tend to be drawn
  // in front or back.  For example, if we wanted to draw the background with a
  // shiny metallic BRDF, and everything else with a matte diffuse shader then
  // we should definitely draw the background last to avoid drawing expensive
  // pixels that will later be overdrawn; this isn't guaranteed if we sort by
  // the pipeline hash.
  uint64_t depth_key(glm::floatBitsToUint(depth));
  return SortKey((pipeline_hash.val << 48) | depth_key << 16 |
                 (draw_hash.val & 0xffff));
}

PaperDrawCallFactory::SortKey PaperDrawCallFactory::SortKey::NewTranslucent(
    Hash pipeline_hash, Hash draw_hash, float depth) {
  // Depth must be non-negative, otherwise comparing the bit representations
  // won't work.
  FXL_DCHECK(depth >= 0) << " is: " << depth;

  // Prioritize back-to-front order over state changes.
  uint64_t depth_key(glm::floatBitsToUint(depth) ^ 0xffffffffu);
  return SortKey((depth_key << 32) | (pipeline_hash.val & 0xffff0000u) |
                 (draw_hash.val & 0xffffu));
}

}  // namespace escher
