// SPDX-License-Identifier: GPL-3.0-only
// Standalone test fixture; it is not linked into the compositor.
mod control;
fn main() {
    let args: Vec<_> = std::env::args().collect();
    let controller = control::Worker::bind(std::path::Path::new(&args[1])).unwrap();
    let mut previous = [-1.0; 3];
    loop {
        // Model a compositor render callback blocked on cold shader/font I/O.
        // The protocol must keep servicing its owner while rendering is busy.
        if args.get(3).is_some_and(|path| std::path::Path::new(path).exists()) {
            std::thread::sleep(std::time::Duration::from_millis(10));
            continue;
        }
        let rgb = controller.rgb();
        if rgb != previous {
            std::fs::write(&args[2], format!("{} {} {}", rgb[0], rgb[1], rgb[2])).unwrap();
            previous = rgb;
        }
        std::thread::sleep(std::time::Duration::from_millis(10));
    }
}
