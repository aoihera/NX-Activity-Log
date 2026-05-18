#pragma once

// TitleIconLoader
// ===============
// Asynchronous, prioritised icon / NACP fetcher for NX-Activity-Log.
//
// Design is adapted from sphaira's title_info system:
//   - A single long-lived background thread processes a queue of titleIDs.
//   - It uses NsApplicationControlSource_CacheOnly on FW < 20 (fast path),
//     falls back to NsApplicationControlSource_Storage (slow but always works).
//   - Icons are stored directly in the NX::Title objects so the Aether Image
//     elements can pull them out without extra allocation.
//   - The "sys-tweak / atmosphere override" path is also supported: if
//     /atmosphere/contents/<titleID>/icon.jpg exists it replaces the system
//     icon, exactly as sphaira does.
//
// Usage
// -----
//   // At startup (after nsInitialize / romfsInit):
//   TitleIconLoader::init();
//
//   // Queue all titles for loading:
//   TitleIconLoader::pushBatch(app.titleVector());
//
//   // At shutdown:
//   TitleIconLoader::exit();

#include "nx/Title.hpp"
#include <vector>
#include <switch.h>

namespace TitleIconLoader {

    // Start the background loader thread.
    // Call once after nsInitialize().
    void init();

    // Stop the background thread and join it.
    // Call before nsExit().
    void exit();

    // Queue a single title for icon + name loading.
    // Safe to call from any thread; no-ops if the title is already queued/loaded.
    void push(NX::Title* title);

    // Convenience: queue every title in the vector.
    void pushBatch(const std::vector<NX::Title*>& titles);

    // Drain the pending queue and reset any Queued titles back to None so they
    // can be re-queued.  Call before re-loading the title list if needed.
    void clearCache();

} // namespace TitleIconLoader
