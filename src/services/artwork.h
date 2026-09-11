#pragma once
#include <cstdint>
#include <vector>
#include <memory>
namespace delight {
struct Artwork {unsigned width=0,height=0;std::vector<std::uint8_t> pixels;};
constexpr unsigned ArtworkEdge=128;
constexpr std::uint64_t MaxArtworkEncoded=4*1024*1024;
inline bool boundedArtwork(unsigned w,unsigned h){return w>0&&h>0&&w<=2048&&h<=2048;}
}
