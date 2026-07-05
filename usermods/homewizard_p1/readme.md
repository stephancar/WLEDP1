# HomeWizard P1 usermod

Visualizes your home's live grid power on a LED strip using a
[HomeWizard P1 meter](https://www.homewizard.com/p1-meter/) in the same network.

Requires an ESP32 (any variant with WiFi); the networking runs on a dedicated
FreeRTOS task, which the ESP8266 does not support.

- **Offtake / import** (consuming from the grid): purple pulses flowing toward the segment start; more power = faster and brighter.
- **Injection / export** (feeding into the grid, e.g. solar): green pulses flowing toward the segment end.
- **Neutral** (within the deadband around 0 W): calm white breathing.
- **No data** (meter unreachable): dim white breathing.

## Setup

1. Enable the **Local API** for the P1 meter in the HomeWizard Energy app
   (Settings → Meters → your P1 meter → Local API).
2. Build WLED with `custom_usermods = homewizard_p1` and flash it.
3. In WLED, go to Config → Usermods → HomeWizard P1. With *autoDiscover*
   enabled and *host* left empty, the meter is found automatically via mDNS
   (`_hwenergy._tcp`). Alternatively enter the meter's IP address as *host*.
4. Select the **Grid Flow** effect on your segment.

## Settings

| Setting | Meaning |
|---|---|
| `enabled` | Master switch for polling |
| `host` | IP/hostname of the P1 meter; leave empty for mDNS auto-discovery |
| `autoDiscover` | Search for the meter via mDNS when no host is set |
| `updateIntervalMs` | Poll interval in ms (min 1000; the v1 API updates about once per second) |
| `deadbandW` | Power band around 0 W treated as neutral |
| `fullScaleW` | Power at which color and flow speed reach their maximum |

## Effect sliders (Grid Flow)

- **Speed**: scales the overall flow speed (the actual speed also grows with power).
- **Pulses**: how many pulses travel along the segment.
- **Trail**: length of the fading tail behind each pulse.
- **Blur**: softens the pulses for a smoother look.

## Troubleshooting

White (the neutral state) lights all three LED channels and is the
maximum-current state, so power problems tend to show up exactly when the
strip turns white:

- **Device browns out / reboots at the white transition**: set a realistic
  PSU limit in Config → LED Preferences (ABL), leaving ~250 mA headroom for
  the ESP32 itself.
- **Light switches itself off (and maybe shows a random color) and stays off
  until a power cycle**: the current step can couple electrical noise into
  GPIO0, where WLED's default button lives — phantom "presses" toggle the
  power off (and a phantom long-press sets a random color). Once off, no
  current flows, so no phantom press ever turns it back on. If you don't use
  a physical button, remove the GPIO0 button in Config → LED Preferences →
  buttons (set it to disabled), or move it to another pin.
- **Device freezes hard (unreachable, serial silent, no crash log) when large
  appliances switch**: mains transients from multi-kW loads can latch up the
  ESP32 — inherently more likely with this usermod, since your light is by
  definition near big switching loads. Mitigate in hardware (330 Ω series
  resistor in the data line, ≥1000 µF capacitor across the strip's 5 V input,
  short data wires, a 74AHCT125 level shifter) and build with
  `-D WLED_WATCHDOG_TIMEOUT=10` so any remaining freeze self-recovers by
  reboot instead of waiting for a power cycle.

Flow direction is tied to import/export; use the segment's *reverse* option if
it should run the other way on your physical strip.

All transitions are smooth: the power value is low-pass filtered (~1.2 s), the
color fades white → purple/green proportionally with power, and the pulses
gradually emerge from the neutral breathing rather than snapping in at the
deadband edge.

## Development: P1 simulator

[`p1_simulator.py`](p1_simulator.py) serves a fake local API v1 on port 8123
that cycles injection (-800 W) → neutral (10 W) → offtake (+1200 W) every 10
seconds. Run it on a PC in the same network (`python p1_simulator.py`) and set
the usermod *host* to `<pc-ip>:8123` to test all states without a real meter.
Building with `-D WLED_ENABLE_JSONLIVE` additionally lets you verify the
rendered colors over HTTP via `/json/live`.

## Technical notes

- Uses the HomeWizard [local API v1](https://api-documentation.homewizard.com/docs/v1/)
  endpoint `GET /api/v1/data`, field `active_power_w` (positive = import, negative = export).
- All networking (mDNS discovery and HTTP polling) runs on a dedicated
  low-priority FreeRTOS task pinned to core 0, so blocking calls can never
  stall the LED pipeline. The task publishes plain 32-bit values (atomic on
  ESP32); host strings are exchanged under a mutex. ESP32 only.
- The pulse position is integrated over time (not derived from `time * speed`),
  so speed changes shift the animation continuously without jumps.
