#include "media.h"
#include "media_brand.h"
#include "catalogue_art.h"
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.Graphics.Imaging.h>
using namespace winrt;
using namespace Windows::Media::Control;
namespace delight {
namespace {
bool appleMusicBrowserWindow(const std::wstring& source){
    struct Probe{std::wstring browser;bool found=false;} probe{mediaBrowser(source)};
    if(probe.browser.empty())return false;
    // Only inspect top-level titles for this browser, on a media metadata event.
    // No browser history, page content, hooks or polling are used.
    EnumWindows([](HWND window,LPARAM data)->BOOL{
        auto& p=*reinterpret_cast<Probe*>(data);if(!IsWindowVisible(window))return TRUE;
        wchar_t title[512]{};GetWindowTextW(window,title,512);if(!appleMusicName(title))return TRUE;
        DWORD pid=0;GetWindowThreadProcessId(window,&pid);auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return TRUE;
        wchar_t path[32768]{};DWORD length=32768;bool matched=QueryFullProcessImageNameW(process,0,path,&length)&&mediaBrowser(path)==p.browser;CloseHandle(process);
        if(matched){p.found=true;return FALSE;}return TRUE;
    },reinterpret_cast<LPARAM>(&probe));return probe.found;
}
}

fire_and_forget Media::start() {
    auto self=shared_from_this();
    try {
        auto result=co_await GlobalSystemMediaTransportControlsSessionManager::RequestAsync();
        if(!hwnd)co_return;
        manager=result;auto weak=weak_from_this();
        current=manager.CurrentSessionChanged([weak](auto&&,auto&&){if(auto s=weak.lock())if(s->hwnd)PostMessageW(s->hwnd,WM_APP+3,0,0);});
        bind();
    }catch(...) {std::scoped_lock lock(mutex);snapshot={L"Media unavailable",L"Windows could not connect to media sessions"};if(hwnd)PostMessageW(hwnd,WM_APP+2,0,0);}
}
void Media::bind() {
    if(session){session.MediaPropertiesChanged(properties);session.PlaybackInfoChanged(playback);}
    ++generation; session=manager?manager.GetCurrentSession():nullptr;
    if(manager)try{if(!session||session.GetPlaybackInfo().PlaybackStatus()!=GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing){for(auto candidate:manager.GetSessions())if(candidate.GetPlaybackInfo().PlaybackStatus()==GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing){session=candidate;break;}}}catch(...){}
    if(session){auto weak=weak_from_this();auto cb=[weak](auto&&,auto&&){if(auto s=weak.lock())if(s->hwnd)PostMessageW(s->hwnd,WM_APP+4,0,0);};properties=session.MediaPropertiesChanged(cb);playback=session.PlaybackInfoChanged(cb);}
    refresh();
}
fire_and_forget Media::refresh() {
    auto self=shared_from_this();auto id=++generation;
    try {
        MediaSnapshot next;
        Windows::Storage::Streams::IRandomAccessStreamReference thumbnail{nullptr};
        if(session){auto selected=session;auto p=co_await selected.TryGetMediaPropertiesAsync();if(id!=generation||!hwnd)co_return;auto info=selected.GetPlaybackInfo();auto controls=info.Controls();next.title=std::wstring(p.Title());next.artist=std::wstring(p.Artist());next.playing=info.PlaybackStatus()==GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;next.play=controls.IsPlayPauseToggleEnabled();next.previous=controls.IsPreviousEnabled();next.next=controls.IsNextEnabled();auto source=std::wstring(selected.SourceAppUserModelId());next.source=source;next.spotify=source.find(L"Spotify")!=std::wstring::npos||source.find(L"spotify")!=std::wstring::npos;thumbnail=p.Thumbnail();}
        next.appleMusic=appleMusicName(next.source)||appleMusicBrowserWindow(next.source);
        {std::scoped_lock lock(mutex);if(next.title==snapshot.title&&next.artist==snapshot.artist&&next.source==snapshot.source)next.artwork=snapshot.artwork;}
        // A player logo is not album artwork. Keep a crisp vector fallback when no cover is supplied.
        if(next.title.empty()&&!next.source.empty()){std::scoped_lock lock(mutex);if(next.source==snapshot.source&&!snapshot.title.empty()){next.title=snapshot.title;next.artist=snapshot.artist;next.artwork=snapshot.artwork;}else next.title=next.source.find(L"Apple")!=std::wstring::npos?L"Apple Music":L"Media playing";}
        bool needsArt=thumbnail&&!next.artwork;
        auto key=next.source+L"|"+next.title+L"|"+next.artist;
        bool lookup=!next.artwork&&(next.appleMusic||!mediaBrowser(next.source).empty())&&!next.artist.empty()&&(key!=lastLookupKey||GetTickCount64()-lastLookupAt>60000);
        pendingTitle=next.title;pendingArtist=next.artist;pendingSource=next.source;
        if(lookup){lastLookupKey=key;lastLookupAt=GetTickCount64();}
        {std::scoped_lock lock(mutex);snapshot=std::move(next);} if(hwnd)PostMessageW(hwnd,WM_APP+2,0,0);
        pendingGeneration=id;pendingArt=needsArt?thumbnail:nullptr;pendingLookup=lookup;if((needsArt||lookup)&&!decoding)decodeArtwork();
    }catch(...) {if(id==generation){std::scoped_lock lock(mutex);snapshot={L"Media unavailable",L"The player stopped responding"};if(hwnd)PostMessageW(hwnd,WM_APP+2,0,0);}}
}
fire_and_forget Media::decodeArtwork(){
    auto self=shared_from_this();decoding=true;apartment_context ui;
    while(hwnd&&(pendingArt||pendingLookup)){
        auto reference=pendingArt;auto id=pendingGeneration;auto title=pendingTitle,artist=pendingArtist,source=pendingSource;bool lookup=pendingLookup;pendingLookup=false;pendingArt=nullptr;
        std::shared_ptr<Artwork> art;
        co_await resume_background();
        try{
            Windows::Storage::Streams::IRandomAccessStream stream{nullptr};
            if(lookup){auto bytes=catalogueArtwork(title,artist);if(!bytes.empty()){Windows::Storage::Streams::InMemoryRandomAccessStream memory;Windows::Storage::Streams::DataWriter writer(memory);writer.WriteBytes(bytes);co_await writer.StoreAsync();writer.DetachStream();memory.Seek(0);stream=memory;}}
            if(!stream&&reference)stream=co_await reference.OpenReadAsync();
            if(!stream)throw hresult_error(E_FAIL);
            if(stream.Size()>0&&stream.Size()<=MaxArtworkEncoded){
                using namespace Windows::Graphics::Imaging;
                auto decoder=co_await BitmapDecoder::CreateAsync(stream);
                if(boundedArtwork(decoder.PixelWidth(),decoder.PixelHeight())){
                    BitmapTransform transform;transform.ScaledWidth(ArtworkEdge);transform.ScaledHeight(ArtworkEdge);transform.InterpolationMode(BitmapInterpolationMode::Fant);
                    auto data=co_await decoder.GetPixelDataAsync(BitmapPixelFormat::Bgra8,BitmapAlphaMode::Premultiplied,transform,ExifOrientationMode::IgnoreExifOrientation,ColorManagementMode::DoNotColorManage);
                    auto bytes=data.DetachPixelData();
                    if(bytes.size()==ArtworkEdge*ArtworkEdge*4){art=std::make_shared<Artwork>();art->width=art->height=ArtworkEdge;art->pixels.assign(bytes.begin(),bytes.end());}
                }
            }
        }catch(...){}
        co_await ui;
        if(hwnd&&art){{std::scoped_lock lock(mutex);if(snapshot.title==title&&snapshot.artist==artist&&snapshot.source==source)snapshot.artwork=std::move(art);}PostMessageW(hwnd,WM_APP+2,0,0);}
    }
    decoding=false;
}
fire_and_forget Media::command(int id) {
    auto self=shared_from_this();
    try {if(id==3){bind();co_return;}if(id==4){refresh();co_return;}if(!session)co_return;auto s=session;auto c=s.GetPlaybackInfo().Controls();if(id==0&&c.IsPlayPauseToggleEnabled())co_await s.TryTogglePlayPauseAsync();if(id==1&&c.IsPreviousEnabled())co_await s.TrySkipPreviousAsync();if(id==2&&c.IsNextEnabled())co_await s.TrySkipNextAsync();}catch(...){}
}
MediaSnapshot Media::get(){std::scoped_lock lock(mutex);return snapshot;}
void Media::stop(){hwnd=nullptr;++generation;try{if(manager)manager.CurrentSessionChanged(current);if(session){session.MediaPropertiesChanged(properties);session.PlaybackInfoChanged(playback);}}catch(...){}session=nullptr;manager=nullptr;}
}
