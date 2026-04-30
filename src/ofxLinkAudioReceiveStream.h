// ============================================================================
// ofxLinkAudioReceiveStream - subscribes to a Link Audio channel
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
#include "AudioRingBuffer.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

// Forward declarations
class LinkAudioManager;
namespace ableton {
    class LinkAudioSource;
}

// ----------------------------------------------------------------------------
// Settings struct passed at setup time
// ----------------------------------------------------------------------------

struct ofxLinkAudioReceiveSettings {
    std::string fromPeerName    = "";       // source peer's name (essential to disambiguate)
    std::string fromChannelName = "Main";   // channel name on that peer
    int         numChannels     = 2;        // 1 (mono) or 2 (stereo) - what we want at the output
    int         sampleRate      = 48000;
    int         bufferSize      = 256;
    bool        autoEnable      = true;
};

// ----------------------------------------------------------------------------
// Receive stream - mirrors ofSoundStream conventions
// ----------------------------------------------------------------------------

class ofxLinkAudioReceiveStream {
public:
    ofxLinkAudioReceiveStream();
    ~ofxLinkAudioReceiveStream();

    bool setup(const ofxLinkAudioReceiveSettings& settings);
    void close();

    // Plug your audioIn(ofSoundBuffer&) callback here.
    void setInput(ofBaseSoundInput& app);
    void setInput(ofBaseSoundInput* app);

    void start();
    void stop();
    bool isRunning() const;

    // Runtime configuration
    void setFromPeerName(const std::string& name);
    void setFromChannelName(const std::string& name);
    void setEnabled(bool enabled);

    std::string getFromPeerName()    const;
    std::string getFromChannelName() const;
    bool        getEnabled()         const;

    // Status query
    bool     isSubscribed()        const;
    int      getNumPeers()         const;
    uint64_t getFramesReceived()   const;
    uint64_t getFramesDropped()    const;
    uint32_t getStreamSampleRate() const;
    uint32_t getStreamNumChannels() const;

    // ---- Link session control / query ----
    //
    // These read and write the shared Link session state (tempo, transport,
    // beat, phase). All wrap captureAppSessionState() / commitAppSessionState()
    // and are thread-safe with respect to other Link API calls. Call from the
    // app thread / main oF loop / setup / update — NOT from inside
    // audioIn(). For sample-accurate audio-thread reads, capture an
    // AudioSessionState directly via your own LinkAudioManager handle.
    //
    // Setters propagate to all Link peers (Live, Max, TD, etc.). Reading
    // returns the current live session value, which reflects changes made
    // by any peer.

    double getTempo();
    void   setTempo(double bpm);

    bool   isTransportPlaying();
    void   setTransport(bool playing);

    double getBeat (double quantum = 4.0);
    double getPhase(double quantum = 4.0);

    const ofxLinkAudioReceiveSettings& getSettings() const { return settings; }

private:
    void audioThreadLoop();
    void workerThreadLoop();
    void applyState();
    void trySubscribe();
    void unsubscribeInternal();

    // Source callback - runs on Link's thread
    void onSourceBuffer(struct ableton_LinkAudioSource_BufferHandle_proxy*);

    mutable std::mutex stateMutex;
    std::string  fromPeerName_;
    std::string  fromChannelName_;
    bool         enabled_ = true;

    std::atomic<bool> stateDirty {true};

    ofxLinkAudioReceiveSettings settings;

    ofBaseSoundInput* input = nullptr;

    std::shared_ptr<LinkAudioManager>            manager;
    std::unique_ptr<ableton::LinkAudioSource>    source;
    std::string                                  subscribedFromPeer;
    std::string                                  subscribedFromChannel;
    bool                                         workerEnabled = false;

    // SPSC ring buffers - written by Link callback thread, read by audio thread
    std::unique_ptr<AudioRingBuffer> ringL;
    std::unique_ptr<AudioRingBuffer> ringR;

    std::thread             audioThread;
    std::atomic<bool>       audioThreadStop {false};
    std::atomic<bool>       audioThreadRunning {false};

    std::thread             workerThread;
    std::atomic<bool>       workerStop {false};
    std::condition_variable workerCv;
    std::mutex              workerCvMutex;

    ofSoundBuffer appBuffer;

    std::atomic<uint32_t> streamSampleRate  {48000};
    std::atomic<uint32_t> streamNumChannels {1};
    std::atomic<uint64_t> framesReceived    {0};
    std::atomic<uint64_t> framesDropped     {0};
    std::atomic<int>      peerCount         {0};
    std::atomic<bool>     isSubscribedFlag  {false};
};