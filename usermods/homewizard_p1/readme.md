# HomeWizard P1 usermod

Visualizes your home's live grid power on a LED strip using a
[HomeWizard P1 meter](https://www.homewizard.com/p1-meter/) in the same network.

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
   (On ESP8266 discovery is not supported; set *host* manually.)
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

Flow direction is tied to import/export; use the segment's *reverse* option if
it should run the other way on your physical strip.

All transitions are smooth: the power value is low-pass filtered (~1.2 s), the
color fades white → purple/green proportionally with power, and the pulses
gradually emerge from the neutral breathing rather than snapping in at the
deadband edge.

## Technical notes

- Uses the HomeWizard [local API v1](https://api-documentation.homewizard.com/docs/v1/)
  endpoint `GET /api/v1/data`, field `active_power_w` (positive = import, negative = export).
- The HTTP request is fully asynchronous (AsyncClient). TCP callbacks only
  buffer data; parsing and all WLED state changes happen on the main loop task.
- mDNS discovery blocks for a few seconds, so it only runs while no meter is
  known, with a 30 s retry backoff. The LEDs may pause briefly during discovery.
