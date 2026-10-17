// SPDX-License-Identifier: GPL-3.0-only
mod control;

use std::{path::PathBuf, time::Duration};
use calloop::timer::{TimeoutAction, Timer};
use crate::state::State;

pub fn init(state: &mut State) -> anyhow::Result<()> {
    if std::env::var("TAPZAP_COSMIC_ENABLE").as_deref() != Ok("1") {
        return Ok(());
    }
    let runtime = PathBuf::from(std::env::var_os("XDG_RUNTIME_DIR")
        .ok_or_else(|| anyhow::anyhow!("TAP ZAP requires XDG_RUNTIME_DIR"))?);
    let path = runtime.join(format!("tapzap-cosmic-{}.sock", state.common.socket.to_string_lossy()));
    let controller = control::Worker::bind(&path)?;
    tracing::info!(?path, "Experimental TAP ZAP RGB socket enabled");
    state.common.event_loop_handle.insert_source(Timer::immediate(), move |_, _, state| {
        let rgb = controller.rgb();
        let mut filter = state.common.config.dynamic_conf.screen_filter().clone();
        // Publish the full saved accessibility state on the first tick too.
        // None means it has not yet been applied through this bridge.
        if filter.tapzap_rgb != Some(rgb) {
            filter.tapzap_rgb = Some(rgb);
            match state.backend.update_screen_filter(&filter) {
                Ok(()) => {
                    state.common.config.dynamic_conf.set_tapzap_rgb(rgb);
                    let outputs: Vec<_> = state.common.shell.read().outputs().cloned().collect();
                    for output in outputs { state.backend.schedule_render(&output); }
                }
                Err(error) => tracing::warn!(?error, "TAP ZAP RGB update failed"),
            }
        }
        TimeoutAction::ToDuration(Duration::from_millis(33))
    }).map_err(|error| anyhow::anyhow!("TAP ZAP timer: {error}"))?;
    Ok(())
}
