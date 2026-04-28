// ============================================================================
// ofxLinkAudioSendStream - implementation
//
// Part of the VoidLinkAudio R&D project by Julien Bayle / Structure Void.
// https://julienbayle.net    https://structure-void.com
//
// Released under the MIT License - see LICENSE file at repo root.
// Built on top of Ableton Link Audio (see ACKNOWLEDGEMENTS.md).
// Provided AS IS, without warranty of any kind.
// ============================================================================

#include "ofxLinkAudioSendStream.h"

#include "LinkAudioManager.h"
#include <ableton/LinkAudio.hpp>

#include <cstring>

namespace {

constexpr std::size_t kInitialMaxSamples = 32768;

inline int16_t floatToInt16Clamped(float v) {
    if (v >=  1.0f) return  32767;
    if (v <= -1.0f) return -32768;
    return static_cast<int16_t>(v * 32768.0f);
}

} // namespace

// ----------------------------------------------------------------------------

ofxLinkAudioSendStream::ofxLinkAudioSendStream() = default;

ofxLinkAudioSendStream::~ofxLinkAudioSendStream() {
    close();
}

// ----------------------------------------------------------------------------

bool ofxLinkAudioSendStream::setup(const ofxLinkAudioSendSettings& s) {
    if (audioThreadRunning.load()) {
        ofLogWarning("ofxLinkAudioSendStream") << "setup called while running, ignoring";
        return false;
    }

    if (s.numChannels != 1 && s.numChannels != 2) {
        ofLogError("ofxLinkAudioSendStream") << "numChannels must be 1 or 2, got " << s.numChannels;
        return false;
    }
    if (s.sampleRate <= 0) {
        ofLogError("ofxLinkAudioSendStream") << "invalid sampleRate " << s.sampleRate;
        return false;
    }
    if (s.bufferSize <= 0) {
        ofLogError("ofxLinkAudioSendStream") << "invalid bufferSize " << s.bufferSize;
        return false;
    }

    settings    = s;
    channelName_ = s.channelName;
    peerName_   = s.peerName;
    enabled_    = s.autoEnable;

    appBuffer.setSampleRate(s.sampleRate);
    appBuffer.setNumChannels(s.numChannels);
    appBuffer.resize(static_cast<std::size_t>(s.bufferSize) * s.numChannels);

    stagingBuffer.assign(static_cast<std::size_t>(s.bufferSize) * s.numChannels, 0);
    stagingFrames = 0;

    manager = LinkAudioManager::acquire();

    workerStop.store(false);
    workerThread = std::thread([this] { workerThreadLoop(); });

    return true;
}

// ----------------------------------------------------------------------------

void ofxLinkAudioSendStream::close() {
    stop();

    workerStop.store(true);
    workerCv.notify_all();
    if (workerThread.joinable()) workerThread.join();

    sink.reset();
    manager.reset();
}

// ----------------------------------------------------------------------------

void ofxLinkAudioSendStream::setOutput(ofBaseSoundOutput& app) {
    output = &app;
}

void ofxLinkAudioSendStream::setOutput(ofBaseSoundOutput* app) {
    output = app;
}

// ----------------------------------------------------------------------------

void ofxLinkAudioSendStream::start() {
    if (audioThreadRunning.load()) return;

    audioThreadStop.store(false);
    audioThread = std::thread([this] { audioThreadLoop(); });
    audioThreadRunning.store(true);
}

void ofxLinkAudioSendStream::stop() {
    if (!audioThreadRunning.load()) return;

    audioThreadStop.store(true);
    if (audioThread.joinable()) audioThread.join();
    audioThreadRunning.store(false);
}

bool ofxLinkAudioSendStream::isRunning() const {
    return audioThreadRunning.load();
}

// ----------------------------------------------------------------------------

void ofxLinkAudioSendStream::setChannelName(const std::string& name) {
    {
        std::lock_guard<std::mutex> lk(stateMutex);
        channelName_ = name;
    }
    stateDirty.store(true);
    workerCv.notify_all();
}

void ofxLinkAudioSendStream::setPeerName(const std::string& name) {
    {
        std::lock_guard<std::mutex> lk(stateMutex);
        peerName_ = name;
    }
    stateDirty.store(true);
    workerCv.notify_all();
}

void ofxLinkAudioSendStream::setEnabled(bool enabled) {
    {
        std::lock_guard<std::mutex> lk(stateMutex);
        enabled_ = enabled;
    }
    stateDirty.store(true);
    workerCv.notify_all();
}

std::string ofxLinkAudioSendStream::getChannelName() const {
    std::lock_guard<std::mutex> lk(stateMutex);
    return channelName_;
}

std::string ofxLinkAudioSendStream::getPeerName() const {
    std::lock_guard<std::mutex> lk(stateMutex);
    return peerName_;
}

bool ofxLinkAudioSendStream::getEnabled() const {
    std::lock_guard<std::mutex> lk(stateMutex);
    return enabled_;
}

// ----------------------------------------------------------------------------

bool     ofxLinkAudioSendStream::isPublishing()       const { return isPublishingFlag.load(); }
int      ofxLinkAudioSendStream::getNumPeers()        const { return peerCount.load(); }
uint64_t ofxLinkAudioSendStream::getFramesPublished() const { return framesPublished.load(); }
uint64_t ofxLinkAudioSendStream::getFramesDropped()   const { return framesDropped.load(); }
uint64_t ofxLinkAudioSendStream::getFramesNoBuffer()  const { return framesNoBuffer.load(); }

// ----------------------------------------------------------------------------
// Worker thread - owns Link API calls and sink lifecycle
// ----------------------------------------------------------------------------

void ofxLinkAudioSendStream::applyState() {
    if (!manager) return;
    auto& la = manager->linkAudio();

    std::string wantChannel, wantPeer;
    bool wantEnabled;
    {
        std::lock_guard<std::mutex> lk(stateMutex);
        wantChannel = channelName_;
        wantPeer    = peerName_;
        wantEnabled = enabled_;
    }

    if (!wantPeer.empty() && wantPeer != currentPeerName) {
        manager->setPeerName(wantPeer);
        currentPeerName = wantPeer;
    }

    if (wantEnabled != workerEnabled) {
        la.enable(wantEnabled);
        la.enableLinkAudio(wantEnabled);
        workerEnabled = wantEnabled;
        if (!wantEnabled) {
            sink.reset();
            publishedChannel.clear();
        }
    }

    if (!workerEnabled) {
        isPublishingFlag.store(false);
        peerCount.store(static_cast<int>(la.numPeers()));
        return;
    }

    if (wantChannel != publishedChannel) {
        sink.reset();
        publishedChannel = wantChannel;
    }

    if (!sink && !publishedChannel.empty()) {
        sink.reset(new ableton::LinkAudioSink(la, publishedChannel, kInitialMaxSamples));
        framesPublished.store(0);
        framesDropped.store(0);
        framesNoBuffer.store(0);
    }

    isPublishingFlag.store(sink != nullptr);
    peerCount.store(static_cast<int>(la.numPeers()));
}

void ofxLinkAudioSendStream::workerThreadLoop() {
    using namespace std::chrono_literals;
    while (!workerStop.load()) {
        applyState();
        stateDirty.store(false);

        std::unique_lock<std::mutex> lk(workerCvMutex);
        workerCv.wait_for(lk, 200ms, [this] {
            return workerStop.load() || stateDirty.load();
        });
    }
}

// ----------------------------------------------------------------------------
// Audio thread - calls user audioOut(), stages, publishes
// ----------------------------------------------------------------------------

void ofxLinkAudioSendStream::publishStaging() {
    if (!sink || stagingFrames == 0) return;

    auto& la = manager->linkAudio();

    ableton::LinkAudioSink::BufferHandle bh(*sink);
    if (!bh) {
        // No consumer ready (no receiver subscribed, or buffer not yet
        // allocated). Not a real drop - count separately for diagnostics.
        framesNoBuffer.fetch_add(stagingFrames);
        stagingFrames = 0;
        return;
    }

    const std::size_t numFrames    = stagingFrames;
    const std::size_t totalSamples = numFrames * static_cast<std::size_t>(settings.numChannels);

    if (totalSamples > bh.maxNumSamples) {
        // Real drop: we asked to send more than the sink can hold.
        framesDropped.fetch_add(numFrames);
        stagingFrames = 0;
        return;
    }

    std::memcpy(bh.samples, stagingBuffer.data(), sizeof(int16_t) * totalSamples);

    const auto   state              = la.captureAppSessionState();
    const auto   now                = la.clock().micros();
    const double quantum            = 4.0;
    const double beatsAtBufferBegin = state.beatAtTime(now, quantum);

    const bool ok = bh.commit(state,
                              beatsAtBufferBegin,
                              quantum,
                              numFrames,
                              static_cast<unsigned>(settings.numChannels),
                              static_cast<double>(settings.sampleRate));

    if (ok) framesPublished.fetch_add(numFrames);
    else    framesDropped.fetch_add(numFrames);

    stagingFrames = 0;
}

void ofxLinkAudioSendStream::audioThreadLoop() {
    using namespace std::chrono;

    const auto framePeriod = nanoseconds(
        static_cast<long long>(1e9 * settings.bufferSize / settings.sampleRate));

    auto next = steady_clock::now();

    while (!audioThreadStop.load()) {
        // Pull audio from user app
        if (output) {
            // Reset to silence in case user doesn't fill it
            std::fill(appBuffer.getBuffer().begin(), appBuffer.getBuffer().end(), 0.0f);
            output->audioOut(appBuffer);
        } else {
            std::fill(appBuffer.getBuffer().begin(), appBuffer.getBuffer().end(), 0.0f);
        }

        // Convert float to int16 interleaved
        const std::size_t totalSamples =
            static_cast<std::size_t>(settings.bufferSize) * settings.numChannels;
        const float* src = appBuffer.getBuffer().data();
        for (std::size_t i = 0; i < totalSamples; ++i) {
            stagingBuffer[i] = floatToInt16Clamped(src[i]);
        }
        stagingFrames = settings.bufferSize;

        // Publish
        publishStaging();

        // Wait until next frame slot
        next += framePeriod;
        const auto now = steady_clock::now();
        if (next > now) {
            std::this_thread::sleep_until(next);
        } else {
            // We are behind; reset to avoid runaway accumulation
            next = now;
        }
    }
}
