# Cascade
Cascade is a simple c++ implementation of Klondike Solitaire. The goal was to create an implementation that is terminal and keyboard focused.

<img width="2558" height="1435" alt="image" src="https://github.com/user-attachments/assets/15e8677b-71fa-454a-b263-5314b347bfb0" />


# Disclaimer
The code might suck. In fact, there is a high probability that it does. If you happen to open display.cpp... I'm sorry.

## Installation

### Method 1: Install via `.deb` Package (Recommended)

For Debian, Ubuntu, and derivative systems (such as MX Linux). This installs the binary to `/usr/bin/cascade`, creates an application menu shortcut, and automatically resolves the `lxterminal` runtime dependency.

```bash
# Clone the repository
git clone https://github.com/kajeko2137/Cascade.git
cd Cascade

# Install the prebuilt package
sudo apt update
sudo apt install ./.deb/cascade_1.2.2_amd64.deb
```

Run Cascade from anywhere via the terminal or through your desktop application launcher:

```bash
cascade
```

To remove the package later:

```bash
sudo apt remove cascade
```

---

### Method 2: Build from Source

If you are running a non-Debian distribution or prefer compiling directly from the source tree:

#### 1. Prerequisites

Ensure you have a C++ compiler, `make`, `lxterminal`, and `xrandr` installed:

```bash
# Debian / Ubuntu / MX Linux
sudo apt update
sudo apt install build-essential lxterminal x11-xserver-utils

# Fedora / RHEL
sudo dnf install gcc-c++ make lxterminal xorg-x11-server-utils

# Arch Linux
sudo pacman -S base-devel lxterminal xorg-xrandr
```

#### 2. Compile

From the project root:

```bash
make
```

The compiled binary will be generated inside the `bin/` directory.

#### 3. Run

```bash
./bin/cascade
```

## Features

- **Keyboard-Driven Gameplay**: Fully playable without a mouse using dedicated keyboard controls.
- **Dual Control Profiles**: Switch seamlessly between Standard (laptop/compact) and dedicated Numpad layouts (`P`).
- **Auto-Foundation Sweep**: Automatically sweeps eligible cards into foundations with animated pacing (`+` on Standard, `/` on Numpad).
- **Deadlock Detection**: Analyzes game state reachability to notify you when no further moves or progress are possible.
- **Persistent Leaderboard**: Automatically tracks and ranks your winning games by move count and time.
- **Flicker-Free Terminal Graphics**: Responsive Unicode card rendering and targeted screen updates.
