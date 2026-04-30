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
// Both classes also expose the shared Link session timing (tempo, beat,
// phase, transport) as plain getters and setters, so an oF app can stay
// in sync with Live, Max, TD, etc. — both reading the live session value
// and writing back to it (changes propagate to all peers).
//
// Quick example (publishing + reading session timing):
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
//       void update() {
//           // Read the live session timing (app thread)
//           double tempo = sendStream.getTempo();
//           double phase = sendStream.getPhase(4.0);
//           bool   playing = sendStream.isTransportPlaying();
//           // Push a new tempo to all peers
//           // sendStream.setTempo(128.0);
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
