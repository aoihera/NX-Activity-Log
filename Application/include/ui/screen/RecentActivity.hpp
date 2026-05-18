#ifndef SCREEN_RECENTACTIVITY_HPP
#define SCREEN_RECENTACTIVITY_HPP

#include "ui/element/Graph.hpp"
#include "ui/element/ListActivity.hpp"
#include "ui/element/SortedList.hpp"
#include "nx/Title.hpp"
#include <vector>

// Forward declaration due to circular dependency
namespace Main {
    class Application;
};

namespace Screen {
    class RecentActivity : public Aether::Screen {
        private:
            // Pointer to app for theme
            Main::Application * app;

            // Updates the "recent activity" part of the screen
            void updateActivity();

            // Element which marks top of sessions
            Aether::Element * topElm;

            // Container holding heading and L/R
            Aether::Container * header;

            // Pointers to elements
            Aether::ListHeading * gameHeading;
            CustomElm::Graph * graph;
            Aether::Text * graphHeading;
            Aether::Text * graphSubheading;
            Aether::Text * heading;
            Aether::Image * image;
            Aether::Text * hours;
            Aether::List * list;
            Aether::Menu * menu;
            Aether::Text * noStats;
            Aether::Image * updateElm;

            // Pending icon uploads: list-item element <-> title whose icon is
            // being fetched by TitleIconLoader.  Drained lazily in update().
            struct PendingTitle {
                CustomElm::ListActivity * element;
                NX::Title               * title;
            };
            std::vector<PendingTitle> pendingTitles_;

            // Copy of tm on push
            struct tm tmCopy;
            // Copy of viewPeriod on push
            ViewPeriod viewCopy;

        public:
            // Passed main application object
            RecentActivity(Main::Application *);

            // Updates list when time is changed
            void update(uint32_t);

            // Prepare user-specific elements + list
            void onLoad();
            // Delete elements created in onLoad()
            void onUnload();

            void onPush();
            void onPop();
    };
};

#endif