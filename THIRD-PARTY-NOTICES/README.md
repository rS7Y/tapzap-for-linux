# Third-party notices

The repository's Tap Zap-authored Linux code, documentation, and project
artwork are licensed under GPL-3.0-or-later, except for the components below.
Each upstream component keeps its own license; the root GPL license does not
replace those terms.

| Component / version | Source and included paths | License | Changes and retained notice |
|---|---|---|---|
| Anybody Bold and ExtraBold fonts, font metadata version `72942` (copyright 2020) | [Etcetera-Type-Co/Anybody](https://github.com/Etcetera-Type-Co/Anybody); copied into each variant's `assets/` directory (Omarchy: `src/linux/assets/`). | SIL Open Font License 1.1 | Unmodified font binaries. The upstream OFL text is retained beside every font directory and copied verbatim to [`OFL-1.1.txt`](OFL-1.1.txt). The preserved snapshot does not record the exact upstream Git commit. |
| `wlr-gamma-control-unstable-v1`, protocol/interface version 1 | Preserved `protocols/wlr-gamma-control-unstable-v1.xml` in the GNOME and Sway variants; upstream protocol source is the wlroots protocol collection. | Permissive notice embedded in the XML (copyright and grant retained verbatim) | XML is unmodified. The preservation snapshot does not record an upstream commit or release tag. |
| Hyprland CTM protocol `hyprland-ctm-control-v1`, manager interface version 2 | `src/omarchy-hyprland/src/linux/ctm/protocols/hyprland-ctm-control-v1.xml` | BSD-style terms | XML and matching [`HYPRLAND-PROTOCOL-LICENSE.txt`](../src/omarchy-hyprland/src/linux/assets/HYPRLAND-PROTOCOL-LICENSE.txt) are retained unmodified. The preserved add-on snapshot is `omarchy-addon-0.1.6`; no separate upstream commit is recorded. |
| wlr virtual pointer protocol `wlr-virtual-pointer-unstable-v1`, interface version 2 | `src/omarchy-hyprland/tests/wlr-virtual-pointer-unstable-v1.xml`; upstream protocol source is the wlroots protocol collection. | MIT | XML is unmodified and contains the original copyright and permission notice. The preservation snapshot does not record an upstream commit or release tag. |

The application links to the desktop and compositor libraries supplied by the
build host, including Wayland client, X11/XRandR, Cairo, FreeType, D-Bus/GIO,
and the relevant desktop services. No runtime system libraries are bundled in
this repository. A future binary package must inventory its actual dynamic
dependencies and preserve any additional notices before publication.

Wayland protocol C sources and headers are generated locally by
`wayland-scanner` from the retained XML; generated files are build output, not
committed source. No macOS or Windows implementation is included.
