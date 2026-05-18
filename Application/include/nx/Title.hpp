#ifndef TITLE_HPP
#define TITLE_HPP

#include "Types.hpp"
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace NX {

    // Status of icon / nacp loading for a Title.
    enum class IconLoadStatus {
        None,       // Not yet queued
        Queued,     // Pushed to background thread, waiting
        Loaded,     // Icon data is ready in iconData
        Error,      // Failed; iconData will be empty (use no_icon fallback)
    };

    class Title {
        private:
            TitleID titleID_;
            bool is_installed;

            // name_ is written by the background loader thread and read by the
            // UI thread.  Protect it with a mutex so reads/writes don't race.
            mutable std::mutex nameMutex_;
            std::string name_;

            // Icon JPEG bytes – filled by the background loader.
            // Freed after the GPU texture has been uploaded to reclaim RAM.
            std::vector<u8> iconData_;
            std::atomic<IconLoadStatus> iconStatus_;

        public:
            // Construct from a known titleID (installed or not).
            // Name + icon are populated later via the async loader.
            Title(TitleID titleID, bool installed);

            // Construct an "uninstalled / missing" title with a known name
            // and no icon (will display the no_icon fallback).
            Title(const TitleID titleID, const std::string & name);

            // ----------------------------------------------------------------
            // Accessors
            // ----------------------------------------------------------------
            TitleID titleID() const;
            bool    isInstalled() const;
            std::string name() const;

            // Raw JPEG bytes for the icon (may be empty before load completes).
            const std::vector<u8>& iconData() const;

            // For backwards-compat with Aether::Image(ptr, size, ...) call sites.
            // Returns nullptr / 0 until the icon is loaded.
            u8 *  imgPtr();
            u32   imgSize();

            // Current load state – checked by the UI to decide when to upload
            // the texture.
            IconLoadStatus iconStatus() const;

            // ----------------------------------------------------------------
            // Called by TitleIconLoader (background thread)
            // ----------------------------------------------------------------
            void setIconData(std::vector<u8>&& data);
            void setName(const std::string& n);
            void setIconStatus(IconLoadStatus s);


            ~Title();
    };

} // namespace NX

#endif // TITLE_HPP
