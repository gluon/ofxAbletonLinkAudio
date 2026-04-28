# ============================================================================
# addon_config.mk for ofxAbletonLinkAudio
#
# Part of the VoidLinkAudio R&D project by Julien Bayle / Structure Void.
# https://julienbayle.net    https://structure-void.com
#
# Released under the MIT License - see LICENSE file at repo root.
# Built on top of Ableton Link Audio (see ACKNOWLEDGEMENTS.md).
# ============================================================================
#
# Self-contained layout: the shared core and the Ableton Link SDK are
# exposed inside libs/ via symlinks (in this dev tree) or via real files
# (in the standalone gluon/ofxAbletonLinkAudio repo).
#
# ============================================================================

meta:
	ADDON_NAME = ofxAbletonLinkAudio
	ADDON_DESCRIPTION = Ableton Link Audio receiver and publisher for openFrameworks
	ADDON_AUTHOR = Julien Bayle / Structure Void
	ADDON_TAGS = "audio" "network" "link" "ableton" "streaming" "sync"
	ADDON_URL = https://structure-void.com

common:
	ADDON_SOURCES += libs/core/LinkAudioManager.cpp

	ADDON_INCLUDES += src
	ADDON_INCLUDES += libs/core
	ADDON_INCLUDES += libs/link/include
	ADDON_INCLUDES += libs/link/modules/asio-standalone/asio/include

	# Link is a header-only library + Asio (header-only too). We do NOT want
# oF to recursively compile everything under libs/link, in particular
# the bundled examples, extensions, tests and CMake scaffolding.
ADDON_SOURCES_EXCLUDE += libs/link/assets/%
ADDON_SOURCES_EXCLUDE += libs/link/ci/%
ADDON_SOURCES_EXCLUDE += libs/link/cmake_include/%
ADDON_SOURCES_EXCLUDE += libs/link/examples/%
ADDON_SOURCES_EXCLUDE += libs/link/extensions/%
ADDON_SOURCES_EXCLUDE += libs/link/src/%
ADDON_SOURCES_EXCLUDE += libs/link/third_party/%
ADDON_SOURCES_EXCLUDE += libs/link/modules/asio-standalone/asio/src/%
ADDON_SOURCES_EXCLUDE += libs/link/modules/asio-standalone/asio/tests/%
ADDON_SOURCES_EXCLUDE += libs/link/modules/asio-standalone/asio/example/%

ADDON_INCLUDES_EXCLUDE += libs/link/assets/%
ADDON_INCLUDES_EXCLUDE += libs/link/ci/%
ADDON_INCLUDES_EXCLUDE += libs/link/cmake_include/%
ADDON_INCLUDES_EXCLUDE += libs/link/examples/%
ADDON_INCLUDES_EXCLUDE += libs/link/extensions/%
ADDON_INCLUDES_EXCLUDE += libs/link/src/%
ADDON_INCLUDES_EXCLUDE += libs/link/third_party/%
ADDON_INCLUDES_EXCLUDE += libs/link/modules/asio-standalone/asio/src/%
ADDON_INCLUDES_EXCLUDE += libs/link/modules/asio-standalone/asio/tests/%
ADDON_INCLUDES_EXCLUDE += libs/link/modules/asio-standalone/asio/example/%

ADDON_CPPFLAGS += -std=c++17

	ADDON_DEFINES += ASIO_STANDALONE
	ADDON_DEFINES += ASIO_NO_DEPRECATED

osx:
	ADDON_DEFINES += LINK_PLATFORM_MACOSX=1
	ADDON_LDFLAGS += -framework CoreFoundation

linux64:
	ADDON_DEFINES += LINK_PLATFORM_LINUX=1
	ADDON_LDFLAGS += -lpthread

linuxarmv6l:
	ADDON_DEFINES += LINK_PLATFORM_LINUX=1
	ADDON_LDFLAGS += -lpthread

linuxarmv7l:
	ADDON_DEFINES += LINK_PLATFORM_LINUX=1
	ADDON_LDFLAGS += -lpthread

linuxaarch64:
	ADDON_DEFINES += LINK_PLATFORM_LINUX=1
	ADDON_LDFLAGS += -lpthread

msys2:
	ADDON_DEFINES += LINK_PLATFORM_WINDOWS=1
	ADDON_DEFINES += _WIN32_WINNT=0x0601
	ADDON_DEFINES += WIN32_LEAN_AND_MEAN
	ADDON_DEFINES += NOMINMAX
	ADDON_LDFLAGS += -lws2_32
	ADDON_LDFLAGS += -liphlpapi
	ADDON_LDFLAGS += -lwinmm

vs:
	ADDON_DEFINES += LINK_PLATFORM_WINDOWS=1
	ADDON_DEFINES += _WIN32_WINNT=0x0601
	ADDON_DEFINES += WIN32_LEAN_AND_MEAN
	ADDON_DEFINES += NOMINMAX
	ADDON_LIBS += ws2_32.lib
	ADDON_LIBS += iphlpapi.lib
	ADDON_LIBS += winmm.lib
