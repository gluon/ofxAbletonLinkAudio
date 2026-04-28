# ofxAbletonLinkAudio

Ableton Link Audio receiver and publisher for openFrameworks.

Stream audio between your openFrameworks apps and Ableton Live, TouchDesigner, Max/MSP, VCV Rack, or any other Link Audio host on the local network — sample-accurate, beat-synced, stereo, lock-free.

## Status

**Functional** on macOS (Apple Silicon and Intel). Bidirectional audio validated against Ableton Live 12.4 (beta and release builds), TouchDesigner, Max/MSP, and VCV Rack on the same LAN. Linux and Windows configurations are present in `addon_config.mk` and expected to work; testing on those platforms is in progress.

This addon is part of **VoidLinkAudio**, an open R&D project that brings the same Link Audio interop to multiple hosts (oF, TD, Max, VCV, VST3/AU). See the parent repository for the other targets.

## What is Link Audio

Link Audio is an extension of Ableton Link that lets multiple applications share **audio streams** in addition to the shared tempo and beat clock that Link already provides. Streams are advertised by name on the local network and can be picked up by any subscriber.

Link Audio is part of Ableton Link's open-source SDK and is exposed in **Ableton Live starting with version 12.4** (public beta from February 2026, public release on May 5, 2026). Host-to-host audio between any other Link Audio app on the network does not require Live.

In Link Audio's model:

- a **peer** is one application participating on the network, identified by a peer name (e.g. `oF App`, `Live`, `VCVRack`)
- a **channel** is one named stream that a peer publishes (e.g. `oF Out`, `Main`)

A subscriber addresses a channel by the pair (peer name, channel name).

## Objects

This addon exposes two stream classes that mirror oF's `ofSoundStream` conventions:

- **`ofxLinkAudioSendStream`** publishes audio to a named Link Audio channel. The app provides an `audioOut(ofSoundBuffer&)` callback (via `setOutput(*this)`); the stream pulls samples from it on its own audio thread and pushes them onto the network.
- **`ofxLinkAudioReceiveStream`** subscribes to a remote channel by `(fromPeer, fromChannel)` and pushes the incoming stream into the app's `audioIn(ofSoundBuffer&)` callback (via `setInput(*this)`).

Both streams own a worker thread that handles all "heavy" Link API calls (peer setup, channel discovery, subscribe/unsubscribe), and an audio thread that runs the lock-free DSP path. Auto-subscribe is on by default, so a Receive stream transparently reconnects when a matching channel reappears on the network.

## Installation

Clone or download this addon into your openFrameworks `addons/` directory:

```bash
cd of_v0.12.x_<platform>_release/addons/
git clone --recursive https://github.com/gluon/ofxAbletonLinkAudio.git ofxAbletonLinkAudio
```

The `--recursive` flag is needed because Ableton Link is included as a git submodule (with its own asio-standalone sub-submodule). If you forgot it, run `git submodule update --init --recursive` inside the addon folder.

The shared core (`LinkAudioManager`, `AudioRingBuffer`) is vendored under `libs/core/`. The Ableton Link SDK (with its embedded ASIO standalone networking module) is fetched via the submodule under `libs/link/`. Nothing else to install.

Use the openFrameworks Project Generator (or `projectGenerator -a ofxAbletonLinkAudio`) to add the addon to your project. The included `addon_config.mk` handles all platform-specific compile flags, defines, and linker flags (CoreFoundation on macOS, pthread on Linux, ws2_32 / iphlpapi / winmm on Windows).

## Quick start — publishing

```cpp
#include "ofMain.h"
#include "ofxAbletonLinkAudio.h"

class ofApp : public ofBaseApp {
    ofxLinkAudioSendStream sendStream;

    void setup() override {
        ofxLinkAudioSendSettings s;
        s.channelName = "oF Out";
        s.peerName    = "oF App";
        s.numChannels = 2;
        s.sampleRate  = 48000;
        s.bufferSize  = 256;

        sendStream.setup(s);
        sendStream.setOutput(*this);
        sendStream.start();
    }

    void exit() override { sendStream.close(); }

    void audioOut(ofSoundBuffer& buffer) override {
        // Generate or pass through audio here.
        for (auto& sample : buffer) sample = ofRandomf() * 0.1f;
    }
};
```

In Ableton Live: enable Link, set a track's `Audio From` to `oF App / oF Out`, turn on monitoring. You should hear the noise.

## Quick start — receiving

```cpp
#include "ofMain.h"
#include "ofxAbletonLinkAudio.h"

class ofApp : public ofBaseApp {
    ofxLinkAudioReceiveStream rxStream;

    void setup() override {
        ofxLinkAudioReceiveSettings s;
        s.fromPeerName    = "Live";
        s.fromChannelName = "Main";
        s.numChannels     = 2;
        s.sampleRate      = 48000;
        s.bufferSize      = 256;

        rxStream.setup(s);
        rxStream.setInput(*this);
        rxStream.start();
    }

    void exit() override { rxStream.close(); }

    void audioIn(ofSoundBuffer& buffer) override {
        // The buffer has already been filled with received audio.
        // Read it, draw it, process it.
    }
};
```

`fromPeerName` is the **source** peer's name, not the local one. Leave it empty to subscribe to the first channel matching `fromChannelName` on any peer, but be aware that this is ambiguous when several peers publish a channel with the same name.

## Examples

Three examples are included:

- **`example-send`** publishes a stereo test signal (sine, saw, square, triangle waveforms, adjustable frequencies) on a named channel. Useful as a known-good Link Audio source for testing other hosts.
- **`example-receive`** subscribes to a channel and visualizes the incoming audio on stereo oscilloscopes plus peak meters. The peer name and channel name are editable at runtime via the keyboard (`p` to edit From Peer, `c` to edit From Channel, `enter` to commit, `esc` to cancel).
- **`example-pingpong`** combines a Receive and a Send in the same application. Audio is pulled from one channel, run through a stereo ping-pong delay, and republished on another channel. Demonstrates that Send and Receive can cohabit in one app.

Build any example with the standard openFrameworks workflow:

```bash
cd addons/ofxAbletonLinkAudio/example-send
make
make RunRelease
```

## API summary

```cpp
struct ofxLinkAudioSendSettings {
    std::string channelName;    // the published stream's name
    std::string peerName;       // this peer's identity on the network
    int         numChannels;    // 1 (mono) or 2 (stereo)
    int         sampleRate;
    int         bufferSize;     // frames per audioOut() callback
    bool        autoEnable;     // true to start publishing immediately
};

class ofxLinkAudioSendStream {
    bool   setup(const ofxLinkAudioSendSettings&);
    void   close();
    void   setOutput(ofBaseSoundOutput& app);
    void   start();
    void   stop();

    void        setChannelName(const std::string&);
    void        setPeerName(const std::string&);
    void        setEnabled(bool);
    std::string getChannelName() const;
    std::string getPeerName()    const;
    bool        getEnabled()     const;

    bool        isPublishing()       const;
    int         getNumPeers()        const;
    uint64_t    getFramesPublished() const;
    uint64_t    getFramesDropped()   const;
};

struct ofxLinkAudioReceiveSettings {
    std::string fromPeerName;       // source peer's name (empty = first match)
    std::string fromChannelName;    // channel name on that peer
    int         numChannels;
    int         sampleRate;
    int         bufferSize;
    bool        autoEnable;
};

class ofxLinkAudioReceiveStream {
    bool   setup(const ofxLinkAudioReceiveSettings&);
    void   close();
    void   setInput(ofBaseSoundInput& app);
    void   start();
    void   stop();

    void        setFromPeerName(const std::string&);
    void        setFromChannelName(const std::string&);
    void        setEnabled(bool);
    std::string getFromPeerName()    const;
    std::string getFromChannelName() const;
    bool        getEnabled()         const;

    bool        isSubscribed()        const;
    int         getNumPeers()         const;
    uint64_t    getFramesReceived()   const;
    uint64_t    getFramesDropped()    const;
    uint32_t    getStreamSampleRate() const;
    uint32_t    getStreamNumChannels()const;
};
```

## Compatibility

- openFrameworks 0.12.x
- macOS 11.0 or later (Apple Silicon and Intel)
- Linux x64 / ARM (configured in `addon_config.mk`, testing in progress)
- Windows 10+ via MSYS2/MinGW or Visual Studio (configured, testing in progress)
- **Ableton Live 12.4** (public release May 5, 2026) or later for cross-host interop with Live
- Any other Link Audio host

The addon needs C++17 (used internally by `core/` and the Link SDK).

## Layout

```
ofxAbletonLinkAudio/
├── addon_config.mk              oF build config (includes, defines, ldflags)
├── LICENSE                      MIT
├── ACKNOWLEDGEMENTS.md          Ableton Link credit and other attributions
├── README.md                    This file
├── src/
│   ├── ofxAbletonLinkAudio.h    Umbrella include
│   ├── ofxLinkAudioSendStream.h
│   ├── ofxLinkAudioSendStream.cpp
│   ├── ofxLinkAudioReceiveStream.h
│   └── ofxLinkAudioReceiveStream.cpp
├── libs/
│   ├── core/                    Shared C++ core (LinkAudioManager, AudioRingBuffer)
│   └── link/                    Ableton Link SDK (git submodule, with ASIO standalone)
├── example-send/
├── example-receive/
└── example-pingpong/
```

## Author

Julien Bayle — Structure Void
[https://structure-void.com](https://structure-void.com) — [https://julienbayle.net](https://julienbayle.net)
Ableton Certified Trainer · Max Certified Trainer.

## Acknowledgements

Built on top of [Ableton Link](https://github.com/Ableton/link), the open-source synchronization library by **Ableton AG** (MIT license). Link Audio is the audio extension shipped with Ableton Link, exposed in Ableton Live starting with version 12.4 (public release May 5, 2026).

This implementation is independent and is **not endorsed, certified, or supported by Ableton**. "Ableton", "Live", and "Link" are trademarks of Ableton AG.

See [`ACKNOWLEDGEMENTS.md`](ACKNOWLEDGEMENTS.md) for the complete attribution list and the verbatim Ableton Link license notice.

## License

ofxAbletonLinkAudio is released under the MIT License — see [`LICENSE`](LICENSE).

This is **R&D code**. It builds, runs, and has been tested in real bidirectional sessions across multiple hosts, but it is not a commercial product and there is no warranty of any kind, express or implied. See the LICENSE for the formal disclaimer.

If you ship something built with this addon, a credit (and a postcard) is appreciated but not required.