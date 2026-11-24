#!/usr/bin/env python3
"""Build an application-only Heltec T1 BLE companion from a released MeshCore tag."""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import tempfile
import zipfile
from pathlib import Path
from pocket_settings import load_settings, write_header
from verify import verify, verify_dfu, verify_usb_entry

KIT = Path(__file__).resolve().parent
UPSTREAM = "https://github.com/meshcore-dev/MeshCore.git"
TARGET = "Heltec_t1_companion_radio_ble"
PIN = json.loads((KIT / "upstream.json").read_text())
TAG = re.compile(r"companion-v(\d+)\.(\d+)\.(\d+)")

def run(*args, cwd=None, **kwargs):
    return subprocess.run(args, cwd=cwd, check=True, text=True, **kwargs)

def output(*args, cwd=None):
    return run(*args, cwd=cwd, stdout=subprocess.PIPE).stdout.strip()

def latest(remote):
    lines = output("git", "ls-remote", "--refs", "--tags", remote, "refs/tags/companion-v*")
    tags = [line.split("refs/tags/")[-1] for line in lines.splitlines()]
    return max((tag for tag in tags if TAG.fullmatch(tag)),
               key=lambda tag: tuple(map(int,TAG.fullmatch(tag).groups())))

def select_patches(profile):
    patches = sorted((KIT / "patches").glob("*.patch"))
    for patch in patches:
        if profile == "clock" and patch.name != "01-clock.patch":
            continue
        if profile == "pocket" and patch.name not in ("01-clock.patch", "02-screen.patch", "06-advert.patch", "07-pocket-controls.patch", "08-beeper.patch"):
            continue
        yield patch

def apply_and_test(repo, profile, patches):
    for patch in patches:
        run("git", "apply", "--check", patch, cwd=repo)
        run("git", "apply", patch, cwd=repo)
    run("c++", "-std=c++11", "-I", KIT/"tests", "-I", repo/"src", "-I", repo/"variants/heltec_t1",
        KIT/"tests/clock.cpp", "-o", repo/"clock-test", cwd=repo)
    run(repo/"clock-test")
    if profile in ("pocket", "dual"):
        run("c++","-std=c++11","-funsigned-char","-I",KIT/"tests","-I",repo/"src","-I",repo/"variants/heltec_t1",
            KIT/"tests/screen.cpp","-o",repo/"screen-test",cwd=repo)
        run(repo/"screen-test",stdout=subprocess.DEVNULL)
        run("c++", "-std=c++11", "-I", repo/"variants/heltec_t1",
            KIT/"tests/buttons.cpp", "-o", repo/"buttons-test", cwd=repo)
        run(repo/"buttons-test")
    if profile == "dual":
        run("c++", "-std=c++11", "-I", KIT/"tests", "-I", repo/"src", "-I", repo/"variants/heltec_t1",
            KIT/"tests/ports.cpp", "-o", repo/"port-test", cwd=repo)
        run(repo/"port-test")
    config=output("git", "diff", "--check", cwd=repo)
    if config: raise ValueError(config)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    select=parser.add_mutually_exclusive_group()
    select.add_argument("--release", default=PIN["tag"])
    select.add_argument("--latest", action="store_true", help="Select newest stable companion tag")
    select.add_argument("--latest-pin", action="store_true", help="Print newest stable tag and exact commit as JSON")
    parser.add_argument("--expect-commit", help="Require the fetched tag to resolve to this commit")
    parser.add_argument("--config", type=Path, help="JSON preferences (default: pocket.local.json if present, otherwise pocket.example.json)")
    parser.add_argument("--profile", choices=("dual","pocket","clock"), default="dual")
    parser.add_argument("--source", default=UPSTREAM, help="Official repo URL or local Git release mirror")
    parser.add_argument("--check-only", action="store_true", help="Apply patches and run host regression")
    parser.add_argument("--out", type=Path, default=Path("builds"))
    args=parser.parse_args()
    if Path(args.source).is_dir(): args.source=str(Path(args.source).resolve())
    tag=latest(args.source) if args.latest or args.latest_pin else args.release
    if not TAG.fullmatch(tag): parser.error("Expected a stable companion-vX.Y.Z tag")
    try:
        settings = load_settings(args.config)
    except (OSError, ValueError) as err:
        parser.error(str(err))
    patches = list(select_patches(args.profile))
    digest = hashlib.sha256(json.dumps(settings, sort_keys=True).encode())
    for patch in patches:
        digest.update(patch.read_bytes())
    version = f"{tag.removeprefix('companion-')}-t1{digest.hexdigest()[:6]}"
    if len(version)>19: raise ValueError("Version exceeds companion wire field")
    with tempfile.TemporaryDirectory(prefix="heltec-t1-") as tmp:
        repo=Path(tmp)/"MeshCore"
        run("git","init","--quiet",repo)
        run("git","fetch","--depth=1",args.source,"refs/tags/"+tag,cwd=repo)
        run("git","checkout","--quiet","--detach","FETCH_HEAD",cwd=repo)
        commit=output("git","rev-parse","HEAD",cwd=repo)
        if tag==PIN["tag"] and PIN["commit"]!=commit: raise ValueError("Published baseline tag changed")
        if args.expect_commit and commit!=args.expect_commit: raise ValueError("Unexpected upstream commit")
        if args.latest_pin:
            print(json.dumps(dict(tag=tag,commit=commit),indent=2))
            return
        write_header(repo/"variants/heltec_t1", settings)
        apply_and_test(repo,args.profile,patches)
        print(f"{tag} {commit} | {args.profile} patches and host tests passed",flush=True)
        if args.check_only: return
        pio=shutil.which("pio") or shutil.which("platformio")
        if not pio: raise SystemExit("Install PlatformIO Core and make pio available on PATH")
        env=os.environ.copy()
        env.pop("PLATFORMIO_BUILD_DIR",None)
        env["PLATFORMIO_BUILD_FLAGS"] = f"""-DFIRMWARE_VERSION='"{version}"'"""
        run(pio,"run","-e",TARGET,"-t","create_uf2","-t","buildprog",cwd=repo,env=env)
        build=repo/".pio"/"build"/TARGET
        manifest=verify(build/"firmware.uf2",build/"firmware.hex")
        manifest["dfu_sha256"] = verify_dfu(build/"firmware.zip",build/"firmware.uf2")
        with zipfile.ZipFile(build/"firmware.zip") as package:
            app = json.loads(package.read("manifest.json"))["manifest"]["application"]
            if version.encode() not in package.read(app["bin_file"]):
                raise ValueError("Embedded version missing from the installed program")
        if args.profile == "dual":
            verify_usb_entry(build/"firmware.elf")
        manifest.update(target=TARGET,version=version,tag=tag,commit=commit,profile=args.profile)
        destination=args.out.resolve()/version
        destination.parent.mkdir(parents=True,exist_ok=True)
        if destination.exists(): raise FileExistsError(destination)
        with tempfile.TemporaryDirectory(dir=destination.parent) as staging:
            product=Path(staging)/version
            product.mkdir()
            shutil.copyfile(build/"firmware.uf2",product/(version+".uf2"))
            shutil.copyfile(build/"firmware.zip",product/(version+"-dfu.zip"))
            (product/"manifest.json").write_text(json.dumps(manifest,indent=2)+"\n")
            product.rename(destination)
        print(destination)
if __name__=="__main__":
    main()
