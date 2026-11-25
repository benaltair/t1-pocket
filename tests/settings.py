"""Check partial overrides, invalid input and the private/public settings boundary."""
import json
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import pocket_settings as prefs

with tempfile.TemporaryDirectory() as tmp:
    root = Path(tmp)
    local = root / "settings.local.json"
    with patch.object(prefs, "LOCAL", local):
        defaults = prefs.load_settings()
        assert defaults["screen_timeout_seconds"] == 30 and defaults["advert_scope"] == "flood"
        assert defaults["screen_resume_seconds"] == 15 and defaults["screen_on_at_boot"] is True
        local.write_text('{"utc_offset_minutes": 345, "screen_timeout_seconds": 5, "screen_resume_seconds": 0, "screen_on_at_boot": false, "advert_scope": "zero-hop"}')
        custom = prefs.load_settings()
        assert custom["clock_label"] == "+05:45" and custom["advert_scope"] == "zero-hop"
        assert prefs.load_settings(prefs.EXAMPLE) == defaults
        prefs.write_header(root, custom)
        assert "#define T1_SCREEN_TIMEOUT_MS 5000UL" in (root / "T1BuildSettings.h").read_text()
        header = (root / "T1BuildSettings.h").read_text()
        assert "resumeSeconds = 0;" in header and "screenOnAtBoot = false;" in header and "floodAdvert = false;" in header
        assert defaults["battery_average_samples"] == 8
        for bad in ({"battery_average_samples": 0}, {"battery_average_samples": 33},
                    {"battery_average_samples": True}, {"screen_resume_seconds": -1}, {"screen_resume_seconds": 3601}, {"screen_on_at_boot": 1},{"screen_timeout_seconds": True}, {"screen_timeout_seconds": 0},
                    {"utc_offset_minutes": 841}, {"clock_label": 'bad"'},
                    {"advert_scope": "unknown"}, {"typo": 1}, []):
            local.write_text(json.dumps(bad))
            try:
                prefs.load_settings()
            except ValueError:
                continue
            raise AssertionError(f"Accepted invalid settings: {bad}")
print("Build settings checks passed.")
