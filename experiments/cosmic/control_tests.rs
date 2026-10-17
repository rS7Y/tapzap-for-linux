// SPDX-License-Identifier: GPL-3.0-only
#[path = "control.rs"]
mod control;
use std::{io::{Read, Write}, os::unix::net::UnixStream, sync::atomic::{AtomicUsize, Ordering}, time::Duration};

#[test]
fn nonexpired_resume_waits_before_consuming_the_sleep_gap() {
    let (mut server, mut client) = server();
    server.poll_using(|| Some(4)); read_reply(&mut client);
    client.write_all(b"SET 1 0 0\n").unwrap();
    server.poll_using(|| Some(4)); read_reply(&mut client);
    std::thread::sleep(Duration::from_millis(800));
    server.poll_using(|| Some(4));
    std::thread::sleep(Duration::from_millis(1000));
    let mut reads = 0;
    assert_eq!(server.poll_using(|| {
        reads += 1;
        Some(if reads == 1 { 4 } else { 5 })
    }), [1.0, 0.0, 0.0]);
    assert!(reads >= 2, "Lost the sleep gap before counter publication");
    std::thread::sleep(Duration::from_millis(250));
    assert_eq!(server.poll_using(|| Some(5)), [1.0, 0.0, 0.0]);
}

#[test]
fn short_sleep_waits_for_late_counter_when_lease_would_expire() {
    let (mut server, mut client) = server();
    server.poll_using(|| Some(4)); read_reply(&mut client);
    client.write_all(b"SET 1 0 0\n").unwrap();
    server.poll_using(|| Some(4)); read_reply(&mut client);
    std::thread::sleep(Duration::from_millis(1800));
    server.poll_using(|| Some(4));
    std::thread::sleep(Duration::from_millis(500));
    let mut reads = 0;
    assert_eq!(server.poll_using(|| {
        reads += 1;
        Some(if reads == 1 { 4 } else { 5 })
    }), [1.0, 0.0, 0.0]);
    assert!(reads >= 2, "Did not wait for the late kernel publication");
}

#[test]
fn confirmed_sleep_preserves_only_the_remaining_awake_lease() {
    let (mut server, mut client) = server();
    let start = std::time::Instant::now();
    server.poll_at(start, Some(4)); read_reply(&mut client);
    client.write_all(b"SET 1 0 0\n").unwrap();
    assert_eq!(server.poll_at(start, Some(4)), [1.0, 0.0, 0.0]);
    read_reply(&mut client);
    server.poll_at(start + Duration::from_millis(1800), Some(4));
    assert_eq!(server.poll_at(start + Duration::from_millis(6800), Some(5)), [1.0, 0.0, 0.0]);
    assert_eq!(server.poll_at(start + Duration::from_millis(7100), Some(5)), [1.0; 3]);
}

#[test]
fn unconfirmed_gaps_and_old_sleep_counts_do_not_extend_ownership() {
    for (before, after) in [(Some(4), Some(4)), (None, None), (None, Some(5)), (Some(4), None), (Some(4), Some(3))] {
        let (mut server, mut client) = server();
        let start = std::time::Instant::now();
        server.poll_at(start, before); read_reply(&mut client);
        client.write_all(b"SET 1 0 0\n").unwrap();
        server.poll_at(start, before); read_reply(&mut client);
        assert_eq!(server.poll_at(start + Duration::from_secs(5), after), [1.0; 3]);
    }
}

#[test]
fn consumed_sleep_count_cannot_cover_a_later_stall() {
    let (mut server, mut client) = server();
    let start = std::time::Instant::now();
    server.poll_at(start, Some(4)); read_reply(&mut client);
    client.write_all(b"SET 1 0 0\n").unwrap();
    server.poll_at(start, Some(4)); read_reply(&mut client);
    server.poll_at(start + Duration::from_millis(10), Some(5));
    assert_eq!(server.poll_at(start + Duration::from_secs(5), Some(5)), [1.0; 3]);
}

#[test]
fn resumed_owner_can_heartbeat_and_off_still_cancels() {
    let (mut server, mut client) = server();
    let start = std::time::Instant::now();
    server.poll_at(start, Some(4)); read_reply(&mut client);
    client.write_all(b"SET 1 0 0\n").unwrap();
    server.poll_at(start, Some(4)); read_reply(&mut client);
    client.write_all(b"PING\n").unwrap();
    assert_eq!(server.poll_at(start + Duration::from_secs(5), Some(5)), [1.0, 0.0, 0.0]);
    assert_eq!(read_reply(&mut client), "OK 1 0 0\n");
    client.write_all(b"OFF\n").unwrap();
    assert_eq!(server.poll_at(start + Duration::from_millis(5010), Some(5)), [1.0; 3]);
}

fn server() -> (control::Controller, UnixStream) {
    static NEXT: AtomicUsize = AtomicUsize::new(0);
    let path = std::env::temp_dir().join(format!("tapzap-test-{}-{}.sock", std::process::id(), NEXT.fetch_add(1, Ordering::Relaxed)));
    let server = control::Controller::bind(&path).unwrap();
    let client = UnixStream::connect(path).unwrap();
    client.set_read_timeout(Some(Duration::from_secs(1))).unwrap();
    (server, client)
}

fn read_reply(client: &mut UnixStream) -> String {
    let mut buffer = [0; 128];
    let n = client.read(&mut buffer).unwrap();
    String::from_utf8(buffer[..n].to_vec()).unwrap()
}

#[test]
fn connection_applies_and_disconnect_restores() {
    let (mut server, mut client) = server();
    assert_eq!(server.poll(), [1.0; 3]);
    assert_eq!(read_reply(&mut client), "TAPZAP-COSMIC 1\n");
    client.write_all(b"SET 1 0.25 0\n").unwrap();
    assert_eq!(server.poll(), [1.0, 0.25, 0.0]);
    assert!(read_reply(&mut client).starts_with("OK "));
    drop(client);
    assert_eq!(server.poll(), [1.0; 3]);
}

#[test]
fn partial_commands_do_not_change_output() {
    let (mut server, mut client) = server();
    server.poll(); read_reply(&mut client);
    client.write_all(b"SET 0.5 ").unwrap();
    assert_eq!(server.poll(), [1.0; 3]);
    client.write_all(b"0.5 0.5\n").unwrap();
    assert_eq!(server.poll(), [0.5; 3]);
}

#[test]
fn invalid_and_oversized_input_restore_neutral() {
    for invalid in [b"SET NaN 1 1\n".as_slice(), &[b'x'; 1025]] {
        let (mut server, mut client) = server();
        server.poll(); read_reply(&mut client);
        client.write_all(b"SET 1 0 0\n").unwrap();
        assert_eq!(server.poll(), [1.0, 0.0, 0.0]);
        read_reply(&mut client);
        client.write_all(invalid).unwrap();
        assert_eq!(server.poll(), [1.0; 3]);
    }
}

#[test]
fn second_owner_is_rejected() {
    let (mut server, mut client) = server();
    server.poll(); read_reply(&mut client);
    client.write_all(b"SET 1 0 0\n").unwrap();
    assert_eq!(server.poll(), [1.0, 0.0, 0.0]);
    read_reply(&mut client);
    let path = client.peer_addr().unwrap().as_pathname().unwrap().to_owned();
    let mut second = UnixStream::connect(path).unwrap();
    second.set_read_timeout(Some(Duration::from_secs(1))).unwrap();
    assert_eq!(server.poll(), [1.0, 0.0, 0.0]);
    assert_eq!(read_reply(&mut second), "BUSY\n");
}

#[test]
fn new_client_can_connect_immediately_after_previous_owner_closes() {
    let (mut server, mut first) = server();
    server.poll(); read_reply(&mut first);
    let path = first.peer_addr().unwrap().as_pathname().unwrap().to_owned();
    drop(first);
    let mut next = UnixStream::connect(path).unwrap();
    next.set_read_timeout(Some(Duration::from_secs(1))).unwrap();
    server.poll();
    assert_eq!(read_reply(&mut next), "TAPZAP-COSMIC 1\n");
}

#[test]
fn heartbeat_timeout_restores_even_if_client_remains_alive() {
    let (mut server, mut client) = server();
    server.poll(); read_reply(&mut client);
    client.write_all(b"SET 1 0 0\n").unwrap();
    assert_eq!(server.poll(), [1.0, 0.0, 0.0]);
    std::thread::sleep(Duration::from_millis(2100));
    assert_eq!(server.poll(), [1.0; 3]);
}
#[test]
fn set_applies_finite_rgb_gains() {
    assert_eq!(control::parse_command("SET 1 0.25 0", [1.0; 3]), Some([1.0, 0.25, 0.0]));
}
#[test]
fn ping_retains_gains_and_off_restores_identity() {
    assert_eq!(control::parse_command("PING", [1.0, 0.2, 0.0]), Some([1.0, 0.2, 0.0]));
    assert_eq!(control::parse_command("OFF", [1.0, 0.2, 0.0]), Some([1.0; 3]));
}
#[test]
fn invalid_values_are_rejected() {
    for line in ["SET NaN 0 0", "SET inf 0 0", "SET -0.1 0 0", "SET 1.1 0 0", "SET 1 0", "SET 1 0 0 1", "BOGUS"] {
        assert!(control::parse_command(line, [1.0; 3]).is_none(), "{line}");
    }
}

#[test]
fn worker_shutdown_closes_owner_and_removes_only_its_socket() {
    let path = std::env::temp_dir().join(format!("tapzap-worker-{}.sock", std::process::id()));
    let worker = control::Worker::bind(&path).unwrap();
    let mut client = UnixStream::connect(&path).unwrap();
    client.set_read_timeout(Some(Duration::from_secs(1))).unwrap();
    assert_eq!(read_reply(&mut client), "TAPZAP-COSMIC 1\n");
    // A second bind must leave the active worker's socket intact.
    assert!(control::Worker::bind(&path).is_err());
    client.write_all(b"SET 1 0 0\n").unwrap();
    assert!(read_reply(&mut client).starts_with("OK "));
    let deadline = std::time::Instant::now() + Duration::from_secs(1);
    while worker.rgb() != [1.0, 0.0, 0.0] && std::time::Instant::now() < deadline {
        std::thread::sleep(Duration::from_millis(1));
    }
    assert_eq!(worker.rgb(), [1.0, 0.0, 0.0]);
    drop(worker);
    assert!(!path.exists());
    assert_eq!(client.read(&mut [0; 1]).unwrap(), 0);
}
