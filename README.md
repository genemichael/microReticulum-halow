# microReticulum-halow

A [microReticulum](https://github.com/attermann/microReticulum) node on the Heltec RC32-68
(ESP32-S3 with a Morse Micro MM6108 Wi-Fi HaLow radio), built as an ESP-IDF project with no Arduino layer.

**Status: bench experiment, not a product.** One thing has been shown on hardware, once (2026-11-16):
an RC32-68 running this image joined an 802.11s HaLow mesh, opened a Reticulum TCP link across the mesh
to a Python Reticulum on a Raspberry Pi, announced a destination, and the Pi found a one-hop path to it.
Nothing else is proven. See "Not shown" below.

## What is in here

| Path | What | Licence |
|---|---|---|
| `examples/mesh/main/TcpClientInterface.*` | Reticulum TCP client interface on lwIP sockets (HDLC framing) | GPL-3.0 |
| `examples/mesh/main/rns_task.cpp` | Starts Reticulum once the mesh interface is up, announces, prints status | GPL-3.0 |
| `components/urns/` | Build glue that compiles microReticulum and its dependencies from `deps/` | GPL-3.0 |
| `boards/rc32-68/` | RC32-68 pin map and settings | GPL-3.0 |
| `build.sh`, `flash.sh`, `tools/` | Scripts | GPL-3.0 |
| `examples/`, `cmake/`, `Makefile` | Copied from [teapotlaboratories/esp-halow-examples](https://github.com/teapotlaboratories/esp-halow-examples) at `86cb8c09`, with local changes | Apache-2.0, see `LICENSES/` |
| `components/halow/` | Copied from [teapotlaboratories/mm-esp32-halow](https://github.com/teapotlaboratories/mm-esp32-halow) at `c9f3a572` (a patched morselib 2.12.3 with 802.11s mesh), with local changes | Apache-2.0 and others, see the licence files inside it |
| `vendor/morse-firmware/` | Copied unmodified from [MorseMicro/morse-firmware](https://github.com/MorseMicro/morse-firmware) at `fd41e1cf` | Morse Micro Binary Distribution Licence, see `vendor/morse-firmware/firmware/LICENSE` |

The local changes to the two Teapot Laboratories trees are recorded as patches in `docs/patches/`:

- `halow-local-changes.patch`: a larger buffer so the node accepts the longer protected block that a Linux
  802.11s node sends in its peering Open, plus diagnostic log lines.
- `examples-local-changes.patch`: accept a ready-made calibration file, debug logging, a link ordering fix.

The mesh example's `app_main.c` also quiets the radio's IRQ pin (GPIO14) and drives GPIO3 high to power the
HaLow regulator before the radio starts. Both are needed on the RC32-68.

The Teapot Laboratories fork is called experimental and not production-ready by its authors.
Its original README is kept as `docs/UPSTREAM-README-esp-halow-examples.md`.

## What is not in here

- **Heltec's calibration file** (`bcf_HC01_V2_3V3.mbin`). Its redistribution terms are not known to us, so it is
  not included. It comes from Heltec's `ESP_HaLow` repository, `tools/bcf/libbcf_HC01_V2_L.a`:

  ```
  ar x libbcf_HC01_V2_L.a bcf_HC01_V2_3V3.mbin.o
  objcopy -O binary --only-section=.rodata._fw_mbin bcf_HC01_V2_3V3.mbin.o bcf_HC01_V2_3V3.mbin
  ```

  The build also expects `bcf_HC01_V2_3V3.mbin.o` next to the `.mbin`.
- **Network values.** The mesh name, subnet and Reticulum server address in git are placeholders.
  Real values go in `boards/rc32-68/sdkconfig.local`, which is git-ignored.
  Copy `boards/rc32-68/sdkconfig.local.example`.
- **microReticulum and its dependencies.** Fetched into `deps/` (git-ignored) by `tools/fetch_deps.sh`
  at the commits and tags this was built with.

## Build

Needs ESP-IDF v5.4.2 installed (default location `~/esp/esp-idf`, override with `IDF_PATH`).

```
./tools/fetch_deps.sh
cp boards/rc32-68/sdkconfig.local.example boards/rc32-68/sdkconfig.local   # then edit it
RC_BCF_MBIN=/path/to/bcf_HC01_V2_3V3.mbin ./build.sh
```

The image is `build/mesh/rc32-68/mesh.bin`, about 2.16 MB. The log is `build.log`.

## Flash

```
PORT=/dev/cu.usbmodemXXX ./flash.sh
```

This writes the bootloader, the partition table and the app. Be sure which board is on the port.

## What the image does

1. Brings up the 802.11s mesh interface and gives itself a static address in the configured /24
   (host part derived from its MAC).
2. When the interface is up, starts a microReticulum transport node with one TCP client interface to
   `CONFIG_RNS_TCP_HOST:CONFIG_RNS_TCP_PORT`.
3. Creates a destination `rc32.bench`, prints its hash, announces when the TCP link comes up and then every
   `CONFIG_RNS_ANNOUNCE_INTERVAL_S` seconds.
4. Prints a status line every 10 s with TCP state, frame counts and free heap.

On the Reticulum server side, `rnpath <hash>` should report a path.

## Things to know

- **Mesh passphrase.** The fork has one fixed mesh passphrase compiled into the library
  (`MESH_SAE_PASSWORD` in `umac/mesh/umac_mesh.c`). It is public. Every node built from this source shares it
  until that is changed. Do not treat a mesh built from this as private.
- **Linux mesh peers** (for example OpenWrt HaLow routers) needed management frame protection switched off on
  their mesh interface (`ieee80211w=0`) to pass traffic with this node.
- **Random numbers.** Without the Arduino layer the Crypto library finds no hardware random source and warns
  at compile time. `rns_task.cpp` feeds the ESP32's hardware generator into the library's generator after it
  starts and before any key is made. This has been read, not tested.
- **No storage.** The node gets a new identity at each restart, and keeps no path table across restarts.
- **Three workarounds for building microReticulum under plain ESP-IDF** live in `components/urns/CMakeLists.txt`
  and `rns_task.cpp`: two warnings demoted from errors, the library's bundled TLSF allocator renamed by macro,
  and a `nanosleep()` supplied by the app.

## Not shown

- A Reticulum link, a message, or a resource transfer. Only announce and path request were exercised.
- Forwarding between two Reticulum interfaces on the board.
- More than about ten minutes of running. Free heap drifted down by about 9 KB a minute in that time;
  the cause is not known.
- More than one radio hop.
- Two RC32-68s talking to each other.
- Power draw. Nothing here is tuned for power.
- The exact image in this repository on hardware. The image that passed on the bench was built from the same
  sources before the network values were moved into configuration options.

## Licence

The code written for this repository is GPL-3.0 (`LICENSE`). The TCP interface follows the framing used in
[microReticulum_Firmware](https://github.com/attermann/microReticulum_Firmware), which is GPL-3.0.
Third-party trees keep their own licences as listed in the table above.
