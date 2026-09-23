# Tap Zap for Linux — beta reports

This public repository is for [Tap Zap's Linux beta](https://tapzap.app/linux-beta/) issue reports. It contains reporting forms, not the application source or downloads.

Before opening a report, check the [known issues](https://tapzap.app/linux-beta/docs/KNOWN-ISSUES.md) and [existing reports](https://github.com/rS7Y/tapzap-linux/issues). Then [choose a report form](https://github.com/rS7Y/tapzap-linux/issues/new/choose):

- **Bug report:** a crash, color-restoration problem, incorrect filtering, or UI defect.
- **Compatibility:** a build that does not install, launch, or work on a particular distro, desktop, session, GPU, or display setup.
- **Feature request:** a specific Linux improvement and why it would help.

The forms ask for the build, environment, what happened, and the details needed to investigate. Short, exact steps and a *redacted* error excerpt are more useful than a full log dump. Do not provoke a crash or disconnect a display just to make a report.

Issues are public. Do not post license keys, serial numbers, account details, hostnames, home-directory paths, or unreviewed logs/screenshots. Use [Tap Zap support](https://tapzap.app/contact) for anything private or security-sensitive; see [SECURITY.md](SECURITY.md).

For initial beta use, keep HDR, PWM-Safe, Extreme Dim, and Launch at Login off. Check OFF and a normal Quit on one SDR display before a longer session. See the [known issues](https://tapzap.app/linux-beta/docs/KNOWN-ISSUES.md) for recovery guidance.

Maintainers can check the form configuration with `python3 -m pip install -r requirements-dev.txt` followed by `python3 -m unittest discover -s tests -v`.
