# CRAP

CRAP is a custom video and audio compression codec container format and toolchain written in C11 for macOS.

---

## Features

- **custom vid Codec**: BT.601 RGB to YUV420P conversion 8x8 2D DCT zigzag run length entropy bitstream encoding.
- **custom binary container**: Chunked multi stream container format with CRC32 checksums timestamps stream descriptors & index tables for fast seeking.
- **audio support**: 16 bit PCM stereo mono audio interleaving with synchronized playback.
- **macOS interface & CLI tools**:
  - **`crappr`**: Native recorder app with live camera preview audio waveform metering and hotkey controls.
  - **`craprec`**: CL AVFoundation camera and microphone recorder.
  - **`crapplay`**: SDL2 based audio/video player with seeking, pause, and volume controls.
  - **`libcrap.a`**: Embeddable static library and C API for encoding and decoding.

---

## Quickstart (Using Releases)

Prebuilt binaries for macOS Apple Silicon (`arm64`) are available on the [Releases](https://github.com/jagath-sajjan/crap/releases) page.

### 1. Download & Extract

```bash
# download and extract the latest release
tar -xzvf crap-macos-arm64.tar.gz
```

### 2. Run the Tools

```bash
# launch screen & camera recorder
./crappr

# record 10 seconds of camera + microphone to output.crap from terminal
./craprec -d 10 -q 85 -o output.crap

# play a .crap video
./crapplay output.crap
```

> **note on SDL2**: `crapplay` requires SDL2. If you don't have it installed:
> ```bash
> brew install sdl2
> ```

---

## Tool Reference

### `crappr` (GUI Recorder)
A native macOS floating panel application built with Cocoa and AVFoundation:
- live camera preview.
- audio input.
- start / stop recording hotkeys.

### `craprec` (CLI Recorder)
```bash
Usage: craprec [options]
Options:
  -o <path>        Output file path (default: output.crap)
  -d <sec>         Recording duration in seconds (default: 5)
  -q <1-100>       Video quality (default: 85)
  -w <width>       Target capture width (default: 640)
  -h <height>      Target capture height (default: 480)
  --no-audio       Disable microphone capture (video only)
```

### `crapplay` (Player)
```bash
Usage: crapplay <file.crap>

Keyboard Controls:
  [Space]       Pause / Resume
  [Left/Right]  Seek backward / forward (5s)
  [Up/Down]     Volume up / down
  [Q / Esc]     Quit
```

---

## Architecture & Codebase Overview

```
crap/
├── include/           # public headers & data structures
│   ├── crap.h         # umbrella header
│   ├── container.h    # CRAP file format, headers, frame chunking, index
│   ├── encoder.h      # video/audio stream encoder
│   ├── decoder.h      # video/audio stream decoder
│   ├── dct.h          # 8x8 2D Forward and Inverse DCT + Zigzag tables
│   ├── quant.h        # Quantization matrix generator and scalar quantizer
│   ├── entropy.h      # Bitstream coder
│   ├── bitstream.h    # Bit reader and writer
│   ├── colorspace.h   # RGB24 / RGBA to YUV420P / YUV422P / YUV444P
│   ├── audio.h        # Audio framing and PCM chunk packaging
│   ├── frame.h        # Memory management for YUV / raw frames
│   ├── pool.h         # Zero allocation frame buffer pool
│   └── framequeue.h   # Thread safe producer consumer queue
├── src/               # Codec and container implementation files
├── encoder/           # Encoder pipeline implementation
├── decoder/           # Decoder pipeline implementation
├── capture/           # AVFoundation camera/mic session management
├── player/            # SDL2 audio/video playback engine
├── tools/             # Binary frontends (craprec, crappr)
└── tests/             # Unit and integration test suites
```

## Building from Source

### Prerequisites (macOS)
- Xcode Command Line Tools (`xcode-select --install`)
- SDL2 (for `crapplay`): `brew install sdl2`
- `pkg-config` (optional): `brew install pkg-config`

### Build All Binaries & Library

```bash
# Build libcrap.a, craprec, crapplay, and crappr
make

# Run the test suite
make test
```

### Clean
```bash
make clean
```

---

## License

MIT License. See [LICENSE](LICENSE) for details.