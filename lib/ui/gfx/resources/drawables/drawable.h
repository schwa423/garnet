// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_LIB_UI_GFX_RESOURCES_DRAWABLES_DRAWABLE_H_
#define GARNET_LIB_UI_GFX_RESOURCES_DRAWABLES_DRAWABLE_H_

#include "garnet/lib/ui/gfx/resources/resource.h"

#include "lib/escher/scene/object.h"

namespace scenic {
namespace gfx {

class Drawable : public Resource {
 public:
  static const ResourceTypeInfo kTypeInfo;

  virtual escher::Object GenerateRenderObject(const escher::mat4& transform,
                                              float opacity_multiplier) = 0;

 protected:
  Drawable(Session* session, scenic::ResourceId id,
           const ResourceTypeInfo& type_info);

 private:
  void Accept(class ResourceVisitor* visitor) override;
};

using DrawablePtr = fxl::RefPtr<Drawable>;

}  // namespace gfx
}  // namespace scenic

#endif  // GARNET_LIB_UI_GFX_RESOURCES_DRAWABLES_DRAWABLE_H_
