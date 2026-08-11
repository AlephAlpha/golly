

# Golly

**Golly** is a powerful, cross-platform application for exploring cellular automata, with a strong focus on Conway's Game of Life and related rules. It features a high-performance simulation engine, a full-featured desktop GUI, an Android app, and a headless command-line tool (`bgolly`) for batch processing and benchmarking.

## Features
- **Multiple Simulation Algorithms**: QuickLife, HashLife, Generations, Larger than Life (LTL), John von Neumann, Super/History/Investigator families, and RuleLoader (for custom table/tree rules).
- **Pattern I/O**: Native support for RLE, MC, and gzip-compressed formats.
- **Timeline Recording**: Record and play back step sequences with automatic pruning.
- **Scripting**: Built-in support for Python and Lua automation.
- **Cross-Platform**: Desktop builds for Windows, macOS, and Linux (via wxWidgets), plus a native Android client.
- **Headless CLI (`bgolly`)**: A lightweight, batch-mode executable for automated testing, benchmarking, and pipeline integration. Does not require GUI dependencies.

## Requirements
- **Compiler**: Modern C++ compiler (GCC, Clang, or MSVC)
- **wxWidgets**: `>= 3.1.5` (required for the desktop GUI only)
- **Python**: `>= 3.3` (64-bit, required for scripting support)
- **Optional**: SDL2 development headers (for sound support)
- **Note**: `bgolly` can be compiled without wxWidgets or Python.

## Building & Installation

### 1. Build Desktop GUI
The desktop version uses wxWidgets for its graphical interface.

1. **Install wxWidgets**: Download and compile wxWidgets for your platform. Ensure it is built with `--disable-shared` (static linking) and 64-bit architecture.
   - *Linux*: `sudo apt-get install libgtk-3-dev libcurl4-openssl-dev libgl1-mesa-dev`
   - *Mac*: Configure with `--with-osx_cocoa --disable-shared`
   - *Windows*: Edit `wxWidgets\build\msw\config.vc` and set `TARGET_CPU=X64`, `RUNTIME_LIBS=static`, `BUILD=release`

2. **Configure Golly**: Navigate to the `gui-wx/` directory.
   - Copy `local-win-template.mk` to `local-win.mk` (Windows)
   - Copy `local-gtk-template.mk` to `local-gtk.mk` (Linux)
   - Copy `makefile-mac` to `makefile` (macOS)
   - Edit the local file to point `WX_DIR` to your wxWidgets installation.

3. **Compile**:
   ```bash
   # Windows (use x64 Native Tools Command Prompt)
   nmake -f makefile-win

   # Linux
   make -f makefile-gtk

   # macOS
   make
   ```
   The executables will be placed in the repository root alongside the `Patterns/`, `Rules/`, `Scripts/`, and `Help/` directories.

### 2. Build `bgolly` (Command Line)
To build only the headless batch-mode tool without GUI dependencies:
```bash
# Linux
make -f makefile-gtk bgolly

# Windows
nmake -f makefile-win bgolly
```

## Usage

### Desktop Application
Launch the `golly` executable. Use the menu bar to load patterns (`Patterns/*.rle`), switch algorithms (`Control > Algorithm`), change rules (`Control > Rule`), record/play timelines, or execute Python/Lua scripts via the `Edit` and `Scripts` menus.

### `bgolly` (Command Line)
`bgolly` is designed for automation, benchmarking, and non-interactive simulation.

**Basic Syntax:**
```bash
bgolly [options] patternfile
```

**Common Options:**
| Flag | Description |
|------|-------------|
| `-m, --generation <N>` | Stop after N generations |
| `-i, --stepsize <N>` | Step size (generations per tick) |
| `-a, --algorithm <name>` | Force algorithm (e.g., `HashLife`, `QuickLife`) |
| `-r, --rule <string>` | Specify Life rule (e.g., `B3/S23`) |
| `-o, --output <file>` | Save pattern (`.rle`, `.mc`, `.rle.gz`, `.mc.gz`) |
| `-h, --hashlife` | Enable HashLife algorithm |
| `-b, --benchmark` | Print timing information |
| `-t, --timeline` | Enable timeline recording |
| `--exec <script>` | Run a command script (reads commands from file or `-` for stdin) |
| `-q, --quiet` | Suppress population output (use twice for silent mode) |
| `-M, --maxmemory <MB>` | Limit memory usage |

**Scripting via `bgolly`:**
You can automate simulations by passing a script to `--exec` or piping commands to stdin. Supported commands include `load <file>`, `step <N>`, `show`, `set <x> <y>`, `unset <x> <y>`, `copy <x1> <y1> <x2> <y2>`, `paste <x> <y>`, `new`, `sethashing <0|1>`, and `quit`.

**Example:**
```bash
# Run a pattern for 1,000,000 generations using HashLife, benchmarking performance
bgolly -m 1000000 -h -b patterns/glider.rle

# Run a custom test script
bgolly --exec test_commands.txt initial_pattern.rle
```

## Directory Structure
| Directory | Description |
|-----------|-------------|
| `gollybase/` | Core simulation engine, algorithm implementations, and I/O routines |
| `gui-wx/` | Desktop GUI source (wxWidgets) and platform makefiles |
| `gui-android/` | Android client source (Java/Native bridge) |
| `cmdline/` | `bgolly` and utility tools |
| `Patterns/` | Built-in pattern collection (RLE/MC) |
| `Rules/` | `.rule` files for custom CA definitions |
| `Scripts/` | Python and Lua automation scripts |
| `Help/` | Documentation and reference guides |
| `docs/` | Build instructions and license information |

## License
Copyright © 2005-2026 The Golly Gang.  
For full licensing details, see `docs/License.html`.

## Contributing
Interested in extending Golly? Refer to `docs/Build.html` for a detailed source code roadmap, including high-level GUI modules and low-level engine architecture. The community welcomes pattern additions, algorithm optimizations, and script contributions.
