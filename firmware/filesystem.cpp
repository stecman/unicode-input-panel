#include "filesystem.hh"

// FatFS
#include "f_util.h"
#include "ff.h"
#include "hw_config.h"

// C
#include "string.h"


// FreeType custom stream handlers
static unsigned long _read_stream(FT_Stream stream,
                                  unsigned long offset,
                                  unsigned char* buffer,
                                  unsigned long count)
{
    FRESULT fr;
    FIL* fp = (FIL*) stream->descriptor.pointer;

    // FreeType normally does a seek (count=0) followed by a separate read at the same
    // offset, however a few functions use FT_STREAM_READ_AT which expects a combined seek
    // and read, so we need to handle seek in both cases.
    if (f_tell(fp) != offset) {
        fr = f_lseek(fp, offset);
        if (fr != FR_OK) {
            printf("f_lseek to %lu failed on slot %d: %s (%d)\n",
                offset, stream->descriptor.value, FRESULT_str(fr), fr);
        }
    }

    if (count == 0) {
        // Seek only. Return value is treated as an error code (non-zero is failure)
        return fr;
    }

    uint bytes_read = 0;

    fr = f_read(fp, buffer, count, &bytes_read);
    if (fr != FR_OK) {
        printf("f_read of %lu bytes at %lu failed on slot %d: %s (%d)\n",
            count, offset, stream->descriptor.value, FRESULT_str(fr), fr);
    }

    return bytes_read;
}

static void _close_stream(FT_Stream stream)
{
    FIL* fp = (FIL*) stream->descriptor.pointer;

    FRESULT fr = f_close(fp);
    if (fr != FR_OK) {
        printf("f_close error on slot %d: %s (%d)\n", stream->descriptor.value, FRESULT_str(fr), fr);
    }

    delete fp;
    delete stream;
}


namespace fs {

int mount()
{
    sd_card_t* sdcard = sd_get_by_num(0);

    FATFS* fs = &sdcard->fatfs;
    if (!fs) {
        printf("Unknown logical drive number: \"%s\"\n", 0);
        return FR_INVALID_PARAMETER;
    }

    FRESULT fr = f_mount(fs, sdcard->pcName, 1);
    if (FR_OK != fr) {
        printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        return fr;
    }

    sdcard->mounted = true;

    return fr;
}

void walkdir(const char* dirpath, const std::function<void(const char* abspath, uint8_t progress)> &callback)
{
    uint total = 0;
    uint current = 0;

    FRESULT fr;
    DIR dir;
    FILINFO info;

    fr = f_opendir(&dir, dirpath);
    if (fr != FR_OK) {
        printf("No directory /%s to open: %s (%d)\n", dirpath, FRESULT_str(fr), fr);
        // TODO: UI error display
        return;
    }

    // Count files to allow for progress display
    while (f_readdir(&dir, &info) == FR_OK && info.fname[0] != '\0') {
        if (info.fattrib & AM_DIR) {
            // Skip directories
            continue;
        }

        total++;
    }

    f_rewinddir(&dir);

    // Actually process files
    while (f_readdir(&dir, &info) == FR_OK && info.fname[0] != '\0') {
        if (info.fattrib & AM_DIR) {
            // Skip directories
            continue;
        }

        // Build path with directory and filename
        char fontpath[256];
        sprintf((char*) &fontpath, "%s/%s", dirpath, info.fname);

        // Process the file
        callback((char*) &fontpath, fp_progress(++current, total));
    }

    f_closedir(&dir);
}

FT_Error load_face(const char* path, FT_Library library, FT_Face* face)
{
    printf("== Loading font %s ==\n", path);

	FIL* fp = new FIL;
    FT_Stream stream = new FT_StreamRec;

    FRESULT fr = f_open(fp, path, FA_READ);
    if (fr != FR_OK) {
        printf("f_read error: %s (%d)\n", FRESULT_str(fr), fr);

        delete fp;
        delete stream;

        return FT_Err_Cannot_Open_Resource;
    }

    stream->base = NULL;
    stream->size = f_size(fp);
    stream->pos = 0;
    stream->descriptor.pointer = fp;
    stream->pathname.pointer = (char*) path;
    stream->read = &_read_stream;
    stream->close = &_close_stream;
    stream->cursor = NULL;
    stream->limit = NULL;

    FT_Open_Args args = {
        .flags = FT_OPEN_STREAM,
        .memory_base = NULL,
        .memory_size = 0,
        .pathname = NULL,
        .stream = stream,
        .driver = NULL,
        .num_params = 0,
        .params = NULL,
    };

    return FT_Open_Face(library, &args, 0, face);
}

}; // namespace fs