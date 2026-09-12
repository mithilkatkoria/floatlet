#pragma once
#include <windows.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Foundation.h>
#include <memory>
#include <mutex>
#include <atomic>
#include <winrt/Windows.Storage.Streams.h>
#include "artwork.h"
namespace delight {
struct MediaSnapshot {std::wstring title=L"No media session",artist=L"Open a player to begin";bool playing=false,play=false,previous=false,next=false;bool spotify=false;std::shared_ptr<const Artwork> artwork;std::wstring source;};
class Media:public std::enable_shared_from_this<Media> {
    std::mutex mutex;
    MediaSnapshot snapshot;
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSessionManager manager{nullptr};
    winrt::Windows::Media::Control::GlobalSystemMediaTransportControlsSession session{nullptr};
    winrt::event_token current{},properties{},playback{};
    std::atomic<HWND> hwnd{}; unsigned generation=0;
    bool decoding=false;
    unsigned pendingGeneration=0;
    winrt::Windows::Storage::Streams::IRandomAccessStreamReference pendingArt{nullptr};
    winrt::fire_and_forget decodeArtwork();
    void bind();
    winrt::fire_and_forget refresh();
public:
    explicit Media(HWND w):hwnd(w){}
    winrt::fire_and_forget start();
    winrt::fire_and_forget command(int id);
    MediaSnapshot get();
    void stop();
};
}
