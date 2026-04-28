// ============================================================================
// example-pingpong / ofApp.cpp - ofxAbletonLinkAudio
// ============================================================================

#include "ofApp.h"

void ofApp::setup() {
    ofSetFrameRate(60);
    ofBackground(20);

    delayL.assign(kDelaySamples, 0.0f);
    delayR.assign(kDelaySamples, 0.0f);

    scopeRxL.assign(kScopeSize, 0.0f);
    scopeRxR.assign(kScopeSize, 0.0f);
    scopeTxL.assign(kScopeSize, 0.0f);
    scopeTxR.assign(kScopeSize, 0.0f);

    // ---- Receive stream ----
    ofxLinkAudioReceiveSettings rs;
    rs.fromPeerName    = "Live";
    rs.fromChannelName = "Main";
    rs.numChannels     = 2;
    rs.sampleRate      = 48000;
    rs.bufferSize      = 256;
    rs.autoEnable      = true;

    if (!rxStream.setup(rs)) {
        ofLogError() << "Failed to setup receive stream";
        return;
    }
    rxStream.setInput(*this);
    rxStream.start();

    // ---- Send stream ----
    ofxLinkAudioSendSettings ts;
    ts.channelName = "oF Pingpong";
    ts.peerName    = "oF Pingpong";
    ts.numChannels = 2;
    ts.sampleRate  = 48000;
    ts.bufferSize  = 256;
    ts.autoEnable  = true;

    if (!txStream.setup(ts)) {
        ofLogError() << "Failed to setup send stream";
        return;
    }
    txStream.setOutput(*this);
    txStream.start();

    ofLog() << "Pingpong: receiving '"  << rs.fromChannelName << "' from '" << rs.fromPeerName << "'"
            << ", republishing on '" << ts.channelName    << "' as '"   << ts.peerName    << "'";
}

void ofApp::exit() {
    rxStream.close();
    txStream.close();
}

void ofApp::update() {
    // nothing per-frame
}

const char* ofApp::editLabel(EditField f) const {
    switch (f) {
        case EDIT_RX_PEER:    return "Rx From Peer";
        case EDIT_RX_CHANNEL: return "Rx From Channel";
        case EDIT_TX_PEER:    return "Tx Peer";
        case EDIT_TX_CHANNEL: return "Tx Channel";
        default:              return "";
    }
}

void ofApp::draw() {
    int y = 30;
    const int dy = 20;

    // ---- Title ----
    ofSetColor(255);
    ofDrawBitmapString("ofxAbletonLinkAudio - Pingpong example",   20, y); y += dy;
    ofDrawBitmapString("---------------------------------------",  20, y); y += dy * 2;

    // ---- RX block ----
    ofSetColor(120, 200, 255);
    ofDrawBitmapString("RECEIVING", 20, y); y += dy;

    {
        std::string label = "Rx From Peer:    ";
        std::string value = (editField == EDIT_RX_PEER)
            ? editBuffer + "_"
            : (rxStream.getFromPeerName().empty() ? "(any)" : rxStream.getFromPeerName());
        if (editField == EDIT_RX_PEER) ofSetColor(255, 220, 100);
        else                            ofSetColor(220);
        ofDrawBitmapString(label + value, 20, y); y += dy;
    }
    {
        std::string label = "Rx From Channel: ";
        std::string value = (editField == EDIT_RX_CHANNEL)
            ? editBuffer + "_"
            : rxStream.getFromChannelName();
        if (editField == EDIT_RX_CHANNEL) ofSetColor(255, 220, 100);
        else                               ofSetColor(220);
        ofDrawBitmapString(label + value, 20, y); y += dy;
    }
    ofSetColor(220);
    ofDrawBitmapString(std::string("Subscribed:      ") + (rxStream.isSubscribed() ? "yes" : "no"), 20, y); y += dy * 2;

    // ---- TX block ----
    ofSetColor(255, 180, 120);
    ofDrawBitmapString("SENDING", 20, y); y += dy;

    {
        std::string label = "Tx Peer:         ";
        std::string value = (editField == EDIT_TX_PEER)
            ? editBuffer + "_"
            : txStream.getPeerName();
        if (editField == EDIT_TX_PEER) ofSetColor(255, 220, 100);
        else                            ofSetColor(220);
        ofDrawBitmapString(label + value, 20, y); y += dy;
    }
    {
        std::string label = "Tx Channel:      ";
        std::string value = (editField == EDIT_TX_CHANNEL)
            ? editBuffer + "_"
            : txStream.getChannelName();
        if (editField == EDIT_TX_CHANNEL) ofSetColor(255, 220, 100);
        else                               ofSetColor(220);
        ofDrawBitmapString(label + value, 20, y); y += dy;
    }
    ofSetColor(220);
    ofDrawBitmapString(std::string("Publishing:      ") + (txStream.isPublishing() ? "yes" : "no"), 20, y); y += dy * 2;

    // ---- Effect params ----
    ofSetColor(255);
    ofDrawBitmapString("DELAY", 20, y); y += dy;
    ofSetColor(220);
    ofDrawBitmapString("Time:     " + ofToString(delayMs.load(),  0) + " ms", 20, y); y += dy;
    ofDrawBitmapString("Feedback: " + ofToString(feedback.load(), 2),         20, y); y += dy;
    ofDrawBitmapString("Wet:      " + ofToString(wet.load(),      2),         20, y); y += dy * 2;

    // ---- Help ----
    ofSetColor(150);
    if (editField == EDIT_NONE) {
        ofDrawBitmapString("[1/2] Rx Peer / Channel    [3/4] Tx Peer / Channel", 20, y); y += dy;
        ofDrawBitmapString("[a/z] time -/+   [q/s] fb -/+   [d/e] wet -/+",      20, y); y += 30;

        ofSetColor(110);
        ofDrawBitmapString("Receives a Link Audio channel, applies a stereo ping-pong delay,",        20, y); y += 18;
        ofDrawBitmapString("and re-publishes the result as a new Link Audio channel.",                20, y);
    } else {
        ofDrawBitmapString(std::string("Editing ") + editLabel(editField)
                           + ": type, [enter] commit, [esc] cancel", 20, y);
    }

    // ---- Right column: 4 small scopes (rx L/R top, tx L/R bottom) ----
    const int scopeX = 380;
    const int scopeW = ofGetWidth() - scopeX - 20;
    const int scopeH = 50;

    int sy = 60;
    ofSetColor(255);
    ofDrawBitmapString("Rx L", scopeX - 36, sy + 30);
    drawScope(scopeX, sy, scopeW, scopeH, scopeRxL, ofColor(80, 200, 255));
    sy += scopeH + 10;

    ofDrawBitmapString("Rx R", scopeX - 36, sy + 30);
    drawScope(scopeX, sy, scopeW, scopeH, scopeRxR, ofColor(80, 160, 220));
    sy += scopeH + 30;

    ofDrawBitmapString("Tx L", scopeX - 36, sy + 30);
    drawScope(scopeX, sy, scopeW, scopeH, scopeTxL, ofColor(255, 140, 80));
    sy += scopeH + 10;

    ofDrawBitmapString("Tx R", scopeX - 36, sy + 30);
    drawScope(scopeX, sy, scopeW, scopeH, scopeTxR, ofColor(220, 110, 60));
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
    switch (f) {
        case EDIT_RX_PEER:    editBuffer = rxStream.getFromPeerName();    break;
        case EDIT_RX_CHANNEL: editBuffer = rxStream.getFromChannelName(); break;
        case EDIT_TX_PEER:    editBuffer = txStream.getPeerName();        break;
        case EDIT_TX_CHANNEL: editBuffer = txStream.getChannelName();     break;
        default: editBuffer.clear();
    }
}

void ofApp::commitEdit() {
    switch (editField) {
        case EDIT_RX_PEER:    rxStream.setFromPeerName(editBuffer);    break;
        case EDIT_RX_CHANNEL: rxStream.setFromChannelName(editBuffer); break;
        case EDIT_TX_PEER:    txStream.setPeerName(editBuffer);        break;
        case EDIT_TX_CHANNEL: txStream.setChannelName(editBuffer);     break;
        default: break;
    }
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
        case '1': beginEdit(EDIT_RX_PEER);    break;
        case '2': beginEdit(EDIT_RX_CHANNEL); break;
        case '3': beginEdit(EDIT_TX_PEER);    break;
        case '4': beginEdit(EDIT_TX_CHANNEL); break;

        case 'a': delayMs.store(std::max( 50.0f, delayMs.load() - 25.0f)); break;
        case 'z': delayMs.store(std::min(900.0f, delayMs.load() + 25.0f)); break;
        case 'q': feedback.store(std::max(0.0f,  feedback.load() - 0.05f)); break;
        case 's': feedback.store(std::min(0.95f, feedback.load() + 0.05f)); break;
        case 'd': wet.store(std::max(0.0f, wet.load() - 0.05f)); break;
        case 'e': wet.store(std::min(1.0f, wet.load() + 0.05f)); break;
    }
}

// ----------------------------------------------------------------------------
// audioIn: receive callback. Stash incoming samples in the bridge buffer.
// ----------------------------------------------------------------------------
void ofApp::audioIn(ofSoundBuffer& buffer) {
    const std::size_t nFrames = buffer.getNumFrames();
    const std::size_t nCh     = buffer.getNumChannels();
    const float*      in      = buffer.getBuffer().data();

    std::vector<float> tmpL, tmpR;
    tmpL.reserve(nFrames);
    tmpR.reserve(nFrames);

    if (nCh == 2) {
        for (std::size_t i = 0; i < nFrames; ++i) {
            tmpL.push_back(in[i * 2 + 0]);
            tmpR.push_back(in[i * 2 + 1]);
        }
    } else {
        for (std::size_t i = 0; i < nFrames; ++i) {
            tmpL.push_back(in[i]);
            tmpR.push_back(in[i]);
        }
    }

    // Push into bridge for audioOut to pick up
    {
        std::lock_guard<std::mutex> lk(bridgeMutex);
        bridgeL = std::move(tmpL);
        bridgeR = std::move(tmpR);
    }

    // Feed Rx scopes
    {
        std::lock_guard<std::mutex> lk(scopeMutex);
        const std::size_t n = std::min(static_cast<std::size_t>(kScopeSize), nFrames);
        scopeRxL.resize(n);
        scopeRxR.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t srcIdx = (i * nFrames) / n;
            scopeRxL[i] = (nCh == 2) ? in[srcIdx * 2 + 0] : in[srcIdx];
            scopeRxR[i] = (nCh == 2) ? in[srcIdx * 2 + 1] : in[srcIdx];
        }
    }
}

// ----------------------------------------------------------------------------
// audioOut: send callback. Pull from the bridge, apply delay, write out.
// ----------------------------------------------------------------------------
void ofApp::audioOut(ofSoundBuffer& buffer) {
    const float sr = static_cast<float>(buffer.getSampleRate());
    const std::size_t nFrames = buffer.getNumFrames();
    const std::size_t nCh     = buffer.getNumChannels();

    // Pick up bridge content
    std::vector<float> inL, inR;
    {
        std::lock_guard<std::mutex> lk(bridgeMutex);
        inL = bridgeL;
        inR = bridgeR;
    }

    // If sizes don't match, pad/truncate to nFrames
    inL.resize(nFrames, 0.0f);
    inR.resize(nFrames, 0.0f);

    const float dms  = delayMs.load();
    const float fb   = feedback.load();
    const float w    = wet.load();
    const float dry  = 1.0f - w;

    const std::size_t dSamples = std::min(
        static_cast<std::size_t>(dms * sr / 1000.0f),
        kDelaySamples - 1);

    float* out = buffer.getBuffer().data();

    std::vector<float> tmpL, tmpR;
    tmpL.reserve(nFrames);
    tmpR.reserve(nFrames);

    for (std::size_t i = 0; i < nFrames; ++i) {
        // Read tail of delay line
        const std::size_t readIdx = (writeIdx + kDelaySamples - dSamples) % kDelaySamples;
        const float dL = delayL[readIdx];
        const float dR = delayR[readIdx];

        // Mix dry + wet
        const float outL = inL[i] * dry + dL * w;
        const float outR = inR[i] * dry + dR * w;

        // Write back to delay line: input + ping-pong feedback (cross channels)
        delayL[writeIdx] = inL[i] + dR * fb;
        delayR[writeIdx] = inR[i] + dL * fb;

        writeIdx = (writeIdx + 1) % kDelaySamples;

        if (nCh == 2) {
            out[i * 2 + 0] = outL;
            out[i * 2 + 1] = outR;
        } else {
            out[i] = (outL + outR) * 0.5f;
        }

        tmpL.push_back(outL);
        tmpR.push_back(outR);
    }

    // Feed Tx scopes
    {
        std::lock_guard<std::mutex> lk(scopeMutex);
        const std::size_t n = std::min(static_cast<std::size_t>(kScopeSize), tmpL.size());
        scopeTxL.resize(n);
        scopeTxR.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            const std::size_t srcIdx = (i * tmpL.size()) / n;
            scopeTxL[i] = tmpL[srcIdx];
            scopeTxR[i] = tmpR[srcIdx];
        }
    }
}
