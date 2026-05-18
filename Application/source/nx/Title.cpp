#include "nx/Title.hpp"
#include <mutex>

namespace NX {

    // ------------------------------------------------------------------
    // Installed title – name + icon come from the async loader later.
    // ------------------------------------------------------------------
    Title::Title(TitleID titleID, bool installed)
        : titleID_(titleID)
        , is_installed(installed)
        , name_("")
        , iconStatus_(IconLoadStatus::None)
    {
    }

    // ------------------------------------------------------------------
    // Missing / uninstalled title – name is known, icon will stay empty
    // and the UI should fall back to the no_icon asset.
    // ------------------------------------------------------------------
    Title::Title(const TitleID titleID, const std::string & name)
        : titleID_(titleID)
        , is_installed(false)
        , name_(name)
        , iconStatus_(IconLoadStatus::Error)  // don't bother queuing
    {
    }

    TitleID Title::titleID() const { return titleID_; }
    bool    Title::isInstalled() const { return is_installed; }

    std::string Title::name() const {
        std::lock_guard<std::mutex> lk(nameMutex_);
        return name_;
    }

    const std::vector<u8>& Title::iconData() const { return iconData_; }

    // Legacy accessors used by existing Aether::Image(ptr, size, ...) call-sites.
    u8*  Title::imgPtr()  { return iconData_.empty() ? nullptr : iconData_.data(); }
    u32  Title::imgSize() { return static_cast<u32>(iconData_.size()); }

    NX::IconLoadStatus Title::iconStatus() const {
        return iconStatus_.load(std::memory_order_acquire);
    }

    // Called by TitleIconLoader from the background thread.
    void Title::setIconData(std::vector<u8>&& data) {
        iconData_ = std::move(data);
    }

    void Title::setName(const std::string& n) {
        std::lock_guard<std::mutex> lk(nameMutex_);
        name_ = n;
    }

    void Title::setIconStatus(IconLoadStatus s) {
        iconStatus_.store(s, std::memory_order_release);
    }


    Title::~Title() {
        // iconData_ is a std::vector, cleaned up automatically.
    }

} // namespace NX
