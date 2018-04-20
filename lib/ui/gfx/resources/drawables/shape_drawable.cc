// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/lib/ui/gfx/resources/drawables/shape_drawable.h"

#include "garnet/lib/ui/scenic/util/error_reporter.h"

namespace scenic {
namespace gfx {

const ResourceTypeInfo ShapeDrawable::kTypeInfo = {
    ResourceType::kDrawable | ResourceType::kShapeDrawable, "ShapeDrawable"};

ShapeDrawable::ShapeDrawable(Session* session, scenic::ResourceId id)
    : Drawable(session, id, ShapeDrawable::kTypeInfo) {}

bool ShapeDrawable::SetMaterial(MaterialPtr material) {
  material_ = std::move(material);
  return true;
}

bool ShapeDrawable::SetShape(ShapePtr shape) {
  shape_ = std::move(shape);
  return true;
}

escher::Object ShapeDrawable::GenerateRenderObject(
    const escher::mat4& transform, float opacity_multiplier) {
  if (shape_ && material_) {
    material_->UpdateEscherMaterial();

    if (escher::MaterialPtr escher_material = material_->escher_material()) {
      if (opacity_multiplier < 1) {
        // When we want to support other material types (e.g. metallic shaders),
        // we'll need to change this. If we want to support semitransparent
        // textures and materials, we'll need more pervasive changes.
        glm::vec4 color = escher_material->color();
        color.a *= opacity_multiplier;
        escher_material =
            escher::Material::New(color, escher_material->texture());
        escher_material->set_opaque(false);
      }
      return shape_->GenerateRenderObject(transform, escher_material);
    }
  }

  return escher::Object();
}

}  // namespace gfx
}  // namespace scenic
