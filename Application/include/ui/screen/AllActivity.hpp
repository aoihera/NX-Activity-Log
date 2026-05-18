#ifndef SCREEN_ALLACTIVITY_HPP
#define SCREEN_ALLACTIVITY_HPP

#include "ui/element/SortedList.hpp"
#include "ui/element/ListActivity.hpp"
#include "nx/Title.hpp"
#include <vector>

namespace Main {
    class Application;
};

namespace Screen {
    class AllActivity : public Aether::Screen {
        private:
            Main::Application * app;

            Aether::Text * heading;
            Aether::Text * hours;
            Aether::Image * image;
            CustomElm::SortedList * list;
            Aether::Menu * menu;
            Aether::Image * updateElm;
            Aether::PopupList * sortOverlay;

            struct PendingTitle {
                CustomElm::ListActivity * element;
                NX::Title               * title;
                SortInfo                * sortInfo;
            };
            std::vector<PendingTitle> pendingTitles_;

            void setupOverlay();

        public:
            AllActivity(Main::Application *);
            void onLoad();
            void update(uint32_t dt);
            void onUnload();
            ~AllActivity();
    };
};

#endif
