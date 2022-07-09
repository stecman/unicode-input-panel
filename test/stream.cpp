#include <freetype2/ft2build.h>
#include FT_CACHE_H
#include FT_FREETYPE_H

#include "stdio.h"
#include "stdint.h"

#include "font_indexer.hh"

#include <filesystem>
#include <iostream>
#include <iterator>
#include <algorithm>
#include <limits>
#include <map>
#include <vector>

namespace fs = std::filesystem;

#define FONTS_MAX 255

static FT_StreamRec _streams[FONTS_MAX];
static uint _next_slot = 0;

FTC_Manager m_ftc_manager;
FTC_CMapCache m_ftc_cmap_cache;
FTC_SBitCache m_ftc_sbit_cache;
FT_Library m_ft_library;

static unsigned long _read_stream(FT_Stream stream,
                                  unsigned long offset,
                                  unsigned char* buffer,
                                  unsigned long count)
{
    FILE* fp = (FILE*) stream->descriptor.pointer;

    // FreeType can call this function with a count of zero to seek only
    if (count == 0) {
    	fseek(fp, offset, SEEK_SET);
    	return 0;
    } else {
        return fread(buffer, 1, count, fp);
    }
}

static void _close_stream(FT_Stream stream)
{
    FILE* fp = (FILE*) stream->descriptor.pointer;
    fclose(fp);
}

FT_Face loadFont(const char* path, const int faceIndex)
{
    const int slot = _next_slot++;

    FT_Stream stream = &_streams[slot];
    FILE* fp = fopen(path, "r");

    fseek(fp, 0L, SEEK_END);
    size_t sz = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    stream->size = sz;
    // stream->pos = 0;
    stream->descriptor.pointer = fp;
    stream->read = &_read_stream;
    stream->close = &_close_stream;

    FT_Open_Args args = {
        .flags = FT_OPEN_STREAM,
        .stream = stream,
    };

    FT_Face face;
    FT_Error error = FT_Open_Face(m_ft_library, &args, faceIndex, &face);
    if (error) {
        printf("ERROR (%s): FT_Done_FreeType error: 0x%02X\n", __func__, error);
        return NULL;
    }

    return face;
}

int count = 0;
int count_fonts = 0;
int count_ranges = 0;

static std::map<uint32_t, uint8_t> charmap;

int main(int argc, const char* argv[])
{
	if (argc < 2) {
		printf("Not enough arguments!\n");
		return 1;
	}

    printf("Initialising freetype...\n");
    FT_Error error = FT_Init_FreeType(&m_ft_library);
    if (error) {
        printf("FATAL (%s): FT_Init_FreeType error: 0x%02X\n", __func__, error);
        return 1;
    }

    FontIndexer indexer;

    std::vector<std::string> paths;

    // Load all fonts
    for (int i = 1; i < argc; ++i) {
        const int slot = _next_slot;

        int index = 0;
        int end = 0;
        while (index <= end) {
            FT_Face face = loadFont(argv[i], index++);
            if (face == NULL) {
                break;
            }

            if (index == 0) {
                end = face->num_faces;
            }

            indexer.indexFace(slot, face);
            FT_Done_Face(face);

            paths.emplace_back(fs::path(argv[i]).filename());
            count_fonts++;
        }
    }

    printf("Found %d unique codepoints in %d fonts\n", indexer.countCodepoints(), count_fonts);

    const int i = 0x1F604;
    const uint32_t cp = indexer.find(i);

    if (cp == FontIndexer::kCodepointNotFound) {
        std::cout << i << " -> " << "(not found)" << std::endl;
    } else {
        std::cout << i << " -> " << paths.at(cp) << std::endl;
    }

    // for (const auto& [key, value] : charmap) {
    //     std::cout << key << " => " << std::string(fs::path( argv[value+1] ).filename()) << std::endl;
    // }
    //

    // for (const CodepointRange &range : range_store) {
    //     std::cout << range.start << "-" << range.end << " => " << std::string(fs::path( argv[range.slot+1] ).filename()) << std::endl;
    // }

    // const CodepointRange* first = nullptr;
    // const CodepointRange* previous = nullptr;
    // for (const CodepointRange &range : range_store) {
    //     if (first == nullptr) {
    //         first = &range;
    //         previous = &range;
    //         continue;
    //     }

    //     if (range.slot != first->slot) {
    //         printf("%d-%d => ", first->start, previous->end);
    //         std::cout << std::string(fs::path( argv[first->slot + 1] ).filename()) << std::endl;
    //     }

    //     previous = &range;
    // }

    // printf("%d-%d => ", first->start, previous->end);
    // std::cout << std::string(fs::path( argv[first->slot + 1] ).filename()) << std::endl;

	return 0;
}