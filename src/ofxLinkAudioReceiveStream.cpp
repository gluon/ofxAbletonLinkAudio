// ============================================================================
// ofxLinkAudioReceiveStream - implementation
//
// Part of the VoidLinkAudio R&D project by Julien Bayle / Structure Void.
// https://julienbayle.net    https://structure-void.com
//
// Released under the MIT License - see LICENSE file at repo root.
// Built on top of Ableton Link Audio (see ACKNOWLEDGEMENTS.md).
// Provided AS IS, without warranty of any kind.
// ============================================================================

#include "ofxLinkAudioReceiveStream.h"

#include "LinkAudioManager.h"
#include <ableton/LinkAudio.hpp>

#include <optional>

namespace {
constexpr std::size_t kRingSize = 16384;
}

// ----------------------------------------------------------------------------

ofxLinkAudioReceiveStream::ofxLinkAudioReceiveStream() = default;

ofxLinkAudioReceiveStream::~ofxLinkAudioReceiveStream() {
    close();
}

// ----------------------------------------------------------------------------

bool ofxLinkAudioReceiveStream::setup(const ofxLinkAudioReceiveSettings& s) {
    if (audioThreadRunning.load()) {
        ofLogWarning("ofxLinkAudioReceiveStream") << "setup called while running, ignoring";
        return false;
    }
    if (s.numChannels != 1 && s.numChannels != 2) {
        ofLogError("ofxLinkAudioReceiveStream") << "numChannels must be 1 or 2";
        return false;
    }

    settings           = s;
    fromPeerName_      = s.fromPeerName;
    fromChannelName_   = s.fromChannelName;
    enabled_           = s.autoEnable;

    appBuffer.setSampleRate(s.sampleRate);
    appBuffer.setNumChannels(s.numChannels);
    appBuffer.resize(static_cast<std::size_t>(s.bufferSize) * s.numChannels);

    ringL.reset(new AudioRingBuffer(kRingSize));
    ringR.reset(new AudioRingBuffer(kRingSize));

    manager = LinkAudioManager::acquire();

    workerStop.store(false);
    workerThread = std::thread([this] { workerThreadLoop(); });

    return true;
}

// ----------------------------------------------------------------------------

void ofxLinkAudioReceiveStream::close() {
    stop();

    workerStop.store(true);
    workerCv.notify_all();
    if (workerThread.joinable()) workerThread.join();

    source.reset();
    manager.reset();
    ringL.reset();
    ringR.reset();
}

// ----------------------------------------------------------------------------

void ofxLinkAudioReceiveStream::setInput(ofBaseSoundInput& app) { input = &app; }
void ofxLinkAudioReceiveStream::setInput(ofBaseSoundInput* app) { input =  app; }

// ----------------------------------------------------------------------------

void ofxLinkAudioReceiveStream::start() {
    if (audioThreadRunning.load()) return;
    audioThreadStop.store(false);
    audioThread = std::thread([this] { audioThreadLoop(); });
    audioThreadRunning.store(true);
}

void ofxLinkAudioReceiveStream::stop() {
    if (!audioThreadRunning.load()) return;
    audioThreadStop.store(true);
    if (audioThread.joinable()) audioThread.join();
    audioThreadRunning.store(false);
}

bool ofxLinkAudioReceiveStream::isRunning() const {
    return audioThreadRunning.load();
}

// ----------------------------------------------------------------------------

void ofxLinkAudioReceiveStream::setFromPeerName(const std::string& name) {
    { std::lock_guard<std::mutex> lk(stateMutex); fromPeerName_ = name; }
    stateDirty.store(true);
    workerCv.notify_all();
}

void ofxLinkAudioReceiveStream::setFromChannelName(const std::string& name) {
    { std::lock_guard<std::mutex> lk(stateMutex); fromChannelName_ = name; }
    stateDirty.store(true);
    workerCv.notify_all();
}

void ofxLinkAudioReceiveStream::setEnabled(bool enabled) {
    { std::lock_guard<std::mutex> lk(stateMutex); enabled_ = enabled; }
    stateDirty.store(true);
    workerCv.notify_all();
}

std::string ofxLinkAudioReceiveStream::getFromPeerName() const {
    std::lock_guard<std::mutex> lk(stateMutex); return fromPeerName_;
}
std::string ofxLinkAudioReceiveStream::getFromChannelName() const {
    std::lock_guard<std::mutex> lk(stateMutex); return fromChannelName_;
}
bool ofxLinkAudioReceiveStream::getEnabled() const {
    std::lock_guard<std::mutex> lk(stateMutex); return enabled_;
}

// ----------------------------------------------------------------------------

bool     ofxLinkAudioReceiveStream::isSubscribed()        const { return isSubscribedFlag.load(); }
int      ofxLinkAudioReceiveStream::getNumPeers()         const { return peerCount.load(); }
uint64_t ofxLinkAudioReceiveStream::getFramesReceived()   const { return framesReceived.load(); }
uint64_t ofxLinkAudioReceiveStream::getFramesDropped()    const { return framesDropped.load(); }
uint32_t ofxLinkAudioReceiveStream::getStreamSampleRate() const { return streamSampleRate.load(); }
uint32_t ofxLinkAudioReceiveStream::getStreamNumChannels()const { return streamNumChannels.load(); }

// ----------------------------------------------------------------------------
// Link session control / query
// ----------------------------------------------------------------------------

double ofxLinkAudioReceiveStream::getTempo() {
    if (!manager) return 0.0;
    return manager->tempo();
}

void ofxLinkAudioReceiveStream::setTempo(double bpm) {
    if (!manager) return;
    manager->setTempo(bpm);
}

bool ofxLinkAudioReceiveStream::isTransportPlaying() {
    if (!manager) return false;
    return manager->isPlaying();
}

void ofxLinkAudioReceiveStream::setTransport(bool playing) {
    if (!manager) return;
    manager->setIsPlaying(playing);
}

double ofxLinkAudioReceiveStream::getBeat(double quantum) {
    if (!manager) return 0.0;
    auto& la = manager->linkAudio();
    const auto state = la.captureAppSessionState();
    return state.beatAtTime(la.clock().micros(), quantum);
}

double ofxLinkAudioReceiveStream::getPhase(double quantum) {
    if (!manager) return 0.0;
    auto& la = manager->linkAudio();
    const auto state = la.captureAppSessionState();
    return state.phaseAtTime(la.clock().micros(), quantum);
}

// ----------------------------------------------------------------------------
// Source callback - runs on Link-managed thread
// ----------------------------------------------------------------------------
//
// We define a small helper here that takes the actual BufferHandle by ref
// (the proxy in the .h is just a forward-decl placeholder).

namespace {

void pushBufferToRings(ableton::LinkAudioSource::BufferHandle& bh,
                       AudioRingBuffer& ringL,
                       AudioRingBuffer& ringR,
                       std::atomic<uint32_t>& streamSampleRate,
                       std::atomic<uint32_t>& streamNumChannels,
                       std::atomic<uint64_t>& framesReceived,
                       std::atomic<uint64_t>& framesDropped) {

    const auto& info = bh.info;
    if (info.numFrames == 0 || info.numChannels == 0 || bh.samples == nullptr) return;

    streamSampleRate.store(info.sampleRate);
    streamNumChannels.store(static_cast<uint32_t>(info.numChannels));

    constexpr std::size_t kBatch = 512;
    float scratchL[kBatch];
    float scratchR[kBatch];

    const std::size_t totalFrames = info.numFrames;
    const std::size_t stride      = info.numChannels;
    const bool        isStereo    = (stride >= 2);

    std::size_t framesLeft = totalFrames;
    std::size_t srcOffset  = 0;

    while (framesLeft > 0) {
        const std::size_t n = (framesLeft < kBatch) ? framesLeft : kBatch;

        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t base = (srcOffset + i) * stride;
            scratchL[i] = static_cast<float>(bh.samples[base]) / 32768.0f;
            if (isStereo) scratchR[i] = static_cast<float>(bh.samples[base + 1]) / 32768.0f;
        }

        const std::size_t writtenL = ringL.write(scratchL, n);
        if (writtenL < n) framesDropped.fetch_add(n - writtenL);

        if (isStereo) ringR.write(scratchR, n);
        else          ringR.write(scratchL, n);

        srcOffset  += n;
        framesLeft -= n;
    }

    framesReceived.fetch_add(totalFrames);
}

} // namespace

void ofxLinkAudioReceiveStream::onSourceBuffer(struct ableton_LinkAudioSource_BufferHandle_proxy*) {
    // unused (kept for ABI symmetry); real callback is set as a lambda inside trySubscribe()
}

// ----------------------------------------------------------------------------

void ofxLinkAudioReceiveStream::trySubscribe() {
    auto channels = manager->channels();
    std::optional<ableton::ChannelId> match;
    for (const auto& ch : channels) {
        if (ch.name != subscribedFromChannel) continue;
        if (!subscribedFromPeer.empty() && ch.peerName != subscribedFromPeer) continue;
        match = ch.id;
        break;
    }
    if (!match) return;

    ringL->reset();
    ringR->reset();
    framesReceived.store(0);
    framesDropped.store(0);

    auto& la = manager->linkAudio();
    auto* ringLp = ringL.get();
    auto* ringRp = ringR.get();
    auto* sampleRatePtr = &streamSampleRate;
    auto* numChannelsPtr = &streamNumChannels;
    auto* framesReceivedPtr = &framesReceived;
    auto* framesDroppedPtr = &framesDropped;

    source.reset(new ableton::LinkAudioSource(
        la, *match,
        [ringLp, ringRp, sampleRatePtr, numChannelsPtr, framesReceivedPtr, framesDroppedPtr]
        (ableton::LinkAudioSource::BufferHandle bh) {
            pushBufferToRings(bh, *ringLp, *ringRp,
                              *sampleRatePtr, *numChannelsPtr,
                              *framesReceivedPtr, *framesDroppedPtr);
        }));
}

void ofxLinkAudioReceiveStream::unsubscribeInternal() {
    source.reset();
    if (ringL) ringL->reset();
    if (ringR) ringR->reset();
}

// ----------------------------------------------------------------------------

void ofxLinkAudioReceiveStream::applyState() {
    if (!manager) return;
    auto& la = manager->linkAudio();

    std::string wantFromPeer, wantFromChannel;
    bool wantEnabled;
    {
        std::lock_guard<std::mutex> lk(stateMutex);
        wantFromPeer    = fromPeerName_;
        wantFromChannel = fromChannelName_;
        wantEnabled     = enabled_;
    }

    if (wantEnabled != workerEnabled) {
        la.enable(wantEnabled);
        la.enableLinkAudio(wantEnabled);
        workerEnabled = wantEnabled;
        if (!wantEnabled) {
            unsubscribeInternal();
            subscribedFromChannel.clear();
            subscribedFromPeer.clear();
        }
    }

    if (!workerEnabled) {
        isSubscribedFlag.store(false);
        peerCount.store(static_cast<int>(la.numPeers()));
        return;
    }

    const bool channelChanged = (wantFromChannel != subscribedFromChannel);
    const bool peerChanged    = (wantFromPeer    != subscribedFromPeer);
    if (channelChanged || peerChanged) {
        unsubscribeInternal();
        subscribedFromChannel = wantFromChannel;
        subscribedFromPeer    = wantFromPeer;
    }

    if (!source && !subscribedFromChannel.empty()) {
        trySubscribe();
    }

    isSubscribedFlag.store(source != nullptr);
    peerCount.store(static_cast<int>(la.numPeers()));
}

void ofxLinkAudioReceiveStream::workerThreadLoop() {
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
// Audio thread - drains rings, calls user audioIn()
// ----------------------------------------------------------------------------

void ofxLinkAudioReceiveStream::audioThreadLoop() {
    using namespace std::chrono;

    const auto framePeriod = nanoseconds(
        static_cast<long long>(1e9 * settings.bufferSize / settings.sampleRate));

    auto next = steady_clock::now();

    std::vector<float> scratchL(settings.bufferSize);
    std::vector<float> scratchR(settings.bufferSize);

    while (!audioThreadStop.load()) {

        // Drain rings into scratch
        if (isSubscribedFlag.load() && ringL && ringR) {
            ringL->read(scratchL.data(), static_cast<std::size_t>(settings.bufferSize));
            ringR->read(scratchR.data(), static_cast<std::size_t>(settings.bufferSize));
        } else {
            std::fill(scratchL.begin(), scratchL.end(), 0.0f);
            std::fill(scratchR.begin(), scratchR.end(), 0.0f);
        }

        // Pack into appBuffer (interleaved)
        float* dst = appBuffer.getBuffer().data();
        if (settings.numChannels == 2) {
            for (int i = 0; i < settings.bufferSize; ++i) {
                dst[i * 2 + 0] = scratchL[i];
                dst[i * 2 + 1] = scratchR[i];
            }
        } else {
            for (int i = 0; i < settings.bufferSize; ++i) {
                dst[i] = scratchL[i];
            }
        }

        // Hand off to user
        if (input) {
            input->audioIn(appBuffer);
        }

        // Sleep
        next += framePeriod;
        const auto now = steady_clock::now();
        if (next > now) std::this_thread::sleep_until(next);
        else            next = now;
    }
}
