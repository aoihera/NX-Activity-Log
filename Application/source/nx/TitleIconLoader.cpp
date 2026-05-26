#include "nx/TitleIconLoader.hpp"
#include "nx/Title.hpp"

#include <cstdio>
#include <cstring>
#include <vector>
#include <deque>
#include <string>
#include <atomic>
#include <memory>
#include <switch.h>
#include <zlib.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Returns true on FW >= 20.0.0 where NsApplicationControlSource_CacheOnly
// blocks (matches sphaira's ns::IsNsControlFetchSlow()).
static inline bool isNsControlFetchSlow() {
    return hosversionAtLeast(20, 0, 0);
}

// /atmosphere/contents/<titleID>  (no trailing slash)
static void getContentsPath(u64 titleID, char* out, size_t outSize) {
    std::snprintf(out, outSize, "/atmosphere/contents/%016lX", titleID);
}

// Read a whole file from the SD card via stdio.  Returns empty on failure.
static std::vector<u8> readFile(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 2 * 1024 * 1024) { // icons should be well under 2 MiB
        fclose(f);
        return {};
    }
    std::vector<u8> data(static_cast<size_t>(sz));
    if (fread(data.data(), 1, data.size(), f) != data.size()) {
        fclose(f);
        return {};
    }
    fclose(f);
    return data;
}

// Minimal [override_nacp] name= parser.
// Looks for:
//   [override_nacp]
//   name=<value>
// Returns the value string, or empty if not found.
static std::string readOverrideName(const char* iniPath) {
    FILE* f = fopen(iniPath, "r");
    if (!f) return {};

    std::string result;
    bool inSection = false;
    char line[512];

    while (fgets(line, sizeof(line), f)) {
        // Strip trailing newline / carriage-return
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[--len] = '\0';
        }

        if (line[0] == '[') {
            inSection = (strncmp(line, "[override_nacp]", 15) == 0);
            continue;
        }

        if (inSection && strncmp(line, "name=", 5) == 0) {
            result = std::string(line + 5);
            break;
        }
    }

    fclose(f);
    return result;
}

// ---------------------------------------------------------------------------
// Core per-title fetch — runs on the background thread.
// Matches sphaira's ThreadData::Get() logic.
// ---------------------------------------------------------------------------
static void fetchIconForTitle(NX::Title* title) {
    const u64 id = title->titleID();
    std::vector<u8> icon;
    std::string name;

    // ---- Steps 1 & 2: get NACP + icon via NS ----
    bool has_nacp = false;
    auto ctrl = std::make_unique<NsApplicationControlData>();
    u64 actualSize = 0;

    // Fast path: NS in-memory cache (FW < 20 only)
    if (!isNsControlFetchSlow()) {
        if (R_SUCCEEDED(nsGetApplicationControlData(
                NsApplicationControlSource_CacheOnly,
                id, ctrl.get(), sizeof(*ctrl), &actualSize))) {
            has_nacp = true;
        }
    }

    // Slow fallback: read from storage
    if (!has_nacp) {
        if (R_SUCCEEDED(nsGetApplicationControlData(
                NsApplicationControlSource_Storage,
                id, ctrl.get(), sizeof(*ctrl), &actualSize))) {
            has_nacp = true;
        }
    }

    if (has_nacp) {
        // Extract name from NACP.
        // TitlesDataFormat==1 (FW 21+) stores lang entries as raw DEFLATE
        // (wbits=-15); decompress into a temporary buffer first.
        NacpLanguageEntry* lang = nullptr;
        NacpStruct* nacp_ptr = &ctrl->nacp;
        NacpStruct tmp;
        if (ctrl->nacp.titles_data_format == 1) {
            NacpLanguageEntry decompressed[32] = {};
            const auto& cd = ctrl->nacp.lang_data.compressed_data;
            z_stream s = {};
            s.next_in   = const_cast<Bytef*>(cd.buffer);
            s.avail_in  = cd.buffer_size;
            s.next_out  = reinterpret_cast<Bytef*>(decompressed);
            s.avail_out = sizeof(decompressed);
            if (inflateInit2(&s, -15) == Z_OK) {
                if (inflate(&s, Z_FINISH) == Z_STREAM_END) {
                    tmp = ctrl->nacp;
                    tmp.titles_data_format = 0;
                    std::memcpy(tmp.lang_data.lang, decompressed,
                                std::min<uLong>(s.total_out, sizeof(tmp.lang_data.lang)));
                    nacp_ptr = &tmp;
                }
                inflateEnd(&s);
            }
        }
        if (R_SUCCEEDED(nsGetApplicationDesiredLanguage(nacp_ptr, &lang)) && lang)
            name = std::string(lang->name);

        // Extract icon JPEG
        if (actualSize > sizeof(NacpStruct)) {
            const u64 jpegSize = actualSize - sizeof(NacpStruct);
            if (jpegSize > 0 && jpegSize <= sizeof(ctrl->icon)) {
                icon.resize(jpegSize);
                std::memcpy(icon.data(), ctrl->icon, jpegSize);
            }
        }
    }

    // ---- Step 3: atmosphere / sys-tweak overrides ----
    // Check /atmosphere/contents/<id>/ for icon.jpg and config.ini.
    // Applied regardless of whether NS succeeded — matches sphaira exactly.
    char contentsPath[64];
    getContentsPath(id, contentsPath, sizeof(contentsPath));

    char iconOverridePath[80];
    std::snprintf(iconOverridePath, sizeof(iconOverridePath), "%s/icon.jpg", contentsPath);
    auto overrideIcon = readFile(iconOverridePath);
    if (!overrideIcon.empty() && overrideIcon.size() < sizeof(NsApplicationControlData::icon)) {
        icon = std::move(overrideIcon);
    }

    char iniOverridePath[80];
    std::snprintf(iniOverridePath, sizeof(iniOverridePath), "%s/config.ini", contentsPath);
    std::string overrideName = readOverrideName(iniOverridePath);
    if (!overrideName.empty()) {
        name = overrideName;
    }

    // ---- Commit results to the Title object ----
    if (!icon.empty()) {
        title->setName(name);
        title->setIconData(std::move(icon));
        title->setIconStatus(NX::IconLoadStatus::Loaded);
    } else if (has_nacp) {
        // Got the name but no icon (unusual). Store name, show fallback icon.
        title->setName(name);
        title->setIconStatus(NX::IconLoadStatus::Error);
    } else {
        // Complete failure — leave name empty, show fallback icon.
        title->setIconStatus(NX::IconLoadStatus::Error);
    }
}

// ---------------------------------------------------------------------------
// Background thread state
// ---------------------------------------------------------------------------
namespace {
    Mutex            g_mutex;
    UEvent           g_uevent;
    Thread           g_thread;
    std::atomic_bool g_running{false};

    // Queue protected by g_mutex
    std::deque<NX::Title*> g_queue;

    void threadFunc(void* /*arg*/) {
        const auto waiter = waiterForUEvent(&g_uevent);

        while (g_running.load(std::memory_order_acquire)) {
            // Block until work arrives or 3 s timeout.
            waitSingle(waiter, 3000000000LL);

            if (!g_running.load(std::memory_order_acquire)) break;

            while (true) {
                NX::Title* title = nullptr;

                mutexLock(&g_mutex);
                if (!g_queue.empty()) {
                    title = g_queue.front();
                    g_queue.pop_front();
                }
                mutexUnlock(&g_mutex);

                if (!title) break;

                fetchIconForTitle(title);

                // Brief sleep so we don't starve the UI / storage bus.
                svcSleepThread(2000000LL); // 2 ms
            }
        }
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
namespace TitleIconLoader {

    void init() {
        mutexInit(&g_mutex);
        ueventCreate(&g_uevent, true);
        g_running = true;
        threadCreate(&g_thread, threadFunc, nullptr, nullptr, 0x40000, 0x2C, -2);
        threadStart(&g_thread);
    }

    void exit() {
        g_running = false;
        ueventSignal(&g_uevent);
        threadWaitForExit(&g_thread);
        threadClose(&g_thread);
    }

    void push(NX::Title* title) {
        if (!title) return;
        if (title->iconStatus() != NX::IconLoadStatus::None) return;

        // Mark queued immediately to prevent double-queuing from the UI thread.
        title->setIconStatus(NX::IconLoadStatus::Queued);

        mutexLock(&g_mutex);
        g_queue.push_back(title);
        mutexUnlock(&g_mutex);
        ueventSignal(&g_uevent);
    }

    void pushBatch(const std::vector<NX::Title*>& titles) {
        bool added = false;

        mutexLock(&g_mutex);
        for (auto* t : titles) {
            if (!t) continue;
            if (t->iconStatus() != NX::IconLoadStatus::None) continue;
            t->setIconStatus(NX::IconLoadStatus::Queued);
            g_queue.push_back(t);
            added = true;
        }
        mutexUnlock(&g_mutex);

        if (added) ueventSignal(&g_uevent);
    }

    void clearCache() {
        // Drain the queue under the lock, collecting titles that were marked
        // Queued but not yet processed, then reset their status back to None
        // so they can be re-queued by pushBatch() on the next load.
        std::deque<NX::Title*> drained;
        mutexLock(&g_mutex);
        drained.swap(g_queue);
        mutexUnlock(&g_mutex);

        for (auto* t : drained) {
            if (t && t->iconStatus() == NX::IconLoadStatus::Queued) {
                t->setIconStatus(NX::IconLoadStatus::None);
            }
        }
    }

} // namespace TitleIconLoader
