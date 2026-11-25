**[Download T1 Pocket for USB (.uf2)]({download_url})** · `{filename}`

For a **Heltec Mesh Node T1 already running a MeshCore companion**. This public build uses the UTC clock, flood adverts, and USB or Bluetooth.

1. Let your phone receive pending messages and back up your node before restarting it.
2. Plug the T1 into your computer with a USB **data** cable. Quickly double-press the **rear power button**. A USB drive appears in Finder or File Explorer. This is firmware update (DFU) mode.
3. Open `INFO_UF2.TXT` on that drive. Confirm `Board-ID: mesh-node-t1` and `SoftDevice: S140 6.1.1`. Drag the downloaded **`.uf2`** onto the drive. Leave the cable plugged in while it copies. The drive disappears as the T1 restarts.
4. Home appears on the T1. Reconnect with the MeshCore app, sync the clock and check your contacts and channels. If you need a Bluetooth PIN, tap the lower side button once from Home.

**Roll back to stock.** {stock_instruction} You can also select **Heltec Mesh Node T1 → Companion / Bluetooth** in the [official MeshCore flasher](https://flasher.meshcore.io/). Leave **Erase device** unchecked to preserve your identity and data.

The other ZIP files are for Nordic DFU or source code. The USB drive takes the UF2 linked above. [Screenshots, customization and source](https://github.com/{repo}#screen-preview) · [Checksums]({manifest_url})
