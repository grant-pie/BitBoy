# BitBoy

**BitBoy** is a classic **8‑bit style oscillator** plugin for **VCV Rack**. It delivers crunchy digital tones with authentic bit‑crushing and NES‑style noise generation.

## 🎛️ Features

- **Waveforms:** Square, triangle, sawtooth, and noise
- **Lo‑fi character:** Bit‑crush style aliasing and digital grit
- **Compact plugin:** Easy to use with minimal controls

## 🧩 Included Module

| Module | Description |
|-------|-------------|
| `BitOscillator` | Classic 8‑bit style oscillator with square, triangle, sawtooth, and noise waveforms. Features authentic bit‑crushing and NES‑style noise generation. |

## 🏗️ Building

### Prerequisites

- [VCV Rack](https://vcvrack.com/) (compatible version, usually Rack 2+)
- A C++ toolchain supported by Rack (e.g., MSYS2/MinGW on Windows)

### Build steps

1. Open a terminal in this project folder.
2. Set `RACK_DIR` to your Rack SDK directory if it is not two levels up from this repo.

```sh
# Example (Windows/MSYS2)
export RACK_DIR="C:/Program Files/Rack2-SDK"
make
```

3. The compiled plugin (`BitBoy.dll` / `BitBoy.so` / `BitBoy.dylib`) will be placed into `bin/` (as managed by Rack's build system).

### Packaging

To create a distributable ZIP package (for uploading to a plugin library or sharing):

```sh
make dist
```

## 🚀 Installing

Copy the compiled plugin file (`BitBoy.*`) into your Rack `plugins/` folder, then restart Rack.

## 🔧 Usage

Load the `8-Bit Oscillator` module from the module browser in VCV Rack and patch it like any other oscillator. Use the waveform selector to choose between square/triangle/saw/noise and adjust the pitch and bit‑crush character to taste.

## 📄 License

This plugin is marked as **proprietary** in `plugin.json`. Check `LICENSE*` files in this repo for additional licensing details.

---

If you want to add more modules or improve the sound design, feel free to open an issue or contribute changes.
