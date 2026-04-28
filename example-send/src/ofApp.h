// ============================================================================
// example-send / ofApp.h - ofxAbletonLinkAudio
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

    void audioOut(ofSoundBuffer& buffer) override;

private:
    enum WaveType  { WAVE_SINE = 0, WAVE_SAW, WAVE_SQUARE, WAVE_TRIANGLE };
    enum EditField { EDIT_NONE = 0, EDIT_PEER, EDIT_CHANNEL };

    static const char* waveName(WaveType w);
    float renderWave(WaveType w, double phase01) const;
    void  drawScope(int x, int y, int w, int h, const std::vector<float>& buf, ofColor c);
    void  beginEdit(EditField f);
    void  commitEdit();
    void  cancelEdit();

    ofxLinkAudioSendStream sendStream;

    // Edit state (main thread only)
    EditField   editField  = EDIT_NONE;
    std::string editBuffer;

    // Audio thread state
    double phaseL = 0.0;
    double phaseR = 0.0;

    // Shared state (atomics)
    std::atomic<int>   waveType {WAVE_SINE};
    std::atomic<float> freqL    {220.0f};
    std::atomic<float> freqR    {330.0f};
    std::atomic<float> amp      {0.2f};

    // Scopes
    std::mutex         scopeMutex;
    std::vector<float> scopeL;
    std::vector<float> scopeR;
    static constexpr std::size_t kScopeSize = 512;
};
