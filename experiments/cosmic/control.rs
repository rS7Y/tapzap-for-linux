// SPDX-License-Identifier: GPL-3.0-only
use std::{
    fs,
    io::{self, Read, Write},
    os::unix::{fs::PermissionsExt, net::{UnixListener, UnixStream}},
    path::{Path, PathBuf},
    sync::{Arc, Mutex, atomic::{AtomicBool, Ordering}},
    thread::{self, JoinHandle},
    time::{Duration, Instant},
};

const NEUTRAL: [f32; 3] = [1.0; 3];
const LEASE: Duration = Duration::from_secs(2);
const MAX_INPUT: usize = 1024;

/// Keep the lease and control socket independent of compositor rendering/I/O.
/// Only the latest desired gains cross the boundary; no queued stale SET can
/// overtake OFF or an expired owner's neutral state when rendering resumes.
pub struct Worker {
    rgb: Arc<Mutex<[f32; 3]>>,
    stop: Arc<AtomicBool>,
    thread: Option<JoinHandle<()>>,
}

impl Worker {
    pub fn bind(path: &Path) -> io::Result<Self> {
        let mut controller = Controller::bind(path)?;
        let rgb = Arc::new(Mutex::new(NEUTRAL));
        let stop = Arc::new(AtomicBool::new(false));
        let published = Arc::clone(&rgb);
        let stopping = Arc::clone(&stop);
        let thread = thread::Builder::new().name("tapzap-control".into()).spawn(move || {
            while !stopping.load(Ordering::Acquire) {
                let current = controller.poll();
                *published.lock().unwrap() = current;
                thread::park_timeout(Duration::from_millis(10));
            }
        })?;
        Ok(Self { rgb, stop, thread: Some(thread) })
    }

    pub fn rgb(&self) -> [f32; 3] {
        // A failed worker must never leave an old non-neutral snapshot active.
        if self.thread.as_ref().is_none_or(|thread| thread.is_finished()) {
            return NEUTRAL;
        }
        self.rgb.lock().map(|rgb| *rgb).unwrap_or(NEUTRAL)
    }
}

impl Drop for Worker {
    fn drop(&mut self) {
        self.stop.store(true, Ordering::Release);
        if let Some(thread) = self.thread.take() {
            thread.thread().unpark();
            let _ = thread.join();
        }
    }
}

/// Prototype-only, single-owner RGB channel. It never changes saved settings.
pub struct Controller {
    listener: UnixListener,
    path: PathBuf,
    client: Option<UnixStream>,
    input: Vec<u8>,
    rgb: [f32; 3],
    last_activity: Instant,
    last_poll: Instant,
    suspend_success: Option<u64>,
}

fn suspend_success() -> Option<u64> {
    fs::read_to_string("/sys/power/suspend_stats/success").ok()?.trim().parse().ok()
}

impl Controller {
    pub fn bind(path: &Path) -> io::Result<Self> {
        // Never unlink a path belonging to another running compositor.
        let listener = UnixListener::bind(path)?;
        fs::set_permissions(path, fs::Permissions::from_mode(0o600))?;
        listener.set_nonblocking(true)?;
        let now = Instant::now();
        Ok(Self {
            listener, path: path.to_owned(), client: None, input: Vec::new(),
            rgb: NEUTRAL, last_activity: now, last_poll: now,
            suspend_success: suspend_success(),
        })
    }

    fn reset(&mut self) {
        self.client = None;
        self.input.clear();
        self.rgb = NEUTRAL;
    }

    pub fn poll(&mut self) -> [f32; 3] {
        self.poll_using(suspend_success)
    }

    pub(crate) fn poll_using(&mut self, mut read_success: impl FnMut() -> Option<u64>) -> [f32; 3] {
        let mut success = read_success();
        // With no cpuidle driver, Linux s2idle can advance CLOCK_MONOTONIC
        // while freezing both peers. The success counter is updated after
        // userspace is thawed, so allow its publication a bounded 100 ms.
        // An ordinary scheduler stall/SIGSTOP never earns a lease extension.
        if self.client.is_some()
            && (self.last_activity.elapsed() >= LEASE
                || self.last_poll.elapsed() >= Duration::from_millis(50))
            && self.suspend_success.is_some() && success == self.suspend_success {
            let deadline = Instant::now() + Duration::from_millis(100);
            while success == self.suspend_success && Instant::now() < deadline {
                thread::sleep(Duration::from_millis(1));
                success = read_success();
            }
        }
        self.poll_at(Instant::now(), success)
    }

    pub(crate) fn poll_at(&mut self, now: Instant, success: Option<u64>) -> [f32; 3] {
        if matches!((self.suspend_success, success), (Some(before), Some(after)) if after > before) {
            // Credit the confirmed-sleep poll gap, retaining the lease age
            // already consumed before it. This conservatively includes any
            // scheduling delay in that same gap; later awake gaps expire.
            self.last_activity += now.duration_since(self.last_poll);
        }
        self.last_poll = now;
        self.suspend_success = success;
        if self.client.is_some() && now.duration_since(self.last_activity) >= LEASE {
            self.reset();
        }
        self.read_client(now);
        // Bound per-tick work, including connections from a misbehaving client.
        for _ in 0..4 {
            let Ok((mut stream, _)) = self.listener.accept() else { break };
            if stream.set_nonblocking(true).is_err() { continue; }
            if self.client.is_some() {
                let _ = stream.write_all(b"BUSY\n");
            } else if stream.write_all(b"TAPZAP-COSMIC 1\n").is_ok() {
                self.client = Some(stream);
                self.last_activity = now;
            }
        }
        self.rgb
    }

    fn read_client(&mut self, now: Instant) {
        let Some(stream) = self.client.as_mut() else { return };
        let mut bytes = [0; MAX_INPUT + 1];
        let count = match stream.read(&mut bytes) {
            Ok(0) => { self.reset(); return; }
            Ok(count) => count,
            Err(e) if e.kind() == io::ErrorKind::WouldBlock => return,
            Err(_) => { self.reset(); return; }
        };
        if self.input.len() + count > MAX_INPUT {
            self.reset();
            return;
        }
        self.input.extend_from_slice(&bytes[..count]);
        while let Some(end) = self.input.iter().position(|byte| *byte == b'\n') {
            let line: Vec<_> = self.input.drain(..=end).collect();
            let rgb = std::str::from_utf8(&line).ok()
                .and_then(|text| parse_command(text, self.rgb));
            let Some(rgb) = rgb else { self.reset(); return };
            self.rgb = rgb;
            self.last_activity = now;
            let reply = format!("OK {} {} {}\n", rgb[0], rgb[1], rgb[2]);
            if self.client.as_mut().unwrap().write_all(reply.as_bytes()).is_err() {
                self.reset();
                return;
            }
        }
    }
}

impl Drop for Controller {
    fn drop(&mut self) { let _ = fs::remove_file(&self.path); }
}

pub fn parse_command(line: &str, current: [f32; 3]) -> Option<[f32; 3]> {
    let words: Vec<_> = line.split_whitespace().collect();
    match words.as_slice() {
        ["PING"] => Some(current),
        ["OFF"] => Some(NEUTRAL),
        ["SET", red, green, blue] => {
            let rgb = [red.parse::<f32>().ok()?, green.parse().ok()?, blue.parse().ok()?];
            rgb.iter().all(|v| v.is_finite() && (0.0..=1.0).contains(v)).then_some(rgb)
        }
        _ => None,
    }
}
