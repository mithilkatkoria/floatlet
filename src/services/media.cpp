#include "media.h"
#include <winrt/Windows.Graphics.Imaging.h>
using namespace winrt;
using namespace Windows::Media::Control;
namespace delight {
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
    if(session){auto weak=weak_from_this();auto cb=[weak](auto&&,auto&&){if(auto s=weak.lock())if(s->hwnd)PostMessageW(s->hwnd,WM_APP+4,0,0);};properties=session.MediaPropertiesChanged(cb);playback=session.PlaybackInfoChanged(cb);}
    refresh();
}
fire_and_forget Media::refresh() {
    auto self=shared_from_this();auto id=++generation;
    try {
        MediaSnapshot next;
        Windows::Storage::Streams::IRandomAccessStreamReference thumbnail{nullptr};
        if(session){auto selected=session;auto p=co_await selected.TryGetMediaPropertiesAsync();if(id!=generation||!hwnd)co_return;auto info=selected.GetPlaybackInfo();auto controls=info.Controls();next.title=std::wstring(p.Title());next.artist=std::wstring(p.Artist());next.playing=info.PlaybackStatus()==GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;next.play=controls.IsPlayPauseToggleEnabled();next.previous=controls.IsPreviousEnabled();next.next=controls.IsNextEnabled();auto source=std::wstring(selected.SourceAppUserModelId());next.spotify=source.find(L"Spotify")!=std::wstring::npos||source.find(L"spotify")!=std::wstring::npos;thumbnail=p.Thumbnail();}
        {std::scoped_lock lock(mutex);if(next.title==snapshot.title&&next.artist==snapshot.artist&&next.spotify==snapshot.spotify)next.artwork=snapshot.artwork;}
        bool needsArt=thumbnail&&!next.artwork;
        {std::scoped_lock lock(mutex);snapshot=std::move(next);} if(hwnd)PostMessageW(hwnd,WM_APP+2,0,0);
        pendingGeneration=id;pendingArt=needsArt?thumbnail:nullptr;if(needsArt&&!decoding)decodeArtwork();
    }catch(...) {if(id==generation){std::scoped_lock lock(mutex);snapshot={L"Media unavailable",L"The player stopped responding"};if(hwnd)PostMessageW(hwnd,WM_APP+2,0,0);}}
}
fire_and_forget Media::decodeArtwork(){
    auto self=shared_from_this();decoding=true;apartment_context ui;
    while(hwnd&&pendingArt){
        auto reference=pendingArt;auto id=pendingGeneration;pendingArt=nullptr;
        std::shared_ptr<Artwork> art;
        co_await resume_background();
        try{
            auto stream=co_await reference.OpenReadAsync();
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
        if(hwnd&&id==generation&&art){{std::scoped_lock lock(mutex);snapshot.artwork=std::move(art);}PostMessageW(hwnd,WM_APP+2,0,0);}
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
