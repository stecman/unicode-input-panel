#include "filesystem.hh"

#include <string>
#include <filesystem>


#include <stdlib.h>

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

    delete stream;
}


namespace fs {

int mount()
{
    // No action required
    return 0;
}

void walkdir(const char* dirpath, const std::function<void(const char* abspath, uint8_t progress)> &callback)
{
    uint total = 0;
    uint current = 0;

    // Count for calculating progress
    for (const auto &entry : std::filesystem::directory_iterator(dirpath)) {
        if (entry.is_regular_file()) {
            total++;
        }
    }

    // Process files
    for (const auto & entry : std::filesystem::directory_iterator(dirpath)) {
        if (entry.is_regular_file()) {
            callback(entry.path().c_str(), fp_progress(++current, total));
        }
    }
}

FT_Error load_face(const char* path, FT_Library library, FT_Face* face)
{
    printf("== Loading font %s ==\n", path);

    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        printf("Failed to open file %s\n", path);
        return FT_Err_Cannot_Open_Resource;
    }

    // Calculate size
    fseek(fp, 0L, SEEK_END);
    size_t sz = ftell(fp);
    fseek(fp, 0L, SEEK_SET);

    FT_Stream stream = new FT_StreamRec;
    stream->size = sz;
    stream->descriptor.pointer = fp;
    stream->read = &_read_stream;
    stream->close = &_close_stream;

    FT_Open_Args args;
    args.flags = FT_OPEN_STREAM;
    args.stream = stream;

    return FT_Open_Face(library, &args, 0, face);
}

}; // namespace fs
