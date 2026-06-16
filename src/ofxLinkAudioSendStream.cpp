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

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr std::size_t kInitialMaxSamples = 32768;

// A.3 first-order DLL loop bandwidth (gain). Low gain: a jitter spike on the
// observed clock only nudges the smoothed timestamp by kDllBandwidth of it,
// while the residual error still converges to zero so there is no long-term
// drift versus the real Link clock. Range tried: 0.01 .. 0.1.
constexpr double kDllBandwidth = 0.05;

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

    // A.3: nominal microseconds per frame, used to advance the smoothed
    // publish timestamp by a fixed sample-derived step each buffer.
    microsPerFrame_ = 1.0e6 / static_cast<double>(s.sampleRate);
    dllInit_        = false;

    appBuffer.setSampleRate(s.sampleRate);
    appBuffer.setNumChannels(s.numChannels);
    appBuffer.resize(static_cast<std::size_t>(s.bufferSize) * s.numChannels);

    stagingBuffer.assign(static_cast<std::size_t>(s.bufferSize) * s.numChannels, 0);
    stagingFrames = 0;

    manager = LinkAudioManager::acquire("oF App");

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

    // Audio and worker threads are both joined by now, so no concurrent access
    // to sink remains; the lock here is just for consistency / future-proofing.
    {
        std::lock_guard<std::mutex> lk(sinkMutex);
        sink.reset();
    }
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

    // A.3: re-anchor the smoothed timestamp at the next publish, so a stop/start
    // cycle does not carry a stale hostTimeIdeal_.
    dllInit_ = false;

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
// Link session control / query
// ----------------------------------------------------------------------------

double ofxLinkAudioSendStream::getTempo() {
    if (!manager) return 0.0;
    return manager->tempo();
}

void ofxLinkAudioSendStream::setTempo(double bpm) {
    if (!manager) return;
    manager->setTempo(bpm);
}

bool ofxLinkAudioSendStream::isTransportPlaying() {
    if (!manager) return false;
    return manager->isPlaying();
}

void ofxLinkAudioSendStream::setTransport(bool playing) {
    if (!manager) return;
    manager->setIsPlaying(playing);
}

double ofxLinkAudioSendStream::getBeat(double quantum) {
    if (!manager) return 0.0;
    auto& la = manager->linkAudio();
    const auto state = la.captureAppSessionState();
    return state.beatAtTime(la.clock().micros(), quantum);
}

double ofxLinkAudioSendStream::getPhase(double quantum) {
    if (!manager) return 0.0;
    auto& la = manager->linkAudio();
    const auto state = la.captureAppSessionState();
    return state.phaseAtTime(la.clock().micros(), quantum);
}

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
            // sink is touched by the audio thread in publishStaging(); guard it.
            {
                std::lock_guard<std::mutex> lk(sinkMutex);
                sink.reset();
            }
            publishedChannel.clear();
        }
    }

    if (!workerEnabled) {
        isPublishingFlag.store(false);
        peerCount.store(static_cast<int>(la.numPeers()));
        return;
    }

    if (wantChannel != publishedChannel) {
        // Channel switch tears down the current sink (used by the audio
        // thread). Guard the reset so publishStaging() never derefs a dangling
        // *sink.
        {
            std::lock_guard<std::mutex> lk(sinkMutex);
            sink.reset();
        }
        publishedChannel = wantChannel;
    }

    if (!sink && !publishedChannel.empty()) {
        // Creation must be visible to the audio thread atomically with respect
        // to its use of *sink. Hold the lock across construction (this is the
        // non-bounded section assumed by the memory-safety contract: see .h).
        std::lock_guard<std::mutex> lk(sinkMutex);
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
    // Hold sinkMutex for the whole body: it both guards the !sink test and
    // keeps *sink / the BufferHandle alive against a concurrent sink.reset()
    // on the worker thread. The worker may be blocked on this lock while we
    // commit; that is the accepted trade-off (memory safety over RT latency)
    // for a timer-driven transport.
    std::lock_guard<std::mutex> lk(sinkMutex);

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

    const auto   state   = la.captureAppSessionState();
    const double quantum = 4.0;

    // A.3: derive the publish timestamp from the sample count, slaved to the
    // real clock by a first-order loop, instead of reading clock().micros()
    // raw (which carries the sleep_until jitter straight into the beat grid).
    const double observed = static_cast<double>(la.clock().micros().count());
    if (!dllInit_) {
        hostTimeIdeal_ = observed;   // phase anchor, once per start()
        dllInit_       = true;
    } else {
        const double error = observed - hostTimeIdeal_;
        // Net step clamped to >= 0: the timestamp may slow down but must never
        // go backwards. On an OS preemption spike, error can be strongly
        // negative and kDllBandwidth*error could exceed the nominal advance;
        // without the clamp hostTimeIdeal_ would regress and beatAtTime() would
        // be non-monotonic (a backwards beat jump for the peers).
        const double step = std::max(
            microsPerFrame_ * static_cast<double>(numFrames) + kDllBandwidth * error,
            0.0);
        hostTimeIdeal_ += step;
    }
    const auto hostTime = std::chrono::microseconds(
        static_cast<std::chrono::microseconds::rep>(std::llround(hostTimeIdeal_)));

    const double beatsAtBufferBegin = state.beatAtTime(hostTime, quantum);

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
