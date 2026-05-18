#include "ui/screen/AllActivity.hpp"
#include "Application.hpp"
#include "utils/Lang.hpp"
#include "utils/Utils.hpp"
#include "utils/Time.hpp"
#include "nx/TitleIconLoader.hpp"

namespace Screen {
    AllActivity::AllActivity(Main::Application * a) {
        this->app = a;

        // Null-initialize onLoad() pointers for safety on rapid screen switches.
        this->heading   = nullptr;
        this->hours     = nullptr;
        this->image     = nullptr;
        this->list      = nullptr;
        this->menu      = nullptr;
        this->updateElm = nullptr;

        // Create "static" elements
        Aether::Rectangle * r;
        if (!this->app->config()->tImage() || this->app->config()->gTheme() != ThemeType::Custom) {
            r = new Aether::Rectangle(400, 88, 850, 559);
            r->setColour(this->app->theme()->altBG());
            this->addElement(r);
        }
        r = new Aether::Rectangle(30, 87, 1220, 1);
        r->setColour(this->app->theme()->fg());
        this->addElement(r);
        r = new Aether::Rectangle(30, 647, 1220, 1);
        r->setColour(this->app->theme()->fg());
        this->addElement(r);
        Aether::ControlBar * c = new Aether::ControlBar();
        c->addControl(Aether::Button::A, "common.buttonHint.ok"_lang);
        c->addControl(Aether::Button::B, "common.buttonHint.back"_lang);
        c->addControl(Aether::Button::X, "common.buttonHint.sort"_lang);
        c->setDisabledColour(this->app->theme()->text());
        c->setEnabledColour(this->app->theme()->text());
        this->addElement(c);

        // Create sort overlay
        this->sortOverlay = new Aether::PopupList("common.sort.heading"_lang);
        this->sortOverlay->setBackLabel("common.buttonHint.back"_lang);
        this->sortOverlay->setOKLabel("common.buttonHint.ok"_lang);
        this->sortOverlay->setBackgroundColour(this->app->theme()->altBG());
        this->sortOverlay->setHighlightColour(this->app->theme()->accent());
        this->sortOverlay->setLineColour(this->app->theme()->fg());
        this->sortOverlay->setListLineColour(this->app->theme()->mutedLine());
        this->sortOverlay->setTextColour(this->app->theme()->text());

        this->onButtonPress(Aether::Button::X, [this](){
            this->setupOverlay();
        });
        this->onButtonPress(Aether::Button::B, [this](){
            if (this->app->isUserPage()) {
                this->app->exit();
            } else {
                this->app->setScreen(ScreenID::UserSelect);
            }
        });
        this->onButtonPress(Aether::Button::ZR, [this](){
            this->app->setHoldDelay(30);
        });
        this->onButtonRelease(Aether::Button::ZR, [this](){
            this->app->setHoldDelay(100);
        });
        this->onButtonPress(Aether::Button::ZL, [this](){
            this->app->setHoldDelay(30);
        });
        this->onButtonRelease(Aether::Button::ZL, [this](){
            this->app->setHoldDelay(100);
        });
    }

    void AllActivity::setupOverlay() {
        if (!this->list) {
            return;
        }
        this->sortOverlay->removeEntries();

        SortType sort = this->list->sort();
        for (int i = 0; i < SortType::TotalSorts; i++) {
            SortType s = (SortType)i;
            this->sortOverlay->addEntry(toString(s), [this, s](){
                this->list->setSort(s);
            }, sort == s);
        }

        this->setFocussed(this->list);
        this->app->addOverlay(this->sortOverlay);
    }

    void AllActivity::onLoad() {
        // User avatar (synchronous – small JPEG, one shot)
        this->heading = new Aether::Text(150, 45,
            Utils::formatHeading(this->app->activeUser()->username()), 28);
        this->heading->setY(this->heading->y() - this->heading->h() / 2);
        this->heading->setColour(this->app->theme()->text());
        this->addElement(this->heading);

        this->image = new Aether::Image(65, 14,
            this->app->activeUser()->imgPtr(),
            this->app->activeUser()->imgSize(),
            Aether::Render::Wait);
        this->image->setScaleDimensions(60, 60);
        this->image->renderSync();
        this->addElement(this->image);

        // Side menu
        this->menu = new Aether::Menu(30, 88, 388, 559);
        this->menu->addElement(new Aether::MenuOption(
            "common.screen.recentActivity"_lang,
            this->app->theme()->accent(), this->app->theme()->text(), [this](){
                this->app->setScreen(ScreenID::RecentActivity);
            }));
        Aether::MenuOption * opt = new Aether::MenuOption(
            "common.screen.allActivity"_lang,
            this->app->theme()->accent(), this->app->theme()->text(), nullptr);
        this->menu->addElement(opt);
        this->menu->setActiveOption(opt);
        this->menu->addElement(new Aether::MenuSeparator(this->app->theme()->mutedLine()));
        this->menu->addElement(new Aether::MenuOption(
            "common.screen.settings"_lang,
            this->app->theme()->accent(), this->app->theme()->text(), [this](){
                this->app->setScreen(ScreenID::Settings);
            }));
        this->menu->setFocussed(opt);
        this->addElement(this->menu);

        // List
        this->list = new CustomElm::SortedList(420, 88, 810, 559);
        this->list->setCatchup(11);
        this->list->setHeadingColour(this->app->theme()->mutedText());
        this->list->setScrollBarColour(this->app->theme()->mutedLine());

        // Clear icon-pending pairs from any previous load.
        this->pendingTitles_.clear();

        std::vector<AdjustmentValue> adjustments = this->app->config()->adjustmentValues();
        std::vector<NX::Title *> t = this->app->titleVector();
        std::vector<uint64_t> hidden = this->app->config()->hiddenTitles();
        uint64_t totalSecs = 0;

        for (size_t i = 0; i < t.size(); i++) {
            if (std::find(hidden.begin(), hidden.end(), t[i]->titleID()) != hidden.end()) {
                continue;
            }

            NX::RecentPlayStatistics *ps = this->app->playdata()->getRecentStatisticsForTitleAndUser(
                t[i]->titleID(),
                std::numeric_limits<u64>::min(),
                std::numeric_limits<u64>::max(),
                this->app->activeUser()->ID());
            NX::PlayStatistics *ps2 = this->app->playdata()->getStatisticsForUser(
                t[i]->titleID(), this->app->activeUser()->ID());

            auto it = std::find_if(adjustments.begin(), adjustments.end(),
                [this, &t, i](AdjustmentValue val) {
                    return val.titleID == t[i]->titleID() && val.userID == this->app->activeUser()->ID();
                });
            if (it != adjustments.end()) {
                // Guard against unsigned underflow: if the negative adjustment
                // would take playtime below zero, clamp to zero instead.
                if (it->value < 0 && (u64)(-it->value) > ps->playtime) {
                    ps->playtime = 0;
                } else {
                    ps->playtime += it->value;
                }
            }

            totalSecs += ps->playtime;
            if (ps->launches == 0) {
                ps2->firstPlayed = Utils::Time::getTimeT(Utils::Time::getTmForCurrentTime());
                ps2->lastPlayed  = ps2->firstPlayed;
                ps->launches = 1;
                if (ps->playtime == 0) {
                    delete ps;
                    delete ps2;
                    continue;
                }
            }

            SortInfo * si = new SortInfo;
            si->name       = t[i]->name();
            si->titleID    = t[i]->titleID();
            si->firstPlayed = ps2->firstPlayed;
            si->lastPlayed  = ps2->lastPlayed;
            si->playtime    = ps->playtime;
            si->launches    = ps->launches;

            CustomElm::ListActivity * la = new CustomElm::ListActivity();

            auto status = t[i]->iconStatus();
            if (status == NX::IconLoadStatus::Loaded) {
                la->setImage(t[i]->imgPtr(), t[i]->imgSize());
            }
            la->setTitle(t[i]->name());
            if (status != NX::IconLoadStatus::Loaded) {
                pendingTitles_.push_back({ la, t[i], si });
            }
            la->setPlaytime(Utils::playtimeToPlayedForString(ps->playtime));
            la->setLeftMuted(Utils::lastPlayedToString(ps2->lastPlayed));
            la->setRightMuted(Utils::launchesToPlayedString(ps->launches));
            // Capture the titleID rather than the loop index i.
            // After sorting the list, the index stored in the lambda would point
            // to a different title; resolving by ID at press-time is correct.
            TitleID tid = t[i]->titleID();
            la->onPress([this, tid](){
                const auto & tv = this->app->titleVector();
                for (size_t j = 0; j < tv.size(); j++) {
                    if (tv[j]->titleID() == tid) {
                        this->app->setActiveTitle(j);
                        break;
                    }
                }
                this->app->pushScreen();
                this->app->setScreen(ScreenID::Details);
            });
            la->setTitleColour(this->app->theme()->text());
            la->setPlaytimeColour(this->app->theme()->accent());
            la->setMutedColour(this->app->theme()->mutedText());
            la->setLineColour(this->app->theme()->mutedLine());
            this->list->addElement(la, si);

            delete ps;
            delete ps2;
        }

        this->list->setSort(this->app->config()->lSort());
        this->addElement(this->list);

        this->hours = new Aether::Text(1215, 44,
            Utils::playtimeToTotalPlaytimeString(totalSecs), 20);
        this->hours->setXY(
            this->hours->x() - this->hours->w(),
            this->hours->y() - this->hours->h() / 2);
        this->hours->setColour(this->app->theme()->mutedText());
        this->addElement(this->hours);

        this->updateElm = nullptr;
        if (this->app->hasUpdate()) {
            this->updateElm = new Aether::Image(50, 669, "romfs:/icon/download.png");
            this->updateElm->setColour(this->app->theme()->text());
            this->addElement(this->updateElm);
        }
    }

    void AllActivity::update(uint32_t dt) {
        // Parent update (input, animations, etc.)
        Screen::update(dt);

        // Lazy-load icons: poll pending pairs, splice out completed ones.
        // We limit the number of textures uploaded per frame to 2 so the GPU
        // is not hammered (matching sphaira's image_load_max pattern).
        int uploaded = 0;
        auto it = pendingTitles_.begin();
        while (it != pendingTitles_.end() && uploaded < 2) {
            auto status = it->title->iconStatus();
            if (status == NX::IconLoadStatus::Loaded) {
                it->element->setImage(it->title->imgPtr(), it->title->imgSize());
                it->element->setTitle(it->title->name());
                if (it->sortInfo) it->sortInfo->name = it->title->name();
                it = pendingTitles_.erase(it);
                uploaded++;
            } else if (status == NX::IconLoadStatus::Error) {
                it->element->setTitle(it->title->name());
                if (it->sortInfo) it->sortInfo->name = it->title->name();
                it = pendingTitles_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void AllActivity::onUnload() {
        this->pendingTitles_.clear();
        this->removeElement(this->heading);
        this->heading = nullptr;
        this->removeElement(this->hours);
        this->hours = nullptr;
        this->removeElement(this->image);
        this->image = nullptr;
        this->removeElement(this->list);
        this->list = nullptr;
        this->removeElement(this->menu);
        this->menu = nullptr;
        this->removeElement(this->updateElm);
        this->updateElm = nullptr;
    }

    AllActivity::~AllActivity() {
        delete this->sortOverlay;
    }
}
