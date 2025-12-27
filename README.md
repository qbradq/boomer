# Boomer

Boomer is a Build-like, software rendered 2.5D game engine designed to deploy
games to the web and desktop.

## Build

### Requirements

Build Tools:

- CMake 3.14 or later
- Ninja build tools
- Emscripten 4.0.22 or later

System Libraries (Debian/Ubuntu):

```bash
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxi-dev \
libgl1-mesa-dev libglu1-mesa-dev libxcursor-dev libxinerama-dev libwayland-dev \
libxkbcommon-dev
```

All other requirements will be installed automatically by CMake:

- raylib 5.5
- raygui 4.0
- QuickJS
- miniz

### Build

```bash
cmake --preset=release
cmake --build --preset=release
```

### Run

```bash
./build/release/boomer path_to_game_pak_or_dir
```

### Development

```bash
cmake --preset=dev
cmake --build --preset=dev
./build/dev/boomer # Path to game defaults to the demo content
```

## Legal

Boomer's licensing structure is intended to be permissive, simple, and easy to
comply with for individuals and organizations. Any and all modifications to the
engine must be shared so that others can benefit from them. However, all example
assets provided with the engine are free to use without attribution for
commercial or non-commercial use, and users are not required to share the source
of any of their own assets or game scripts even if they are derived from the
example assets.

### Copyright

Boomer is Copyright (c) 2025 Norman B. Lancaster (qbradq)

### Authors

Norman B. Lancaster (qbradq) <qbradq@gmail.com>

### License

The Boomer engine and related tools are licensed under the GPL v3.0 or later.
See [LICENSE](LICENSE) for more information.

Game assets are covered under their own licenses, which should be included with
the assets in a file called `LICENSE`.

### Disclaimer of Warranty

The Boomer engine and related tools are provided "as is" without warranty of
any kind, either express or implied, including but not limited to the implied
warranties of merchantability and fitness for a particular purpose.

In no event shall the authors or copyright holders be liable for any claim,
damages or other liability, whether in an action of contract, tort or otherwise,
arising from, out of or in connection with the Boomer engine or the use or other
dealings in the Boomer engine.
