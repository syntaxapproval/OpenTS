# WebAssembly build

An unsupported build of OpenTS that runs in a browser. It is the engine behind the
Tiberian Sun Demo in Windows 99 (win99.dev). The supported targets are still the Visual
Studio builds in `docs/BUILDING.md`.

## Build

Needs Emscripten 6.0.12 (emsdk), CMake and Ninja.

```
source <emsdk>/emsdk_env.sh
port/emscripten/build.sh
```

The output is in `build-wasm/bin`: `Game.js`, `Game.wasm` and the `ui` folder. The script
configures with the title-screen options the Windows 99 build uses (edition "Windows 99
Build", platform NT9X); set `OPENTS_CMAKE_ARGS` to replace them. The script's header
lists its arguments.

## Running it

A page loads `Game.js` as an Emscripten module and supplies the files before the game
starts:

- the `ui` folder at `/opents/ui` in the module's file system, beside the
  `/opents/GAME.EXE` the engine reports as its own path,
- the game's data in a folder named by `-DATADIR=`,
- a writable user folder named by `-USERDIR=` (settings, saved games, Skirmish maps).

Windows 99 starts it with `-DATADIR=/tsdemo -USERDIR=/tsuser -WIN`, keeps `/tsuser` in
IndexedDB, and copies the demo's files into `/tsdemo` from its own drive. The game data
is not part of OpenTS and is not in this repository.

`port/emscripten` supplies the Win32 the engine still uses; `winshim.cpp` describes how.
`probe.cpp` exports a few readings for test pages.
