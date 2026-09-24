# Troubleshooting safely

Tap Zap for Linux is experimental. Read [known issues](KNOWN-ISSUES.md) before
launching a build. Do not use a work display or a session where a temporary
color shift would put you at risk.

## If the display looks wrong

1. If the interface is responsive, switch Tap Zap OFF and wait for the display
   to settle. Quit the app normally afterward.
2. If OFF or Quit does not restore the expected colors, stop testing. Do not
   force-kill and relaunch repeatedly, delete helper state/journals, or run an
   unreviewed gamma-reset command; those actions can overwrite state needed for
   recovery.
3. If the color shift persists, sign out of the graphical session or restart
   the computer to reset the desktop session, then check the display's normal
   settings. Recovery may depend on the backend and driver.
4. Do not intentionally unplug a display to test recovery. If a display is
   already disconnected, reconnect it only if safe and convenient.

## If it does not build or launch

- Follow the exact package prerequisites in [Building](BUILDING.md). The
  preserved scripts may build a headless/stub variant when development
  dependencies are absent; an exit code alone is not enough.
- Run the non-display `selftest` only. It checks color-curve invariants, not
  actual display control.
- Confirm that the selected source directory matches the current desktop
  session in the [support matrix](SUPPORT-MATRIX.md).
- Do not paste raw system logs into public issues. Remove personal paths,
  usernames, hostnames, license keys, and unrelated application data.

For help, file a concise report with the [GitHub issue forms](https://github.com/rS7Y/tapzap-linux/issues/new/choose).
For security-sensitive details, use the private route in [SECURITY.md](../SECURITY.md).
