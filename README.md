# AurexTranslator

Cross-platform real-time translator for Windows and Linux (X11/XWayland) — translates text captured from the screen (OCR), from the clipboard, or hooked directly out of a game or engine via [plugins](https://github.com/Kirizaku/AurexTranslator-plugins).

<table>
<tr>
<td width="50%">
<img src="https://github.com/user-attachments/assets/1d6656cf-393b-4e40-bba7-a26f2ea1d0af" width="100%" alt="AurexTranslator OCR translation overlay over Clannad, with the Output settings dialog open">
</td>
<td width="50%">
<img src="https://github.com/user-attachments/assets/3df7e5f3-288f-4a56-a69f-5305eca3bebe" width="100%" alt="AurexTranslator hook text selection dialog and a Hook:Textbox translation overlay over Doki Doki Literature Club">
</td>
</tr>
</table>

## Features

- **Multiple input sources**
  - **OCR**: screen-region capture via Tesseract or Ollama Vision (auto or manual mode)
  - **Clipboard**: translate copied text automatically (native Wayland, XDG Portal, or Qt clipboard backend depending on your desktop)
  - **Game Text Extraction**: intercepts text output at runtime by injecting a plugin into the game's process and hooking its text output functions. Requires installing the injector (`libat-injector` + `at-injector`) and a game/engine-specific plugin — see [AurexTranslator-plugins](https://github.com/Kirizaku/AurexTranslator-plugins) for supported engines, games, and installation instructions.
- **Named config profiles** — keep separate settings per game, switch between them without re-injecting
- **Text replacement rules** — regex-capable, scoped per source or per game
- **Configurable text output** — whole-block or per-character reveal with tunable speed, multi-block selection with priority ordering (for games/engines with multiple simultaneous text sources)
- **Cross-platform**: Windows and Linux (X11/XWayland)
- **Multi-arch plugin loading** — automatically selects the correct x86/x64 plugin build

## Roadmap

| Feature | Description |
|---|---|
| 🖼️ Multi-zone OCR | Capture and translate several screen regions at once instead of a single zone |
| 📦 Native Linux packages | `.deb` (Ubuntu/Debian), AUR (Arch), `.rpm` (Fedora/openSUSE), Flatpak & Snap (universal) |
| 🎮 In-game overlay | On-screen translation without a separate window, via graphics API hooking (DLL injection on Windows, `LD_PRELOAD` on Linux) |
| 🔊 TTS (Text-to-Speech) | Voice output |
| 🌐 More translation engines | Additional providers beyond the currently supported ones |
| 🧩 More game/engine plugins | See [AurexTranslator-plugins](https://github.com/Kirizaku/AurexTranslator-plugins) |

Have a feature request or want to help with one of these? Open an [issue](https://github.com/Kirizaku/AurexTranslator/issues) or a [PR](https://github.com/Kirizaku/AurexTranslator/pulls).

## Building

### Requirements

#### Common
- CMake 3.10+
- Qt6 (Core, Network, Widgets, Svg, LinguistTools)
- OpenCV (core, imgproc, imgcodecs)
- Tesseract OCR

#### Linux Specific
- Qt6 DBus
- libpipewire-0.3-dev
- libX11-dev, libXcomposite-dev, libXfixes-dev, libXrandr-dev
- libwayland-dev (**optional** — enables the native Wayland clipboard backend; without it the app still builds and falls back to the XDG Portal/Qt clipboard backends)

### Compilation Instructions

Clone the repository:
```bash
git clone https://github.com/Kirizaku/AurexTranslator.git
cd AurexTranslator
```

Create build directory:
```bash
mkdir build && cd build
```
Configure with CMake:
```bash
cmake ..
```
Build the project:
```bash
cmake --build .
```

## License

AurexTranslator code is licensed under the [GPL-3 License](https://www.gnu.org/licenses/gpl-3.0.html).
Please see [the licence file](https://github.com/Kirizaku/AurexTranslator/blob/main/LICENSE) for more information.
