// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/lib/ui/gfx/resources/drawables/drawable.h"

namespace scenic {
namespace gfx {

const ResourceTypeInfo Drawable::kTypeInfo = {ResourceType::kDrawable,
                                              "Drawable"};

Drawable::Drawable(Session* session,
                   scenic::ResourceId id,
                   const ResourceTypeInfo& type_info)
    : Resource(session, id, type_info) {
  FXL_DCHECK(type_info.IsKindOf(Drawable::kTypeInfo));
}

}  // namespace gfx
}  // namespace scenic
