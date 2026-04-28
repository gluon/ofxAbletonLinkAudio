// ============================================================================
// example-pingpong / main.cpp - ofxAbletonLinkAudio
// ============================================================================

#include "ofMain.h"
#include "ofApp.h"

int main() {
    ofGLFWWindowSettings settings;
    settings.setSize(820, 560);
    settings.windowMode = OF_WINDOW;
    settings.title = "ofxAbletonLinkAudio - Pingpong";

    auto window = ofCreateWindow(settings);
    ofRunApp(window, std::make_shared<ofApp>());
    ofRunMainLoop();
}
