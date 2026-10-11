# Publishing an Omarchy release

Only the Omarchy package has a source-matched public binary release. The other
backend snapshots retain their separate experimental status.

1. Update the version in the root and packaged plugin manifests and in
   `src/omarchy-hyprland/package.sh`. Update the changelog and setup guide.
2. Run `make check` and review the complete diff. Commit to a task branch and
   verify its GitHub source/native-build checks.
3. On Linux x86_64 with the documented dependencies, build from a clean checkout
   of that exact commit:

   ```sh
   bash src/omarchy-hyprland/package.sh
   ```

4. The package runs engine/protocol, QML and isolated installer tests. Use an
   unlocked Omarchy desktop with `WAYLAND_DISPLAY` set to cover both native QML
   entry points. A skipped QML check is not a pass for a native release.
5. Verify the archive/checksum pair from its download directory using
   `sha256sum --check`. Install the archive on the test desktop; exercise Settings,
   all clock pickers, save/validation, a real scheduled start, manual hold, OFF,
   Quit and restart. Preserve and restore the operator's preferences. Record
   tested versions and remaining limits in [Validation](VALIDATION.md).
6. Fast-forward reviewed source into `main`. Tag that same commit
   `omarchy-v<VERSION>`. Create a GitHub release with the archive and adjacent
   checksum; include the exact source SHA, build command, dependencies, test
   results and experimental hardware limits in its notes.
7. Download the public assets again, compare hashes, and run the documented
   setup/update path. Do not replace a published archive silently; publish a
   new version for a changed payload.

The repository-root manifest is for Omarchy's Git manager. The package includes
the same UI plus the native engine. Test both layouts and keep their plugin IDs
and versions identical. `tapzap uninstall` preserves a Git-managed checkout and
its edits; Omarchy's own Remove command deletes that checkout separately.

Marketplace submissions use the
[official submission guide](https://github.com/omacom/omarchy-plugin-marketplace/blob/main/SUBMISSION.md).
Mark the native engine as manual setup. Submission and automated checks are
separate from maintainer approval and marketplace publication.
