// ============================================================================
// ofxLinkAudioSendStream - publishes audio to a Link Audio channel
//
// Part of the VoidLinkAudio R&D project by Julien Bayle / Structure Void.
// https://julienbayle.net    https://structure-void.com
//
// Released under the MIT License - see LICENSE file at repo root.
// Built on top of Ableton Link Audio (see ACKNOWLEDGEMENTS.md).
// Provided AS IS, without warranty of any kind.
// ============================================================================

#pragma once

#include "ofMain.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Forward declarations to avoid leaking Link headers into oF apps
class LinkAudioManager;
namespace ableton {
    class LinkAudioSink;
}

// ----------------------------------------------------------------------------
// Settings struct passed at setup time
// ----------------------------------------------------------------------------

struct ofxLinkAudioSendSettings {
    std::string channelName  = "oF Out";    // advertised channel name
    std::string peerName     = "oF App";    // this peer's identity
    int         numChannels  = 2;           // 1 (mono) or 2 (stereo)
    int         sampleRate   = 48000;
    int         bufferSize   = 256;         // frames per audioOut() callback
    bool        autoEnable   = true;        // start publishing immediately
};

// ----------------------------------------------------------------------------
// Send stream - mirrors ofSoundStream conventions
// ----------------------------------------------------------------------------

class ofxLinkAudioSendStream {
public:
    ofxLinkAudioSendStream();
    ~ofxLinkAudioSendStream();

    // Setup creates the audio thread, peer, and channel publication.
    // Returns true on success.
    bool setup(const ofxLinkAudioSendSettings& settings);

    // Tear down audio thread and Link resources.
    void close();

    // Plug your audioOut(ofSoundBuffer&) callback here.
    // Equivalent to ofSoundStream::setOutput.
    void setOutput(ofBaseSoundOutput& app);
    void setOutput(ofBaseSoundOutput* app);

    // Start / stop the audio thread.
    void start();
    void stop();
    bool isRunning() const;

    // ---- Runtime configuration ----
    // These can be called at any time from the app thread.
    void setChannelName(const std::string& name);
    void setPeerName(const std::string& name);
    void setEnabled(bool enabled);

    std::string getChannelName() const;
    std::string getPeerName()    const;
    bool        getEnabled()     const;

    // ---- Status query ----
    bool        isPublishing()       const;
    int         getNumPeers()        const;
    uint64_t    getFramesPublished() const;
    uint64_t    getFramesDropped()   const;
    uint64_t    getFramesNoBuffer()  const;

    // ---- Settings accessor ----
    const ofxLinkAudioSendSettings& getSettings() const { return settings; }

private:
    void audioThreadLoop();
    void workerThreadLoop();
    void applyState();
    void publishStaging();

    // ---- Configuration (UI <-> worker) ----
    mutable std::mutex stateMutex;
    std::string  channelName_;
    std::string  peerName_;
    bool         enabled_ = true;

    std::atomic<bool> stateDirty {true};

    // ---- Settings (read-only after setup) ----
    ofxLinkAudioSendSettings settings;

    // ---- App callback ----
    ofBaseSoundOutput* output = nullptr;

    // ---- Link Audio plumbing ----
    std::shared_ptr<LinkAudioManager>       manager;
    std::unique_ptr<ableton::LinkAudioSink> sink;
    std::string                             publishedChannel;
    std::string                             currentPeerName;
    bool                                    workerEnabled = false;

    // ---- Audio thread ----
    std::thread             audioThread;
    std::atomic<bool>       audioThreadStop {false};
    std::atomic<bool>       audioThreadRunning {false};

    // ---- Worker thread ----
    std::thread             workerThread;
    std::atomic<bool>       workerStop {false};
    std::condition_variable workerCv;
    std::mutex              workerCvMutex;

    // ---- Buffers ----
    ofSoundBuffer        appBuffer;     // float buffer passed to user callback
    std::vector<int16_t> stagingBuffer; // int16 interleaved, sent to Link
    std::size_t          stagingFrames = 0;

    // ---- Stats ----
    std::atomic<uint64_t> framesPublished {0};
    std::atomic<uint64_t> framesDropped   {0};
    std::atomic<uint64_t> framesNoBuffer  {0};
    std::atomic<int>      peerCount       {0};
    std::atomic<bool>     isPublishingFlag {false};
};
