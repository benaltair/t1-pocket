# T1 companion restores a future clock from saved contacts

**Proposed PR title** — Skip contact-based clock initialization on the Heltec T1 companion.

On a Heltec Mesh Node T1 running stock companion v1.18.0, the clock returned to February 2103 after reboot. The largest saved contact modification timestamp was 4201255210. The companion startup code uses the largest modification timestamp plus one, and its set-time command rejects a lower value. A local sync on the affected stock node was rejected. The T1 has no battery-backed calendar RTC. The earlier IMU/DS3231 detection fix is already in this release. [Contact bootstrap](https://github.com/meshcore-dev/MeshCore/blob/companion-v1.18.0/src/helpers/BaseChatMesh.cpp#L54-L64) · [I²C detection fix](https://github.com/meshcore-dev/MeshCore/pull/3301)

The attached patch skips this startup step on the T1 companion. Contact loading, stored timestamps, and other device targets retain their existing behaviour. On reboot the T1 uses its normal unsynced baseline until a phone or GPS supplies time. This proposal addresses the recurrent startup failure. It does not change the live command’s backwards-correction policy or add pocket screens. Those are separate review topics.

## Try the change

From a checkout of upstream MeshCore, create a branch at the reviewed development commit, then apply the patch from this directory.

```sh
git switch -c fix/t1-contact-clock ae283810e16b0231964c494aa954fb22ce75639e
git apply --check /path/to/t1-contact-clock.patch
git apply /path/to/t1-contact-clock.patch
pio run -e Heltec_t1_companion_radio_ble -e Heltec_t1_companion_radio_usb
```

Both official T1 companion targets compiled successfully with this patch on the reviewed development commit. On a backed-up test T1 with existing future-dated contact metadata, reboot with GPS off, read the clock, sync from the companion app and read it again after a minute. Verify the node’s public identity and contact list after the restart. The full pocket firmware passed the clock and preservation checks on one affected T1. The isolated upstream candidate still needs that separate hardware check before describing it to maintainers as tested on-device.

MeshCore accepts small fixes as individual PRs, and asks contributors to discuss larger features in an issue first. Keep this change independent of display preferences, advert routing defaults and USB/Bluetooth multiplexing. [Contribution guide](https://github.com/meshcore-dev/MeshCore/blob/dev/CONTRIBUTING.md)
