// ============================================================================
// example-pingpong / ofApp.h - ofxAbletonLinkAudio
//
// Receives audio from one Link Audio channel, applies a stereo delay with
// feedback, and republishes the result onto another Link Audio channel.
// Demonstrates Send + Receive cohabiting in the same application.
// ============================================================================

#pragma once

#include "ofMain.h"
#include "ofxAbletonLinkAudio.h"
#include <atomic>
#include <vector>
#include <mutex>
#include <string>

class ofApp : public ofBaseApp {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;
    void exit() override;

    // Receive callback: pull incoming audio
    void audioIn (ofSoundBuffer& buffer) override;
    // Send callback: produce outgoing audio
    void audioOut(ofSoundBuffer& buffer) override;

private:
    enum EditField {
        EDIT_NONE = 0,
        EDIT_RX_PEER, EDIT_RX_CHANNEL,
        EDIT_TX_PEER, EDIT_TX_CHANNEL
    };

    void  drawScope(int x, int y, int w, int h, const std::vector<float>& buf, ofColor c);
    void  beginEdit(EditField f);
    void  commitEdit();
    void  cancelEdit();
    const char* editLabel(EditField f) const;

    ofxLinkAudioReceiveStream rxStream;
    ofxLinkAudioSendStream    txStream;

    // Edit state (main thread only)
    EditField    editField = EDIT_NONE;
    std::string  editBuffer;

    // ---- Delay line (stereo) ----
    static constexpr std::size_t kDelaySamples = 48000; // 1 sec at 48k
    std::vector<float> delayL;
    std::vector<float> delayR;
    std::size_t        writeIdx = 0;

    std::atomic<float> delayMs   {300.0f};   // 50..900 ms
    std::atomic<float> feedback  {0.45f};    // 0..0.95
    std::atomic<float> wet       {0.5f};     // dry/wet mix

    // Buffer that bridges audioIn (rx) to audioOut (tx) within one block
    std::mutex          bridgeMutex;
    std::vector<float>  bridgeL;
    std::vector<float>  bridgeR;

    // Scopes - rx (input) and tx (output)
    std::mutex         scopeMutex;
    std::vector<float> scopeRxL, scopeRxR;
    std::vector<float> scopeTxL, scopeTxR;
    static constexpr std::size_t kScopeSize = 384;
};
