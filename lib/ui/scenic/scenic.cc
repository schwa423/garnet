// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/lib/ui/scenic/scenic.h"

#include <lib/async/default.h>

#include "lib/component/cpp/startup_context.h"
#include "lib/fxl/logging.h"

namespace scenic_impl {

Scenic::Scenic(component::StartupContext* app_context,
               fit::closure quit_callback)
    : app_context_(app_context), quit_callback_(std::move(quit_callback)) {
  FXL_DCHECK(app_context_);

  app_context->outgoing().AddPublicService(scenic_bindings_.GetHandler(this));

  // Scenic relies on having a valid default dispatcher. A hard check here means
  // we don't have to be defensive everywhere else.
  FXL_CHECK(async_get_default_dispatcher());
}

Scenic::~Scenic() = default;

void Scenic::OnSystemInitialized(System* system) {
  size_t num_erased = uninitialized_systems_.erase(system);
  FXL_CHECK(num_erased == 1);

  if (uninitialized_systems_.empty()) {
    for (auto& closure : run_after_all_systems_initialized_) {
      closure();
    }
    run_after_all_systems_initialized_.clear();
  }
}

void Scenic::ShutdownSession(Session* session) {
  for (auto& binding : session_bindings_.bindings()) {
    // It's possible that this is called by BindingSet::CloseAndCheckForEmpty.
    // In that case, binding could be empty, so check for that.
    if (binding && binding->impl().get() == session) {
      // TODO(before-submit): This can't work because impl() returns a const&,
      // which can't be moved into a new unique_ptr.  Store a shared_ptr
      // instead?
      td::unique_ptr<Session> session_ptr(std::move(binding->impl()));

      // TODO(FIDL-344): also make sure that shutdown happens properly
      // when zx::channel closes.  The bug description describes a possible
      // BindingSet API which might make it easier to manage both
      // client-initiated and server-initiated session teardown.
      // TODO(SCN-1065): this doesn't actually remove the binding from the set;
      // resources are effectively leaked.
      binding->Unbind();

      // TODO(before-submit): PrepareForDestruction() ?
      session_ptr->PrepareToShutdown();

      async::PostTask(async_get_default_dispatcher(),
                      [keepalive{std::move(session_ptr)}] {
                        // This just keeps the Session alive.
                        // TODO(before-submit): does this "do the job well
                        // enough"?  Certainly the session needs to be kept
                        // alive until the next event-loop tick, at least when
                        // one of the sub-systems requested the shutdown because
                        // we're at an arbitrary place in the sub-system's
                        // call-stack.  However, if a sub-system has async tasks
                        // underway which ref the Session, then keeping the
                        // Session alive until the next tick might now be long
                        // enough.  What are the alternatives?  Perhaps each
                        // CommandDispatcher should return a future from
                        // PrepareToShutdown(), and the Session is destroyed the
                        // next tick after all futures have been resolved.
                      });
      return;
    }
  }
}

void Scenic::CreateSession(
    ::fidl::InterfaceRequest<fuchsia::ui::scenic::Session> session_request,
    ::fidl::InterfaceHandle<fuchsia::ui::scenic::SessionListener> listener) {
  if (uninitialized_systems_.empty()) {
    CreateSessionImmediately(std::move(session_request), std::move(listener));
  } else {
    run_after_all_systems_initialized_.push_back(
        [this, session_request = std::move(session_request),
         listener = std::move(listener)]() mutable {
          CreateSessionImmediately(std::move(session_request),
                                   std::move(listener));
        });
  }
}

void Scenic::CreateSessionImmediately(
    ::fidl::InterfaceRequest<fuchsia::ui::scenic::Session> session_request,
    ::fidl::InterfaceHandle<fuchsia::ui::scenic::SessionListener> listener) {
  auto session =
      std::make_unique<Session>(this, next_session_id_++, std::move(listener));

  // Give each installed System an opportunity to install a CommandDispatcher in
  // the newly-created Session.
  std::array<std::unique_ptr<CommandDispatcher>, System::TypeId::kMaxSystems>
      dispatchers;
  for (size_t i = 0; i < System::TypeId::kMaxSystems; ++i) {
    if (auto& system = systems_[i]) {
      dispatchers[i] = system->CreateCommandDispatcher(
          CommandDispatcherContext(this, session.get()));
    }
  }
  session->SetCommandDispatchers(std::move(dispatchers));

  session_bindings_.AddBinding(std::move(session), std::move(session_request));
}

void Scenic::GetDisplayInfo(
    fuchsia::ui::scenic::Scenic::GetDisplayInfoCallback callback) {
  FXL_DCHECK(systems_[System::kGfx]);
  TempSystemDelegate* delegate =
      reinterpret_cast<TempSystemDelegate*>(systems_[System::kGfx].get());
  delegate->GetDisplayInfo(std::move(callback));
}

void Scenic::TakeScreenshot(
    fuchsia::ui::scenic::Scenic::TakeScreenshotCallback callback) {
  FXL_DCHECK(systems_[System::kGfx]);
  TempSystemDelegate* delegate =
      reinterpret_cast<TempSystemDelegate*>(systems_[System::kGfx].get());
  delegate->TakeScreenshot(std::move(callback));
}

void Scenic::GetDisplayOwnershipEvent(
    fuchsia::ui::scenic::Scenic::GetDisplayOwnershipEventCallback callback) {
  FXL_DCHECK(systems_[System::kGfx]);
  TempSystemDelegate* delegate =
      reinterpret_cast<TempSystemDelegate*>(systems_[System::kGfx].get());
  delegate->GetDisplayOwnershipEvent(std::move(callback));
}

}  // namespace scenic_impl
