#ifndef SCREEN_ADJUSTPLAYTIME_HPP
#define SCREEN_ADJUSTPLAYTIME_HPP

#include "Aether/Aether.hpp"
#include "ui/element/ListAdjust.hpp"
#include "nx/Title.hpp"
#include <vector>

namespace Main {
    class Application;
};

namespace CustomOvl {
    class PlaytimePicker;
};

namespace Screen {
    // Screen allowing user to adjust the playtime displayed in All Activity.
    class AdjustPlaytime : public Aether::Screen {
        private:
            // Pointer to application object
            Main::Application * app;

            // List object
            Aether::List * list;

            // User
            Aether::Image * userimage;
            Aether::Text * username;

            // Adjustment picker
            CustomOvl::PlaytimePicker * picker;

            // Helper to get value string
            std::string getValueString(int);

            // Create and show the adjustment overlay
            void setupPlaytimePicker(const std::string &, size_t, CustomElm::ListAdjust *);

            // Pending icon uploads
            struct IconPair {
                CustomElm::ListAdjust * element;
                NX::Title             * title;
            };
            std::vector<IconPair> iconPairs_;

            // Vector of title IDs and their adjustment value
            std::vector<AdjustmentValue> adjustments;

        public:
            // Constructs the screen
            AdjustPlaytime(Main::Application *);

            // Poll pending icon loads
            void update(uint32_t);

            // Create relevant elements on load
            void onLoad();

            // Undo onLoad();
            void onUnload();
    };
};

#endif
