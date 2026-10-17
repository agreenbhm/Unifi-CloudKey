#!/usr/bin/env python3
"""Exercise archive installs, working-directory isolation, Compose variants and failed updates."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

SCRIPT = Path(__file__).resolve().parents[1] / "upgrade_cloudkey.sh"

with tempfile.TemporaryDirectory() as directory:
    temp = Path(directory)
    installation = temp / "archive installation"
    unrelated = temp / "another directory"
    installation.mkdir()
    unrelated.mkdir()
    shutil.copy2(SCRIPT, installation / SCRIPT.name)
    # No .git or Dockerfile: the documented archive installation needs neither.
    compose = installation / "docker-compose.yml"
    compose.write_text("services:\n  unifi:\n    image: example/unifi:selected\n")
    data = installation / "existing-data"
    data.write_bytes(b"existing application data\x00\xff")
    for variant in ["legacy", "plugin"]:
        for failure in ["", "config", "pull", "up"]:
            commands = temp / f"bin-{variant}-{failure or 'success'}"
            commands.mkdir()
            log = temp / "log"
            log.write_text("")
            # If the updater tries to rebuild or modify its checkout, fail the test.
            for name in ["git", "docker-compose" if variant == "legacy" else "docker"]:
                command = commands / name
                if name == "git":
                    command.write_text("#!/bin/sh\nprintf 'unexpected git\\n' >> \"$LOG\"\nexit 99\n")
                else:
                    command.write_text("""#!/bin/sh
if [ "$1" = compose ]; then shift; fi
if [ "$1" = version ]; then exit 0; fi
printf '%s|%s\n' "$PWD" "$*" >> "$LOG"
if [ "$1" != -f ]; then exit 99; fi
shift 2
case "$1" in config|pull|up) ;; *) exit 99 ;; esac
if [ "$1" = "$FAILURE" ]; then exit 42; fi
""")
                command.chmod(0o755)
            result = subprocess.run(["/bin/bash", str(installation / SCRIPT.name)], cwd=unrelated,
                                    env={**os.environ, "PATH": f"{commands}:/usr/bin:/bin",
                                         "LOG": str(log), "FAILURE": failure,
                                         "COMPOSE_FILE": str(unrelated / "wrong-compose.yml")},
                                    capture_output=True, text=True)
            assert (result.returncode == 0) == (not failure), result.stderr
            expected = ["config -q", "pull", "up -d"]
            if failure:
                expected = expected[:["config", "pull", "up"].index(failure) + 1]
            assert log.read_text().splitlines() == [f"{installation}|-f {compose} {command}" for command in expected]
            assert data.read_bytes() == b"existing application data\x00\xff"
            assert compose.read_text() == "services:\n  unifi:\n    image: example/unifi:selected\n"
            assert not (installation / ".git").exists()
            if failure:
                assert "Containers started." not in result.stdout
print("PASS: archive install from another directory, both Compose variants, validation/pull/up failures, retained config and data")
