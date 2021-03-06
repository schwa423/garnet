// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#![deny(warnings)]
#![feature(async_await, await_macro, futures_api, pin, arbitrary_self_types)]

// Macros used to serialize bonding data FIDL types for persistent storage.
#[macro_use] extern crate serde_derive;

use failure::{Error, ResultExt};
use fidl::endpoints::{ServerEnd, ServiceMarker};
use fidl_fuchsia_bluetooth_bredr::{ProfileMarker};
use fidl_fuchsia_bluetooth_control::ControlMarker;
use fidl_fuchsia_bluetooth_gatt::Server_Marker;
use fidl_fuchsia_bluetooth_le::{CentralMarker, PeripheralMarker};
use fuchsia_app::server::ServicesServer;
use fuchsia_async as fasync;
use fuchsia_bluetooth::{make_clones, util};
use fuchsia_syslog::{self as syslog, fx_log, fx_log_info};
use futures::{future, TryFutureExt};
use parking_lot::RwLock;
use std::sync::Arc;

mod services;
mod store;

mod host_device;
mod host_dispatcher;

use crate::host_dispatcher::*;

const BT_GAP_COMPONENT_ID: &'static str = "bt-gap";

fn main() -> Result<(), Error> {
    syslog::init_with_tags(&["bt-gap"]).expect("Can't init logger");
    fx_log_info!("Starting bt-gap...");

    let mut executor = fasync::Executor::new().context("Error creating executor")?;
    let stash = executor.run_singlethreaded(store::stash::init_stash(BT_GAP_COMPONENT_ID))?;

    let hd = Arc::new(RwLock::new(HostDispatcher::new(stash)));

    make_clones!(hd => host_hd, control_hd, central_hd, peripheral_hd, gatt_hd, profile_hd);

    let host_watcher = watch_hosts(host_hd);

    let server = ServicesServer::new()
        .add_service((ControlMarker::NAME, move |chan: fasync::Channel| {
            fx_log_info!("Spawning Control Service");
            fasync::spawn(
                services::start_control_service(control_hd.clone(), chan)
                    .unwrap_or_else(|e| eprintln!("Failed to spawn {:?}", e)),
            )
        })).add_service((CentralMarker::NAME, move |chan: fasync::Channel| {
            fx_log_info!("Connecting CentralService to Adapter");
            fasync::spawn(
                HostDispatcher::get_active_adapter(central_hd.clone())
                    .and_then(move |adapter| {
                        let remote = ServerEnd::<CentralMarker>::new(chan.into());
                        if let Some(adapter) = adapter {
                            let _ = adapter.read().get_host().request_low_energy_central(remote);
                        }
                        future::ready(Ok(()))
                    }).unwrap_or_else(|e| eprintln!("Failed to spawn {:?}", e)),
            )
        })).add_service((PeripheralMarker::NAME, move |chan: fasync::Channel| {
            fx_log_info!("Connecting Peripheral Service to Adapter");
            fasync::spawn(
                HostDispatcher::get_active_adapter(peripheral_hd.clone())
                    .and_then(move |adapter| {
                        let remote = ServerEnd::<PeripheralMarker>::new(chan.into());
                        if let Some(adapter) = adapter {
                            let _ = adapter
                                .read()
                                .get_host()
                                .request_low_energy_peripheral(remote);
                        }
                        future::ready(Ok(()))
                    }).unwrap_or_else(|e| eprintln!("Failed to spawn {:?}", e)),
            )
        })).add_service((ProfileMarker::NAME, move |chan: fasync::Channel| {
            fx_log_info!("Connecting Profile Service to Adapter");
            fasync::spawn(
                HostDispatcher::get_active_adapter(profile_hd.clone())
                    .and_then(move |adapter| {
                        let remote = ServerEnd::<ProfileMarker>::new(chan.into());
                        if let Some(adapter) = adapter {
                            let _ = adapter
                                .read()
                                .get_host()
                                .request_profile(remote);
                        }
                        future::ready(Ok(()))
                    }).unwrap_or_else(|e| eprintln!("Failed to spawn {:?}", e)),
            )
        })).add_service((Server_Marker::NAME, move |chan: fasync::Channel| {
            fx_log_info!("Connecting Gatt Service to Adapter");
            fasync::spawn(
                HostDispatcher::get_active_adapter(gatt_hd.clone())
                    .and_then(move |adapter| {
                        let remote = ServerEnd::<Server_Marker>::new(chan.into());
                        if let Some(adapter) = adapter {
                            let _ = adapter.read().get_host().request_gatt_server_(remote);
                        }
                        future::ready(Ok(()))
                    }).unwrap_or_else(|e| eprintln!("Failed to spawn {:?}", e)),
            )
        })).start()?;

    executor
        .run_singlethreaded(server.try_join(host_watcher))
        .map(|_| ())
}
