// utils/NX.cpp
// Changes vs. original:
//   - getTitleObjects() no longer passes NsApplicationControlData through
//     each Title constructor.  Title objects are created lightweight; the
//     TitleIconLoader background thread populates icons asynchronously.
//   - startServices() / stopServices() call TitleIconLoader::init() /
//     TitleIconLoader::exit() so callers don't need to know about it.

#include "utils/NX.hpp"
#include "nx/TitleIconLoader.hpp"
#include <algorithm>
#include <iterator>

// Maximum number of titles to read using pdm
#define MAX_TITLES 32768

// Comparison of AccountUids
bool operator == (const AccountUid &a, const AccountUid &b) {
    if (a.uid[0] == b.uid[0] && a.uid[1] == b.uid[1]) {
        return true;
    }
    return false;
}

namespace Utils::NX {
    ThemeType getHorizonTheme() {
        ColorSetId thm;
        Result rc = setsysGetColorSetId(&thm);
        if (R_SUCCEEDED(rc)) {
            switch (thm) {
                case ColorSetId_Light:
                    return ThemeType::Light;
                    break;

                case ColorSetId_Dark:
                    return ThemeType::Dark;
                    break;
            }
        }

        return ThemeType::Dark;
    }

    Language getSystemLanguage() {
        SetLanguage sl;
        u64 l;
        setInitialize();
        setGetSystemLanguage(&l);
        setMakeLanguage(l, &sl);
        setExit();

        Language lang;
        switch (sl) {
            case SetLanguage_ENGB:
            case SetLanguage_ENUS:
                lang = English;
                break;

            case SetLanguage_FR:
                lang = French;
                break;

            case SetLanguage_DE:
                lang = German;
                break;

            case SetLanguage_IT:
                lang = Italian;
                break;

            case SetLanguage_PT:
                lang = Portugese;
                break;

            case SetLanguage_RU:
                lang = Russian;
                break;

            case SetLanguage_ES:
                lang = Spanish;
                break;

            case SetLanguage_ZHHANT:
                lang = ChineseTraditional;
                break;

            case SetLanguage_ZHCN:
            case SetLanguage_ZHHANS:
                lang = Chinese;
                break;

            case SetLanguage_KO:
                lang = Korean;
                break;

            default:
                lang = Default;
                break;
        }

        return lang;
    }

    ::NX::User * getUserPageUser() {
        ::NX::User * u = nullptr;

        AppletType t = appletGetAppletType();
        if (t == AppletType_LibraryApplet) {
            AppletStorage * s = (AppletStorage *)malloc(sizeof(AppletStorage));
            if (R_SUCCEEDED(appletPopInData(s))) {
                if (R_SUCCEEDED(appletPopInData(s))) {
                    AccountUid uid;
                    appletStorageRead(s, 0x8, &uid, 0x10);

                    AccountUid userIDs[ACC_USER_LIST_SIZE];
                    s32 num = 0;
                    accountListAllUsers(userIDs, ACC_USER_LIST_SIZE, &num);
                    for (s32 i = 0; i < num; i++) {
                        if (uid == userIDs[i]) {
                            u = new ::NX::User(uid);
                            break;
                        }
                    }
                }
            }
            free(s);
        }

        return u;
    }

    std::vector<::NX::User *> getUserObjects() {
        std::vector<::NX::User *> users;
        AccountUid userIDs[ACC_USER_LIST_SIZE];
        s32 num = 0;
        Result rc = accountListAllUsers(userIDs, ACC_USER_LIST_SIZE, &num);

        if (R_SUCCEEDED(rc)) {
            for (s32 i = 0; i < num; i++) {
                users.emplace_back(new ::NX::User(userIDs[i]));
            }
        }

        return users;
    }

    std::vector<::NX::Title *> getTitleObjects(std::vector<::NX::User *> u) {
        Result rc = 0;

        // ----------------------------------------------------------------
        // 1. Collect all played titleIDs across all users (unchanged logic).
        // ----------------------------------------------------------------
        std::vector<TitleID> playedIDs;
        for (auto user : u) {
            s32 offset = 0;
            s32 playedTotal = 1;
            PdmAccountPlayEvent *userPlayEvents = new PdmAccountPlayEvent[MAX_TITLES];
            TitleID tmpID = 0;

            while (playedTotal > 0) {
                memset(userPlayEvents, 0, MAX_TITLES * sizeof(PdmAccountPlayEvent));
                rc = pdmqryQueryAccountPlayEvent(offset, user->ID(), userPlayEvents, MAX_TITLES, &playedTotal);
                if (R_SUCCEEDED(rc)) {
                    offset += playedTotal;
                    for (s32 i = 0; i < playedTotal; i++) {
                        tmpID = (static_cast<TitleID>(userPlayEvents[i].application_id[0]) << 32)
                              | userPlayEvents[i].application_id[1];
                        if (tmpID != 0) {
                            if (std::find_if(playedIDs.begin(), playedIDs.end(),
                                    [tmpID](auto id){ return id == tmpID; }) == playedIDs.end()) {
                                playedIDs.emplace_back(tmpID);
                            }
                        }
                    }
                }
            }

            delete[] userPlayEvents;
        }

        // ----------------------------------------------------------------
        // 2. Collect installed titleIDs (unchanged logic).
        // ----------------------------------------------------------------
        std::vector<TitleID> installedIDs;
        NsApplicationRecord *records = new NsApplicationRecord[MAX_TITLES];
        s32 count = 0;
        s32 out = 0;
        while (true) {
            memset(records, 0, MAX_TITLES * sizeof(NsApplicationRecord));
            rc = nsListApplicationRecord(records, MAX_TITLES, count, &out);
            if (R_FAILED(rc) || out == 0) break;
            for (s32 i = 0; i < out; i++) {
                if ((records + i)->application_id != 0) {
                    installedIDs.emplace_back((records + i)->application_id);
                }
            }
            count += out;
        }
        delete[] records;

        // ----------------------------------------------------------------
        // 3. Create lightweight Title objects (no icon load here).
        //    Icons are fetched asynchronously by TitleIconLoader.
        // ----------------------------------------------------------------
        std::vector<::NX::Title *> titles;
        titles.reserve(playedIDs.size());
        for (auto playedID : playedIDs) {
            bool installed = std::find_if(installedIDs.begin(), installedIDs.end(),
                [playedID](auto id){ return id == playedID; }) != installedIDs.end();
            titles.emplace_back(new ::NX::Title(playedID, installed));
        }

        // ----------------------------------------------------------------
        // 4. Kick off background icon loading for all titles at once.
        //    Installed titles have higher priority; uninstalled ones will
        //    likely fail (and show the fallback) anyway.
        // ----------------------------------------------------------------
        TitleIconLoader::pushBatch(titles);

        return titles;
    }

    void startServices() {
        accountInitialize(AccountServiceType_System);
        nsInitialize();
        pdmqryInitialize();
        romfsInit();
        setsysInitialize();
        socketInitializeDefault();

        // Start the async icon loader now that NS is initialised.
        TitleIconLoader::init();

        #if _NXLINK_
            nxlinkStdio();
        #endif
    }

    void stopServices() {
        // Stop loader before NS exits.
        TitleIconLoader::exit();

        accountExit();
        nsExit();
        pdmqryExit();
        romfsExit();
        setsysExit();
        socketExit();
    }
}
