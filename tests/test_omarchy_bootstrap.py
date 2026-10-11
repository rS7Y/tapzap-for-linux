# Copyright (C) 2026 Tap Zap.
# SPDX-License-Identifier: GPL-3.0-or-later
"""Exercise the bootstrap without network access or a desktop session."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class BootstrapTests(unittest.TestCase):
    def run_bootstrap(self, payload):
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            source = temporary / "download.py"
            source.write_bytes(payload)
            marker = temporary / "executed"
            # Replace only the network boundary. Python and the bootstrap run normally.
            curl = temporary / "curl"
            curl.write_text(
                "#!/bin/bash\n"
                'while [ "$#" -gt 0 ]; do\n'
                '  if [ "$1" = "--output" ]; then\n'
                '    /bin/cp "$BOOTSTRAP_PAYLOAD" "$2"\n'
                "    exit 0\n"
                "  fi\n"
                "  shift\n"
                "done\n"
                "exit 2\n"
            )
            curl.chmod(0o755)
            environment = dict(os.environ)
            environment.update(
                PATH=f"{temporary}{os.pathsep}{environment['PATH']}",
                BOOTSTRAP_PAYLOAD=str(source),
                BOOTSTRAP_MARKER=str(marker),
            )
            result = subprocess.run(
                ["bash", str(ROOT / "install-omarchy.sh"), "--help"],
                env=environment,
                capture_output=True,
                text=True,
                timeout=10,
            )
            return result, marker.exists()

    def test_changed_download_is_rejected_before_execution(self):
        payload = (
            b"import os\nfrom pathlib import Path\n"
            b"Path(os.environ['BOOTSTRAP_MARKER']).write_text('executed')\n"
        )
        result, executed = self.run_bootstrap(payload)
        self.assertFalse(executed, "Downloaded code ran before verification")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("checksum mismatch", result.stderr.lower())

    def test_verified_installer_can_show_help(self):
        payload = (ROOT / "src/omarchy-hyprland/install.py").read_bytes()
        result, executed = self.run_bootstrap(payload)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("--archive", result.stdout)
        self.assertFalse(executed)


if __name__ == "__main__":
    unittest.main()
