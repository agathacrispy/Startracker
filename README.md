# Startracker

A camera mount that looks up where a planet or star is in the sky and points a camera at it.

## How it works

The mount moves on two axes: altitude (up/down) and azimuth (left/right, relative to north).

A local Python script does the math: given an object (a planet, the Moon, or a star), it calculates that object's current altitude and azimuth for your exact location and sends the mount there over a serial connection to an Arduino, which drives the stepper motor. 

Added option to `track` something instead of just `goto` it.

Since the mount's "home" direction is set with a compass rather than true north, there's also a small correction step that accounts for magnetic declination.

## Hardware

- A stepper motor + driver
- An Arduino for the motor control
- A camera tripod
- Gears for the drivetrain
- 3D-printed parts (STL files in `models/`)

## Software

Install the Python dependencies with:

```
pip install skyfield pyserial
```

The first time you run it, Skyfield will download the ephemeris and star catalog data. After that, it's cached locally and doesn't need the internet again.

## Setup

Open `StarTracker/main.py` and set these to match your setup:

```python
PORT = "COM3"        # whatever serial port your Arduino shows up on
LAT_DEG = 43.88       # your latitude
LON_DEG = -78.94      # your longitude
ELEV_M  = 100.0       # your elevation, in meters
```

Then run it:

```
python main.py
```

## Usage

It drops you into a small command prompt:

| Command | What it does |
|---|---|
| `sync` | Tell the mount its current position is correct |
| `where` | Ask the mount where it currently thinks it's pointing |
| `sethome` | Save the current position as home |
| `gohome` | Return to the saved home position |
| `goto <az> <alt>` | Point at a specific azimuth/altitude, in degrees |
| `goto <object>` | Point at a named object |
| `track <object> [hz]` | Keep following an object (defaults to 2 updates/sec); Ctrl+C to stop |
| `quit` | Exit |

`<object>` can be:
- A planet, the sun, or the moon, by name (`sun`, `moon`, `mars`, `jupiter`, etc.)
- A named bright star (`polaris`, `sirius`, `vega`, `betelgeuse`, `rigel`)
- Any other star by its Hipparcos catalog number (`hip:91262`), or just the number on its own

Example:

```
> sethome
> goto moon
> track jupiter
```
