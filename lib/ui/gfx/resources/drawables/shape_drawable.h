// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_LIB_UI_GFX_RESOURCES_DRAWABLES_SHAPE_DRAWABLE_H_
#define GARNET_LIB_UI_GFX_RESOURCES_DRAWABLES_SHAPE_DRAWABLE_H_

#include "garnet/lib/ui/gfx/resources/drawables/drawable.h"

#include "garnet/lib/ui/gfx/resources/material.h"
#include "garnet/lib/ui/gfx/resources/shapes/shape.h"

namespace scenic {
namespace gfx {

class ShapeDrawable final : public Drawable {
 public:
  static const ResourceTypeInfo kTypeInfo;

  ShapeDrawable(Session* session, scenic::ResourceId id);

  bool SetShape(ShapePtr shape);
  bool SetMaterial(MaterialPtr material);

 private:
  ShapePtr shape_;
  MaterialPtr material_;

  // |Drawable|.
  escher::Object GenerateRenderObject(const escher::mat4& transform,
                                      float opacity_multiplier) override;
};

using ShapeDrawablePtr = fxl::RefPtr<ShapeDrawable>;

}  // namespace gfx
}  // namespace scenic

#endif  // GARNET_LIB_UI_GFX_RESOURCES_DRAWABLES_SHAPE_DRAWABLE_H_
