// ============================================================================
// ofxAbletonLinkAudio - main header (umbrella)
//
// Part of the VoidLinkAudio R&D project by Julien Bayle / Structure Void.
// https://julienbayle.net    https://structure-void.com
//
// Released under the MIT License - see LICENSE file at repo root.
// Built on top of Ableton Link Audio (see ACKNOWLEDGEMENTS.md).
// Provided AS IS, without warranty of any kind.
// ============================================================================
//
// Stream audio between openFrameworks apps and Ableton Live, TouchDesigner,
// Max, VCV Rack, or any other Link Audio host on the local network.
//
// Two stream classes mirror oF's ofSoundStream conventions:
//
//   ofxLinkAudioSendStream      publishes audio to a named Link Audio channel
//   ofxLinkAudioReceiveStream   subscribes to a remote channel and pulls audio
//
// Quick example (publishing):
//
//   class ofApp : public ofBaseApp {
//       ofxLinkAudioSendStream sendStream;
//       void setup() {
//           ofxLinkAudioSendSettings s;
//           s.channelName = "oF Out";
//           s.peerName    = "oF App";
//           s.numChannels = 2;
//           s.sampleRate  = 48000;
//           s.bufferSize  = 256;
//           sendStream.setup(s);
//           sendStream.setOutput(*this);
//           sendStream.start();
//       }
//       void audioOut(ofSoundBuffer& buffer) override {
//           // generate audio here
//       }
//   };
//
// ============================================================================

#pragma once

#include "ofxLinkAudioSendStream.h"
#include "ofxLinkAudioReceiveStream.h"
