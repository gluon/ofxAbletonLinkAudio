// ============================================================================
// example-receive / ofApp.cpp - ofxAbletonLinkAudio
// ============================================================================

#include "ofApp.h"

void ofApp::setup() {
    ofSetFrameRate(60);
    ofBackground(20);

    scopeL.assign(kScopeSize, 0.0f);
    scopeR.assign(kScopeSize, 0.0f);

    ofxLinkAudioReceiveSettings s;
    s.fromPeerName    = "";        // empty = first match wins (any peer)
    s.fromChannelName = "Main";    // try Live's "Main" by default
    s.numChannels     = 2;
    s.sampleRate      = 48000;
    s.bufferSize      = 256;
    s.autoEnable      = true;

    if (!receiveStream.setup(s)) {
        ofLogError() << "Failed to setup receive stream";
        return;
    }

    receiveStream.setInput(*this);
    receiveStream.start();

    ofLog() << "Subscribing to channel '" << s.fromChannelName
            << "' from peer '" << (s.fromPeerName.empty() ? "(any)" : s.fromPeerName) << "'";
}

void ofApp::exit() {
    receiveStream.close();
}

void ofApp::update() {
    // Decay the peak meters over time so they don't get stuck high
    const float decay = 0.92f;
    peakL.store(peakL.load() * decay);
    peakR.store(peakR.load() * decay);
}

void ofApp::draw() {
    // ---- Left column ----
    int y = 30;
    const int dy = 20;

    ofSetColor(255);
    ofDrawBitmapString("ofxAbletonLinkAudio - Receive example",  20, y); y += dy;
    ofDrawBitmapString("--------------------------------------", 20, y); y += dy * 2;

    // From Peer (with edit cursor if focused)
    {
        std::string label = "From Peer:    ";
        std::string value = (editField == EDIT_FROM_PEER)
            ? editBuffer + "_"
            : (receiveStream.getFromPeerName().empty() ? "(any)" : receiveStream.getFromPeerName());
        if (editField == EDIT_FROM_PEER) ofSetColor(255, 220, 100);
        else                              ofSetColor(255);
        ofDrawBitmapString(label + value, 20, y);
        y += dy;
    }

    // From Channel
    {
        std::string label = "From Channel: ";
        std::string value = (editField == EDIT_FROM_CHANNEL)
            ? editBuffer + "_"
            : receiveStream.getFromChannelName();
        if (editField == EDIT_FROM_CHANNEL) ofSetColor(255, 220, 100);
        else                                 ofSetColor(255);
        ofDrawBitmapString(label + value, 20, y);
        y += dy;
    }

    ofSetColor(255);
    ofDrawBitmapString("Subscribed:   " + std::string(receiveStream.isSubscribed() ? "yes" : "no"), 20, y); y += dy;
    ofDrawBitmapString("Peers:        " + ofToString(receiveStream.getNumPeers()),                  20, y); y += dy * 2;

    // Stream info (when subscribed)
    if (receiveStream.isSubscribed()) {
        ofDrawBitmapString("Stream rate:  " + ofToString(receiveStream.getStreamSampleRate())  + " Hz", 20, y); y += dy;
        ofDrawBitmapString("Stream chans: " + ofToString(receiveStream.getStreamNumChannels()),         20, y); y += dy * 2;
    } else {
        ofSetColor(140);
        ofDrawBitmapString("Stream rate:  --",  20, y); y += dy;
        ofDrawBitmapString("Stream chans: --",  20, y); y += dy * 2;
        ofSetColor(255);
    }

    ofDrawBitmapString("Recv:  " + ofToString(receiveStream.getFramesReceived()), 20, y); y += dy;
    ofDrawBitmapString("Drop:  " + ofToString(receiveStream.getFramesDropped()),  20, y); y += dy * 2;

    // Link session timing (read live from the shared session - main thread only)
    ofSetColor(180, 230, 180);
    ofDrawBitmapString("Tempo:        " + ofToString(receiveStream.getTempo(), 1) + " BPM",                              20, y); y += dy;
    ofDrawBitmapString("Phase:        " + ofToString(receiveStream.getPhase(4.0), 3) + "  (q=4)",                        20, y); y += dy;
    ofDrawBitmapString("Transport:    " + std::string(receiveStream.isTransportPlaying() ? "playing" : "stopped"),       20, y); y += dy * 2;
    ofSetColor(255);

    // ---- Help ----
    ofSetColor(150);
    if (editField == EDIT_NONE) {
        ofDrawBitmapString("[p] edit From Peer    [c] edit From Channel",   20, y); y += dy;
        ofDrawBitmapString("[space] toggle enable",                         20, y); y += dy;
        ofDrawBitmapString("[t/y] tempo -/+       [r] toggle transport",    20, y); y += dy * 2;
    } else {
        ofDrawBitmapString("Editing: type, [enter] commit, [esc] cancel",   20, y); y += dy * 2;
    }

    ofSetColor(110);
    ofDrawBitmapString("Subscribes to a Link Audio channel and visualizes the incoming stream.", 20, y); y += 18;
    ofDrawBitmapString("Open Live / Max / TD / VCV / another oF and publish to this channel.", 20, y);

    // ---- Right column: stereo scopes + peak ----
    const int scopeX = 340;
    const int scopeW = ofGetWidth() - scopeX - 20;
    const int scopeH = 70;

    ofSetColor(255);
    ofDrawBitmapString("L", scopeX - 12, 95);
    drawScope(scopeX, 70, scopeW, scopeH, scopeL, ofColor(80, 200, 255));

    ofDrawBitmapString("R", scopeX - 12, 175);
    drawScope(scopeX, 150, scopeW, scopeH, scopeR, ofColor(255, 140, 80));

    // Peak meters (small bars under the scopes)
    {
        const int meterY = 230;
        const int meterH = 8;
        const float pL = std::min(1.0f, peakL.load());
        const float pR = std::min(1.0f, peakR.load());

        ofSetColor(40); ofNoFill(); ofDrawRectangle(scopeX, meterY,         scopeW, meterH);
        ofSetColor(40); ofNoFill(); ofDrawRectangle(scopeX, meterY + 14,    scopeW, meterH);

        ofSetColor(80, 200, 255); ofFill(); ofDrawRectangle(scopeX, meterY,      pL * scopeW, meterH);
        ofSetColor(255, 140, 80); ofFill(); ofDrawRectangle(scopeX, meterY + 14, pR * scopeW, meterH);

        ofSetColor(180);
        ofDrawBitmapString("peak L: " + ofToString(pL, 2), scopeX, meterY + meterH + 24);
        ofDrawBitmapString("peak R: " + ofToString(pR, 2), scopeX + 130, meterY + meterH + 24);
    }
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

// ----------------------------------------------------------------------------
// Edit mode helpers
// ----------------------------------------------------------------------------

void ofApp::beginEdit(EditField f) {
    editField = f;
    if      (f == EDIT_FROM_PEER)    editBuffer = receiveStream.getFromPeerName();
    else if (f == EDIT_FROM_CHANNEL) editBuffer = receiveStream.getFromChannelName();
    else                             editBuffer.clear();
}

void ofApp::commitEdit() {
    if      (editField == EDIT_FROM_PEER)    receiveStream.setFromPeerName(editBuffer);
    else if (editField == EDIT_FROM_CHANNEL) receiveStream.setFromChannelName(editBuffer);

    editField = EDIT_NONE;
    editBuffer.clear();
}

void ofApp::cancelEdit() {
    editField = EDIT_NONE;
    editBuffer.clear();
}

void ofApp::keyPressed(int key) {
    if (editField != EDIT_NONE) {
        // Edit mode: capture characters
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

    // Normal mode
    switch (key) {
        case 'p': beginEdit(EDIT_FROM_PEER);    break;
        case 'c': beginEdit(EDIT_FROM_CHANNEL); break;
        case ' ': receiveStream.setEnabled(!receiveStream.getEnabled()); break;

        // Link session control (writes propagate to all peers)
        case 't': receiveStream.setTempo(std::max( 20.0, receiveStream.getTempo() - 1.0)); break;
        case 'y': receiveStream.setTempo(std::min(999.0, receiveStream.getTempo() + 1.0)); break;
        case 'r': receiveStream.setTransport(!receiveStream.isTransportPlaying());         break;
    }
}

// ----------------------------------------------------------------------------
// Audio callback - the wrapper has filled buffer with received audio
// ----------------------------------------------------------------------------

void ofApp::audioIn(ofSoundBuffer& buffer) {
    const std::size_t nFrames = buffer.getNumFrames();
    const std::size_t nCh     = buffer.getNumChannels();
    const float*      in      = buffer.getBuffer().data();

    std::vector<float> tmpL, tmpR;
    tmpL.reserve(nFrames);
    tmpR.reserve(nFrames);

    float pL = 0.0f, pR = 0.0f;

    if (nCh == 2) {
        for (std::size_t i = 0; i < nFrames; ++i) {
            const float sL = in[i * 2 + 0];
            const float sR = in[i * 2 + 1];
            tmpL.push_back(sL);
            tmpR.push_back(sR);
            pL = std::max(pL, std::abs(sL));
            pR = std::max(pR, std::abs(sR));
        }
    } else {
        for (std::size_t i = 0; i < nFrames; ++i) {
            const float s = in[i];
            tmpL.push_back(s);
            tmpR.push_back(s);
            pL = std::max(pL, std::abs(s));
            pR = std::max(pR, std::abs(s));
        }
    }

    // Latch peaks if higher than current
    float curL = peakL.load();
    if (pL > curL) peakL.store(pL);
    float curR = peakR.load();
    if (pR > curR) peakR.store(pR);

    // Push into scope buffers
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
