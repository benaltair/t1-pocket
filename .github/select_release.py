"""Resolve the stable upstream tag on scheduled runs, or test the pinned PR baseline."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys

pin = json.loads(Path("upstream.json").read_text())
event = os.environ["GITHUB_EVENT_NAME"]
candidate = pin
if event in ("schedule", "workflow_dispatch"):
    candidate = json.loads(subprocess.check_output([sys.executable, "update.py", "--latest-pin"], text=True))
if not re.fullmatch(r"companion-v\d+\.\d+\.\d+", candidate["tag"]):
    raise ValueError("Expected a stable companion release tag")
if not re.fullmatch(r"[a-f0-9]{40}", candidate["commit"]):
    raise ValueError("Expected a full Git commit")
changed = candidate != pin
with open(os.environ["GITHUB_OUTPUT"], "a") as out:
    for key in ("tag", "commit"):
        print(f"{key}={candidate[key]}", file=out)
    print("update=" + str(changed).lower(), file=out)
    print("build=" + str(changed or event != "schedule").lower(), file=out)
