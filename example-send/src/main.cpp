// ============================================================================
// example-send / main.cpp - ofxAbletonLinkAudio
// ============================================================================

#include "ofMain.h"
#include "ofApp.h"

int main() {
    ofGLFWWindowSettings settings;
    settings.setSize(720, 520);
    settings.windowMode = OF_WINDOW;
    settings.title = "ofxAbletonLinkAudio - Send";

    auto window = ofCreateWindow(settings);
    ofRunApp(window, std::make_shared<ofApp>());
    ofRunMainLoop();
}
