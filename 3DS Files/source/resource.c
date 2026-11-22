// resource -- see resource.h.

#include "resource.h"

#include <stdio.h>
#include <string.h>

#include "bchunk.h"
#include "bchunkload.h"
#include "bfile.h"
#include "bmem.h"
#include "eview.h"
#include "logwriter.h"
#include "lzc.h"
#include "queuedfile.h"

#define COMP_MAGIC        0x504d4f43u   // "COMP"
#define RF_PRIORITY       5
#define RF_BLOCK_SIZE     0x02000000

int32_t HotChunkFilesNeedUpdating;
bNode ResourceFileList;
int32_t NumResourcesBeingLoaded;

static void unlink_file(ResourceFile *rf)
{
    bNode *next = rf->Node.next;
    bNode *prev = rf->Node.prev;

    prev->next = next;
    next->prev = prev;
}

// bStrCpy as far as the field holds, with nothing after the terminator.
static void copy_name(char *dst, size_t cap, const char *src)
{
    size_t i;

    for (i = 0; i + 1 < cap && src[i] != '\0'; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

// The image's bStrNCpy (0x002535a0): up to n bytes, stopping after the
// terminator, and no terminator when n runs out first.
static void b_strncpy(char *dst, const char *src, int32_t n)
{
    int32_t i;

    for (i = 0; i < n; i++) {
        dst[i] = src[i];
        if (src[i] == '\0')
            break;
    }
}

// AllocateMemory (0x00199408). bMalloc's 0x40 is a 128-byte boundary, as
// LoadChunks needs, and 8 is the top of the pool.
static void allocate_memory(ResourceFile *rf, int32_t top)
{
    int pool = (rf->Flags & RESOURCE_FILE_FLAG_USE_TEMPORARY_MEMORY) ? 72 : 64;

    if (top)
        pool |= 8;
    rf->pFirstChunk = (uint8_t *)bMalloc(rf->SizeofChunks, rf->Filename, 0,
                                         pool);
}

// FileTransferCallback (0x001995d0).
static void file_transfer_callback(intptr_t param, int32_t error)
{
    (void)error;
    ((ResourceFile *)param)->FileTransfersInProgress--;
}

// ResourceFile(const char *, ResourceFileType, int, void (*)(int), int),
// 0x00199320.
static ResourceFile *construct(ResourceFile *rf, const char *name,
                               int32_t type, int32_t flags,
                               ResourceFileCallback cb, intptr_t param)
{
    rf->Type = type;
    rf->Flags = flags;
    copy_name(rf->Filename, sizeof rf->Filename, name);
    rf->Callback = cb;
    rf->CallbackParam = param;
    rf->FileTransfersInProgress = 0;
    rf->LoadingFinishedFlag = 0;
    rf->HotFileNumber = 0;
    rf->pLoadedHotFileEntries = NULL;
    rf->NumLoadedHotFileEntries = 0;
    rf->InventoryHandle = 0;
    rf->HotFilename[0] = '\0';
    rf->SizeofChunks = bFileSizeName(rf->Filename);
    if (rf->SizeofChunks < 0)
        rf->SizeofChunks = 0;
    rf->pFirstChunk = NULL;
    if (rf->SizeofChunks != 0) {
        // The top for the packed copy, freed once expanded. The card holds
        // the tracks' TRACKB<n>.LZC expanded (bfile.c), and what is read is
        // the bundle itself, kept for the race: not the top's, which on the
        // 3DS is the linear heap the track's textures go to (Session 109).
        int32_t handle;

        allocate_memory(rf, (rf->Flags & RESOURCE_FILE_FLAG_LZCOMPRESSED)
                                && !bFileIsExpanded(rf->Filename));
        rf->FileTransfersInProgress++;
        handle = add_queued_file(rf->pFirstChunk, rf->Filename, 0,
                                 rf->SizeofChunks, file_transfer_callback,
                                 (intptr_t)rf);
        if (handle < 0) {
            // The file queue is full: nothing was queued, so the
            // transfer can never finish. Drop the count again, and
            // the resource loads empty (as a missing file does),
            // instead of waiting forever. R36S v42.
            rf->FileTransfersInProgress--;
        } else {
            change_queued_file_params(handle, RF_PRIORITY, RF_BLOCK_SIZE);
        }
    }
    return rf;
}

// ~ResourceFile (0x00199458), and the delete after it.
static void delete_file(ResourceFile *rf)
{
    if (gEWaitUntilRenderingDoneFn != NULL)
        gEWaitUntilRenderingDoneFn();
    if (rf->pLoadedHotFileEntries != NULL) {
        bFree(rf->pLoadedHotFileEntries);
        rf->NumLoadedHotFileEntries = 0;
        rf->pLoadedHotFileEntries = NULL;
        HotChunkFilesNeedUpdating++;
    }
    if (rf->pFirstChunk != NULL) {
        /* R36S v18 DIAG: v17 log proved the crash is after
           unload_chunks returns and before "res deleted" — one of
           bFree(pFirstChunk), post_load_fixup, bFree(rf). Step DIAGs
           show which two lines the crash falls between. */
        unload_chunks(rf->pFirstChunk, rf->SizeofChunks);
        logwriter_put("DIAG unload: delete_file after unload_chunks\n",
                      sizeof("DIAG unload: delete_file after unload_chunks\n") - 1);
        logwriter_put("DIAG unload: delete_file before bFree chunks\n",
                      sizeof("DIAG unload: delete_file before bFree chunks\n") - 1);
        bFree(rf->pFirstChunk);
        logwriter_put("DIAG unload: delete_file after bFree chunks\n",
                      sizeof("DIAG unload: delete_file after bFree chunks\n") - 1);
        rf->pFirstChunk = NULL;
        logwriter_put("DIAG unload: delete_file before fixup\n",
                      sizeof("DIAG unload: delete_file before fixup\n") - 1);
#ifdef NFSHP2_R36S
        /* R36S v19: SKIP post_load_fixup on unload. v18 log proved the
           race-exit segfault is inside post_load_fixup after
           efix_up_tables returns, in el_update_textures or
           afx_update_textures: both walk lists whose nodes point into
           just-freed chunk memory (EmitterDataList nodes ARE in-chunk
           AcidEmitterData; TexturePackList packs' TextureInfo arrays
           ARE in-chunk; d->Texture dangles once the pack's file is
           freed), and efix_up_tables itself is disabled on unload
           ("fixup disabled, done"). The same re-resolution runs again
           on the next load (load_if_transfer_finished, frontres.c,
           trackview.c all call post_load_fixup after loading), so
           skipping it here loses no work. Load path untouched. */
        logwriter_put("DIAG unload: delete_file fixup skipped (unload)\n",
                      sizeof("DIAG unload: delete_file fixup skipped (unload)\n") - 1);
#else
        post_load_fixup();
#endif
        logwriter_put("DIAG unload: delete_file after fixup\n",
                      sizeof("DIAG unload: delete_file after fixup\n") - 1);
    }
    logwriter_put("DIAG unload: delete_file before bFree rf\n",
                  sizeof("DIAG unload: delete_file before bFree rf\n") - 1);
    bFree(rf);
}

// LoadResourceIfFileTransferFinished (0x001994f0).
static void load_if_transfer_finished(ResourceFile *rf)
{
    if (rf->LoadingFinishedFlag || rf->FileTransfersInProgress != 0)
        return;
    // R36S v43 DIAG: only car geometry files are traced (see resLoad).
    if (strncmp(rf->Filename, "CARS", 4) == 0) {
        char buf[128];
        int n = snprintf(buf, sizeof buf,
                         "DIAG load: transfer done %s chunks=%d\n",
                         rf->Filename, (int)rf->SizeofChunks);
        if (n > 0)
            logwriter_put(buf, (size_t)n);
        logwriter_drain();
    }
    if (rf->Flags & RESOURCE_FILE_FLAG_LZCOMPRESSED) {
        uint8_t *packed = rf->pFirstChunk;

        if (packed != NULL && bchunk_u32(packed) == COMP_MAGIC) {
            int32_t packed_size = rf->SizeofChunks;

            rf->pFirstChunk = NULL;
            rf->SizeofChunks = (int32_t)bchunk_u32(packed + 8);
            if (rf->SizeofChunks != 0) {
                // R36S v43 DIAG: LZC expand entry, drained (car files).
                if (strncmp(rf->Filename, "CARS", 4) == 0) {
                    char buf[128];
                    int n = snprintf(buf, sizeof buf,
                                     "DIAG load: lzc expand %s packed=%d expanded=%d\n",
                                     rf->Filename, (int)packed_size,
                                     (int)rf->SizeofChunks);
                    if (n > 0)
                        logwriter_put(buf, (size_t)n);
                    logwriter_drain();
                }
                allocate_memory(rf, 0);
                (void)lzc_decompress_into(packed, (size_t)packed_size, 0,
                                          rf->pFirstChunk,
                                          (size_t)rf->SizeofChunks, NULL);
                if (strncmp(rf->Filename, "CARS", 4) == 0) {
                    logwriter_put("DIAG load: lzc done\n",
                                  sizeof("DIAG load: lzc done\n") - 1);
                    logwriter_drain();
                }
            }
            bFree(packed);
        }
    }
    // R36S v43 DIAG: chunk walk entry/exit, drained (car files).
    if (strncmp(rf->Filename, "CARS", 4) == 0) {
        char buf[128];
        int n = snprintf(buf, sizeof buf,
                         "DIAG load: load_chunks %s size=%d\n",
                         rf->Filename, (int)rf->SizeofChunks);
        if (n > 0)
            logwriter_put(buf, (size_t)n);
        logwriter_drain();
    }
    load_chunks(rf->pFirstChunk, rf->SizeofChunks);
    if (strncmp(rf->Filename, "CARS", 4) == 0) {
        logwriter_put("DIAG load: load_chunks done\n",
                      sizeof("DIAG load: load_chunks done\n") - 1);
        logwriter_drain();
    }
    if (rf->Flags & RESOURCE_FILE_FLAG_FREE_AFTER_LOADING) {
        if (rf->pFirstChunk != NULL)
            bFree(rf->pFirstChunk);
        rf->pFirstChunk = NULL;
        rf->SizeofChunks = 0;
    }
    post_load_fixup();
    rf->LoadingFinishedFlag = 1;
}

void resource_static_init(void)
{
    ResourceFileList.prev = &ResourceFileList;
    ResourceFileList.next = &ResourceFileList;
}

void resource_static_destruct(void)
{
    while (ResourceFileList.next != &ResourceFileList) {
        ResourceFile *rf = (ResourceFile *)ResourceFileList.next;

        unlink_file(rf);
        delete_file(rf);
    }
}

ResourceFile *load_resource_file(const char *name, int32_t type,
                                 int32_t flags, ResourceFileCallback cb,
                                 intptr_t param)
{
    ResourceFile *rf;
    bNode *last;

    NumResourcesBeingLoaded++;
    rf = construct((ResourceFile *)bMalloc((int)sizeof(ResourceFile),
                                           "ResourceFile", 0, 16),
                   name, type, flags, cb, param);
    // R36S v43 DIAG: a car geometry file's load entry, drained. Gated
    // to CARS\ so every other resource load (tracks, frontend) is
    // untouched. If the fault is in the file open / queue, this is the
    // last line; the matching "lzc"/"load_chunks" lines never come.
    if (strncmp(name, "CARS", 4) == 0) {
        char buf[128];
        int n = snprintf(buf, sizeof buf,
                         "DIAG load: resLoad %s size=%d\n",
                         name, (int)rf->SizeofChunks);
        if (n > 0)
            logwriter_put(buf, (size_t)n);
        logwriter_drain();
    }
    last = ResourceFileList.prev;
    last->next = &rf->Node;
    ResourceFileList.prev = &rf->Node;
    rf->Node.prev = last;
    rf->Node.next = &ResourceFileList;
    return rf;
}

ResourceFile *load_resource_file_hot(const char *name, int32_t type,
                                     int32_t hot_chunkable, int32_t keep,
                                     ResourceFileCallback cb,
                                     intptr_t param)
{
    int32_t flags = hot_chunkable != 0;

    if (!keep)
        flags |= RESOURCE_FILE_FLAG_USE_TEMPORARY_MEMORY
                 | RESOURCE_FILE_FLAG_FREE_AFTER_LOADING;
    return load_resource_file(name, type, flags, cb, param);
}

void change_filename_for_hot_chunking(ResourceFile *rf, const char *name)
{
    b_strncpy(rf->Filename, name, 79);
}

void unload_resource_file(ResourceFile *rf)
{
    /* DIAG v12: race-exit segfault adimi — hangi alt adimda cokuyor? */
    char diagbuf[128];
    int n = snprintf(diagbuf, sizeof(diagbuf),
                     "DIAG unload: resUnload %s\n", rf->Filename);
    if (n > 0)
        logwriter_put(diagbuf, (size_t)n);
    while (!rf->LoadingFinishedFlag)
        service_resource_loading();
    unlink_file(rf);
    logwriter_put("DIAG unload: res unlinked\n",
                  sizeof("DIAG unload: res unlinked\n") - 1);
    delete_file(rf);
    logwriter_put("DIAG unload: res deleted\n",
                  sizeof("DIAG unload: res deleted\n") - 1);
}

int32_t service_resource_loading(void)
{
    bNode *n;

    service_queued_files();
    if (NumResourcesBeingLoaded == 0)
        return 0;
    for (n = ResourceFileList.next; n != &ResourceFileList; n = n->next) {
        ResourceFile *rf = (ResourceFile *)n;

        if (rf->LoadingFinishedFlag)
            continue;
        load_if_transfer_finished(rf);
        if (!rf->LoadingFinishedFlag)
            return NumResourcesBeingLoaded;
        NumResourcesBeingLoaded--;
        if (rf->Callback != NULL)
            rf->Callback(rf->CallbackParam);
        return NumResourcesBeingLoaded;
    }
    return 0;
}

int32_t is_resource_loading_complete(void)
{
    return NumResourcesBeingLoaded == 0;
}

ResourceFile *find_resource_file(const char *name)
{
    bNode *n;

    for (n = ResourceFileList.prev; n != &ResourceFileList; n = n->prev) {
        if (strcmp(((ResourceFile *)n)->Filename, name) == 0)
            return (ResourceFile *)n;
    }
    return NULL;
}

ResourceFile *find_resource_file_by_type(int32_t type)
{
    bNode *n;

    for (n = ResourceFileList.prev; n != &ResourceFileList; n = n->prev) {
        if (((ResourceFile *)n)->Type == type)
            return (ResourceFile *)n;
    }
    return NULL;
}

int32_t resource_file_get_size(ResourceFile *rf, uint32_t id,
                               int32_t *count)
{
    int32_t  total = 0;
    int32_t  n = 0;
    uint8_t *c = rf->pFirstChunk;

    if (c != NULL) {
        uint8_t *end = c + rf->SizeofChunks;

        while (c != end) {
            uint32_t size = bchunk_u32(c + 4);

            if (id == 0 || bchunk_u32(c) == id) {
                n++;
                total += (int32_t)size;
            }
            c += size + 8;
        }
    }
    if (count != NULL)
        *count = n;
    return total;
}

int32_t get_resource_file_size(int32_t type, uint32_t id, int32_t *count)
{
    int32_t total = 0;
    int32_t chunks = 0;
    bNode  *n;

    for (n = ResourceFileList.prev; n != &ResourceFileList; n = n->prev) {
        ResourceFile *rf = (ResourceFile *)n;
        int32_t       k;

        if (rf->Type != type)
            continue;
        total += resource_file_get_size(rf, id, &k);
        chunks += k;
    }
    if (count != NULL)
        *count = chunks;
    return total;
}
