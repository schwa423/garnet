// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/lib/ui/scenic/command_dispatcher.h"

#include "garnet/lib/ui/scenic/session.h"

namespace scenic_impl {

CommandDispatcherContext::CommandDispatcherContext(Scenic* scenic,
                                                   Session* session)
    : scenic_(scenic), session_(session), session_id_(session->id()) {
  FXL_DCHECK(scenic_);
  FXL_DCHECK(session_);
  FXL_DCHECK(session_id_);
}

CommandDispatcherContext::CommandDispatcherContext(
    CommandDispatcherContext&& context)
    : CommandDispatcherContext(context.scenic_, context.session_) {
  auto& other_scenic = const_cast<Scenic*&>(context.scenic_);
  auto& other_session = const_cast<Session*&>(context.session_);
  auto& other_session_id = const_cast<SessionId&>(context.session_id_);
  other_scenic = nullptr;
  other_session = nullptr;
  other_session_id = 0;
}

CommandDispatcher::CommandDispatcher(CommandDispatcherContext context)
    : context_(std::move(context)) {}

CommandDispatcher::~CommandDispatcher() { FXL_DCHECK(shutting_down_); }

void CommandDispatcher::ShutdownScenicSession() {
  if (!shutting_down_) {
    context_.scenic()->ShutdownSession(context_.session());

    // Scenic::ShutdownSession() immediately calls PrepareForShutdown(), then
    // schedules async destruction.
    FXL_DCHECK(shutting_down_);
  }
}

void CommandDispatcher::PrepareForShutdown() {
  // Should be called exactly once, by Scenic::ShutdownSession().
  FXL_DCHECK(!shutting_down_);
  shutting_down_ = true;

  // Subclasses must make their own preparations for shutdown.
  OnPrepareForShutdown();
}

TempSessionDelegate::TempSessionDelegate(CommandDispatcherContext context)
    : CommandDispatcher(std::move(context)) {}

}  // namespace scenic_impl
