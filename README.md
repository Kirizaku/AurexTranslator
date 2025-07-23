# AurexTranslator
Cross-platform screen text translator for Windows and Linux (X11/XWayland)

### Features

- Real-time screen text translation
- Windows and Linux support
- Simple intuitive interface

## Building
### Requirements
#### Common
- QT6
- OpenCV
- Tesseract OCR
#### Linux Specific
- libpipewire-0.3-dev
- libX11-dev

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
