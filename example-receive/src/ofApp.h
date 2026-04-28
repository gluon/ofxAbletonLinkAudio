// ============================================================================
// example-receive / ofApp.h - ofxAbletonLinkAudio
//
// Subscribes to a Link Audio channel and visualizes the incoming stereo
// audio on two oscilloscopes. From Peer / From Channel are editable.
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

    // ofBaseSoundInput callback - called by the receive stream
    void audioIn(ofSoundBuffer& buffer) override;

private:
    enum EditField { EDIT_NONE = 0, EDIT_FROM_PEER, EDIT_FROM_CHANNEL };

    void  drawScope(int x, int y, int w, int h, const std::vector<float>& buf, ofColor c);
    void  beginEdit(EditField f);
    void  commitEdit();
    void  cancelEdit();

    ofxLinkAudioReceiveStream receiveStream;

    // Edit state (main thread only)
    EditField    editField  = EDIT_NONE;
    std::string  editBuffer;

    // Scope buffers - audio thread fills, main thread draws
    std::mutex         scopeMutex;
    std::vector<float> scopeL;
    std::vector<float> scopeR;
    static constexpr std::size_t kScopeSize = 512;

    // Peak meters (decay over time)
    std::atomic<float> peakL {0.0f};
    std::atomic<float> peakR {0.0f};
};
