// ============================================================================
// example-send / ofApp.cpp - ofxAbletonLinkAudio
// ============================================================================

#include "ofApp.h"

static constexpr double kTwoPi = 6.28318530717958647692;

const char* ofApp::waveName(WaveType w) {
    switch (w) {
        case WAVE_SINE:     return "sine";
        case WAVE_SAW:      return "saw";
        case WAVE_SQUARE:   return "square";
        case WAVE_TRIANGLE: return "triangle";
    }
    return "?";
}

float ofApp::renderWave(WaveType w, double phase01) const {
    switch (w) {
        case WAVE_SINE:
            return std::sin(phase01 * kTwoPi);
        case WAVE_SAW:
            return static_cast<float>(2.0 * phase01 - 1.0);
        case WAVE_SQUARE:
            return phase01 < 0.5 ? 1.0f : -1.0f;
        case WAVE_TRIANGLE:
            return phase01 < 0.5
                ? static_cast<float>(4.0 * phase01 - 1.0)
                : static_cast<float>(3.0 - 4.0 * phase01);
    }
    return 0.0f;
}

void ofApp::setup() {
    ofSetFrameRate(60);
    ofBackground(20);

    scopeL.assign(kScopeSize, 0.0f);
    scopeR.assign(kScopeSize, 0.0f);

    ofxLinkAudioSendSettings s;
    s.channelName = "oF Out";
    s.peerName    = "oF App";
    s.numChannels = 2;
    s.sampleRate  = 48000;
    s.bufferSize  = 256;
    s.autoEnable  = true;

    if (!sendStream.setup(s)) {
        ofLogError() << "Failed to setup send stream";
        return;
    }

    sendStream.setOutput(*this);
    sendStream.start();

    ofLog() << "Publishing channel '" << s.channelName
            << "' as peer '" << s.peerName << "'";
}

void ofApp::exit() {
    sendStream.close();
}

void ofApp::update() {
    // nothing per-frame
}

void ofApp::draw() {
    int y = 30;
    const int dy = 20;

    ofSetColor(255);
    ofDrawBitmapString("ofxAbletonLinkAudio - Send example",  20, y); y += dy;
    ofDrawBitmapString("-----------------------------------", 20, y); y += dy * 2;

    // Peer name (with edit cursor if focused)
    {
        std::string label = "Peer name:  ";
        std::string value = (editField == EDIT_PEER)
            ? editBuffer + "_"
            : sendStream.getPeerName();
        if (editField == EDIT_PEER) ofSetColor(255, 220, 100);
        else                         ofSetColor(255);
        ofDrawBitmapString(label + value, 20, y);
        y += dy;
    }

    // Channel
    {
        std::string label = "Channel:    ";
        std::string value = (editField == EDIT_CHANNEL)
            ? editBuffer + "_"
            : sendStream.getChannelName();
        if (editField == EDIT_CHANNEL) ofSetColor(255, 220, 100);
        else                            ofSetColor(255);
        ofDrawBitmapString(label + value, 20, y);
        y += dy;
    }

    ofSetColor(255);
    ofDrawBitmapString("Publishing: " + std::string(sendStream.isPublishing() ? "yes" : "no"), 20, y); y += dy;
    ofDrawBitmapString("Peers:      " + ofToString(sendStream.getNumPeers()), 20, y); y += dy * 2;

    const WaveType w = static_cast<WaveType>(waveType.load());
    ofDrawBitmapString(std::string("Wave:    ") + waveName(w),                    20, y); y += dy;
    ofDrawBitmapString("Freq L:  " + ofToString(freqL.load(), 1) + " Hz",         20, y); y += dy;
    ofDrawBitmapString("Freq R:  " + ofToString(freqR.load(), 1) + " Hz",         20, y); y += dy;
    ofDrawBitmapString("Amp:     " + ofToString(amp.load(),   2),                 20, y); y += dy * 2;

    ofDrawBitmapString("Pub:  " + ofToString(sendStream.getFramesPublished()), 20, y); y += dy;
    ofDrawBitmapString("Drop: " + ofToString(sendStream.getFramesDropped()),   20, y); y += dy * 2;

    // Link session timing (read live from the shared session — main thread only)
    ofSetColor(180, 230, 180);
    ofDrawBitmapString("Tempo:     " + ofToString(sendStream.getTempo(), 1) + " BPM",                                  20, y); y += dy;
    ofDrawBitmapString("Phase:     " + ofToString(sendStream.getPhase(4.0), 3) + "  (q=4)",                            20, y); y += dy;
    ofDrawBitmapString("Transport: " + std::string(sendStream.isTransportPlaying() ? "playing" : "stopped"),           20, y); y += dy * 2;
    ofSetColor(255);

    // Help
    ofSetColor(150);
    if (editField == EDIT_NONE) {
        ofDrawBitmapString("[p] edit Peer name    [c] edit Channel",          20, y); y += dy;
        ofDrawBitmapString("[1] sine   [2] saw   [3] square   [4] triangle",  20, y); y += dy;
        ofDrawBitmapString("[a/z] freq L -/+      [q/s] freq R -/+",          20, y); y += dy;
        ofDrawBitmapString("[d/e] amp  -/+        [space] toggle enable",     20, y); y += dy;
        ofDrawBitmapString("[t/y] tempo -/+       [r] toggle transport",      20, y); y += dy * 2;
    } else {
        ofDrawBitmapString("Editing: type, [enter] commit, [esc] cancel",     20, y); y += dy * 2;
    }

    ofSetColor(110);
    ofDrawBitmapString("Publishes a stereo test signal as a Link Audio channel.", 20, y); y += 18;
    ofDrawBitmapString("Open Live / Max / TD / VCV and subscribe to this channel.", 20, y);

    // Scopes
    const int scopeX = 340;
    const int scopeW = ofGetWidth() - scopeX - 20;
    const int scopeH = 70;

    ofSetColor(255);
    ofDrawBitmapString("L", scopeX - 12, 95);
    drawScope(scopeX, 70, scopeW, scopeH, scopeL, ofColor(80, 200, 255));

    ofDrawBitmapString("R", scopeX - 12, 175);
    drawScope(scopeX, 150, scopeW, scopeH, scopeR, ofColor(255, 140, 80));
}

void ofApp::drawScope(int x, int y, int w, int h, const std::vector<float>& buf, ofColor c) {
    ofPushStyle();

    ofSetColor(40);
    ofNoFill();
    ofDrawRectangle(x, y, w, h);

    ofSetColor(60);
    ofDrawLine(x, y + h * 0.5f, x + w, y + h * 0.5f);

    ofSetColor(c);
    ofNoFill();
    ofBeginShape();
    {
        std::lock_guard<std::mutex> lk(const_cast<std::mutex&>(scopeMutex));
        const std::size_t n = buf.size();
        if (n > 0) {
            for (std::size_t i = 0; i < n; ++i) {
                const float fx = x + (w * static_cast<float>(i)) / static_cast<float>(n - 1);
                const float fy = y + h * 0.5f - buf[i] * (h * 0.45f);
                ofVertex(fx, fy);
            }
        }
    }
    ofEndShape();

    ofPopStyle();
}

void ofApp::beginEdit(EditField f) {
    editField = f;
    if      (f == EDIT_PEER)    editBuffer = sendStream.getPeerName();
    else if (f == EDIT_CHANNEL) editBuffer = sendStream.getChannelName();
    else                        editBuffer.clear();
}

void ofApp::commitEdit() {
    if      (editField == EDIT_PEER)    sendStream.setPeerName(editBuffer);
    else if (editField == EDIT_CHANNEL) sendStream.setChannelName(editBuffer);
    editField = EDIT_NONE;
    editBuffer.clear();
}

void ofApp::cancelEdit() {
    editField = EDIT_NONE;
    editBuffer.clear();
}

void ofApp::keyPressed(int key) {
    if (editField != EDIT_NONE) {
        if (key == OF_KEY_RETURN) {
            commitEdit();
        } else if (key == OF_KEY_ESC) {
            cancelEdit();
        } else if (key == OF_KEY_BACKSPACE || key == OF_KEY_DEL) {
            if (!editBuffer.empty()) editBuffer.pop_back();
        } else if (key >= 32 && key < 127) {
            editBuffer.push_back(static_cast<char>(key));
        }
        return;
    }

    switch (key) {
        case 'p': beginEdit(EDIT_PEER);    break;
        case 'c': beginEdit(EDIT_CHANNEL); break;

        case '1': waveType.store(WAVE_SINE);     break;
        case '2': waveType.store(WAVE_SAW);      break;
        case '3': waveType.store(WAVE_SQUARE);   break;
        case '4': waveType.store(WAVE_TRIANGLE); break;

        case 'a': freqL.store(std::max(20.0f,    freqL.load() - 10.0f)); break;
        case 'z': freqL.store(std::min(20000.0f, freqL.load() + 10.0f)); break;
        case 'q': freqR.store(std::max(20.0f,    freqR.load() - 10.0f)); break;
        case 's': freqR.store(std::min(20000.0f, freqR.load() + 10.0f)); break;

        case 'd': amp.store(std::max(0.0f, amp.load() - 0.05f)); break;
        case 'e': amp.store(std::min(1.0f, amp.load() + 0.05f)); break;

        case ' ': sendStream.setEnabled(!sendStream.getEnabled()); break;

        // Link session control (writes propagate to all peers)
        case 't': sendStream.setTempo(std::max( 20.0, sendStream.getTempo() - 1.0)); break;
        case 'y': sendStream.setTempo(std::min(999.0, sendStream.getTempo() + 1.0)); break;
        case 'r': sendStream.setTransport(!sendStream.isTransportPlaying());         break;
    }
}

void ofApp::audioOut(ofSoundBuffer& buffer) {
    const float sr = static_cast<float>(buffer.getSampleRate());
    const std::size_t nFrames = buffer.getNumFrames();
    const std::size_t nCh     = buffer.getNumChannels();

    const WaveType w  = static_cast<WaveType>(waveType.load());
    const double   fL = static_cast<double>(freqL.load());
    const double   fR = static_cast<double>(freqR.load());
    const float    a  = sendStream.getEnabled() ? amp.load() : 0.0f;

    const double dpL = fL / sr;
    const double dpR = fR / sr;

    float* out = buffer.getBuffer().data();

    std::vector<float> tmpL, tmpR;
    tmpL.reserve(nFrames);
    tmpR.reserve(nFrames);

    for (std::size_t i = 0; i < nFrames; ++i) {
        const float sL = renderWave(w, phaseL) * a;
        const float sR = renderWave(w, phaseR) * a;

        if (nCh == 2) {
            out[i * 2 + 0] = sL;
            out[i * 2 + 1] = sR;
        } else {
            out[i] = (sL + sR) * 0.5f;
        }

        tmpL.push_back(sL);
        tmpR.push_back(sR);

        phaseL += dpL;
        phaseR += dpR;
        if (phaseL >= 1.0) phaseL -= 1.0;
        if (phaseR >= 1.0) phaseR -= 1.0;
    }

    {
        std::lock_guard<std::mutex> lk(scopeMutex);
        const std::size_t n = std::min(static_cast<std::size_t>(kScopeSize), tmpL.size());
        scopeL.resize(n);
        scopeR.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t srcIdx = (i * tmpL.size()) / n;
            scopeL[i] = tmpL[srcIdx];
            scopeR[i] = tmpR[srcIdx];
        }
    }
}
