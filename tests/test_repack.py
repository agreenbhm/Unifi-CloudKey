#!/usr/bin/env python3
"""Run with Python 3, JDK 17, and dpkg-deb installed; no UniFi download required."""
import hashlib
from pathlib import Path
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
EXPORTS = ["--add-exports", "java.base/jdk.internal.org.objectweb.asm=ALL-UNNAMED",
           "--add-exports", "java.base/jdk.internal.org.objectweb.asm.tree=ALL-UNNAMED"]


def run(*args, success=True):
    result = subprocess.run(args, text=True, capture_output=True)
    if (result.returncode == 0) != success:
        raise AssertionError(f"Unexpected status {result.returncode}: {args}\n{result.stdout}{result.stderr}")
    return result


def unchanged_except_launcher(original, patched):
    with zipfile.ZipFile(original) as before, zipfile.ZipFile(patched) as after:
        assert before.namelist() == after.namelist()
        changed = [name for name in before.namelist() if before.read(name) != after.read(name)]
        assert changed == ["com/ubnt/ace/Launcher.class"], changed


with tempfile.TemporaryDirectory() as directory:
    temp = Path(directory)
    classes = temp / "classes"
    run("javac", *EXPORTS, "-d", str(classes), str(ROOT / "tools/PatchUnifi.java"),
        str(ROOT / "tests/PatchUnifiTest.java"))
    fixture = temp / "fixture.jar"
    run("java", *EXPORTS, "-cp", str(classes), "PatchUnifiTest", str(fixture))
    patch_command = ["java", *EXPORTS, "-cp", str(classes), "PatchUnifi"]
    patched = temp / "patched.jar"
    run(*patch_command, str(fixture), str(patched))
    unchanged_except_launcher(fixture, patched)
    for source in [patched, temp / "missing.jar", temp / "signed.jar"]:
        if source.name == "missing.jar":
            with zipfile.ZipFile(source, "w") as jar:
                jar.writestr("irrelevant", b"unchanged")
        elif source.name == "signed.jar":
            source.write_bytes(fixture.read_bytes())
            with zipfile.ZipFile(source, "a") as jar:
                jar.writestr("META-INF/TEST.SF", b"signature")
        rejected_output = temp / "rejected.jar"
        run(*patch_command, str(source), str(rejected_output), success=False)
        assert not rejected_output.exists()

    package = temp / "package"
    control = package / "DEBIAN"
    library = package / "usr/lib/unifi/lib"
    control.mkdir(parents=True)
    library.mkdir(parents=True)
    (control / "control").write_text(
        "Package: unifi-patch-fixture\nVersion: 1.0\nArchitecture: all\n"
        "Maintainer: Test <test@example.invalid>\nDescription: Patch integration fixture\n")
    (control / "preinst").write_text("#!/bin/sh\nexit 1\n")
    (control / "preinst").chmod(0o755)
    original_jar = library / "ace.jar"
    original_jar.write_bytes(fixture.read_bytes())
    (library / "keep.txt").write_text("package payload unchanged\n")
    checksums = "".join(f"{hashlib.md5(path.read_bytes()).hexdigest()}  {path.relative_to(package)}\n"
                        for path in [original_jar, library / "keep.txt"])
    (control / "md5sums").write_text(checksums)
    deb = temp / "fixture.deb"
    run("dpkg-deb", "-b", str(package), str(deb))
    run("bash", str(ROOT / "repack_unifi.sh"), str(deb))
    extracted = temp / "extracted"
    run("dpkg-deb", "-R", str(temp / "fixture-modified.deb"), str(extracted))
    assert not (extracted / "DEBIAN/preinst").exists()
    unchanged_except_launcher(fixture, extracted / "usr/lib/unifi/lib/ace.jar")
    assert (extracted / "usr/lib/unifi/lib/keep.txt").read_bytes() == (library / "keep.txt").read_bytes()
    for line in (extracted / "DEBIAN/md5sums").read_text().splitlines():
        checksum, path = line.split(None, 1)
        assert hashlib.md5((extracted / path).read_bytes()).hexdigest() == checksum
    # An unrecognized application must stop repackaging before any modified .deb is produced.
    original_jar.write_bytes(patched.read_bytes())
    unknown = temp / "unknown.deb"
    run("dpkg-deb", "-b", str(package), str(unknown))
    run("bash", str(ROOT / "repack_unifi.sh"), str(unknown), success=False)
    assert not (temp / "unknown-modified.deb").exists()
print("PASS: gate verification, archive preservation, signed/unknown rejection, package repackaging and checksums")
