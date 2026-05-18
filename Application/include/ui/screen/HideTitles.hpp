#ifndef SCREEN_HIDETITLES_HPP
#define SCREEN_HIDETITLES_HPP

#include "Aether/Aether.hpp"
#include "ui/element/ListHide.hpp"
#include "nx/Title.hpp"
#include <vector>

// Forward declare due to circular dependency
namespace Main {
    class Application;
};

namespace Screen {
    class HideTitles : public Aether::Screen {
        private:
            // Pointer to application object
            Main::Application * app;

            // Main list element
            Aether::List * list;

            // Titles hidden in sidebar
            Aether::Text * hiddenCountText;
            Aether::TextBlock * hiddenSubText;
            std::vector<uint64_t> hiddenIDs;

            // Update the titles hidden counter
            void updateHiddenCounter();

            // Pending icon uploads
            struct IconPair {
                CustomElm::ListHide * element;
                NX::Title           * title;
            };
            std::vector<IconPair> iconPairs_;

        public:
            // Constructor takes application object
            HideTitles(Main::Application *);

            // Poll pending icon loads
            void update(uint32_t);

            // Prepare and show list
            void onLoad();

            // Undo onLoad();
            void onUnload();
    };
};

#endif
