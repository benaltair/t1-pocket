"""Build-time display and advert settings for the T1 companion."""
import json
import re
from pathlib import Path

EXAMPLE = Path(__file__).with_name("pocket.example.json")
LOCAL = EXAMPLE.with_name("pocket.local.json")


def load_settings(path=None):
    defaults = json.loads(EXAMPLE.read_text())
    if path is None:
        path = LOCAL if LOCAL.exists() else EXAMPLE
    overrides = json.loads(Path(path).read_text())
    if not isinstance(overrides, dict):
        raise ValueError("Expected a JSON settings object")
    unknown = overrides.keys() - defaults.keys()
    if unknown:
        raise ValueError("Unknown settings: " + ", ".join(sorted(unknown)))
    values = {**defaults, **overrides}
    for key, low, high in (("screen_timeout_seconds", 1, 3600), ("utc_offset_minutes", -720, 840)):
        value = values[key]
        if type(value) is not int or not low <= value <= high:
            raise ValueError(f"{key} must be an integer between {low} and {high}")
    if "utc_offset_minutes" in overrides and "clock_label" not in overrides:
        minutes = values["utc_offset_minutes"]
        if minutes:
            hours, remainder = divmod(abs(minutes), 60)
            values["clock_label"] = f"{'+' if minutes > 0 else '-'}{hours:02}:{remainder:02}"
    label = values["clock_label"]
    if not isinstance(label, str) or not re.fullmatch(r"[A-Za-z0-9+:-]{1,8}", label):
        raise ValueError("clock_label must be 1–8 letters, digits or + - :")
    if values["advert_scope"] not in ("flood", "zero-hop"):
        raise ValueError("advert_scope must be flood or zero-hop")
    return values


def write_header(directory, values):
    text = (
        "#pragma once\n"
        f"#define T1_SCREEN_TIMEOUT_MS {values['screen_timeout_seconds'] * 1000}UL\n"
        "namespace T1Build {\n"
        f"  constexpr int utcOffsetMinutes = {values['utc_offset_minutes']};\n"
        f"  constexpr char clockLabel[] = {json.dumps(values['clock_label'])};\n"
        f"  constexpr bool floodAdvert = {str(values['advert_scope'] == 'flood').lower()};\n"
        "}\n"
    )
    (directory / "T1BuildSettings.h").write_text(text)
