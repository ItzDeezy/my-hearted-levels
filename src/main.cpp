#include <Geode/Geode.hpp>
#include <Geode/modify/LevelCell.hpp>
#include <Geode/modify/LevelBrowserLayer.hpp>

using namespace geode::prelude;

// ─── Helpers ───────────────────────────────────────────────

static std::string levelKey(GJGameLevel* level) {
    if (!level) return "";
    int id = (int)level->m_levelID;
    if (id > 0) return "id_" + std::to_string(id);
    return "local_" + std::string(level->m_levelName);
}

static bool isFavorited(GJGameLevel* level) {
    return Mod::get()->getSavedValue<bool>("fav_" + levelKey(level), false);
}

static void setFavorited(GJGameLevel* level, bool state) {
    Mod::get()->setSavedValue("fav_" + levelKey(level), state);
}

// persists filter state within a session but resets on game relaunch
static bool s_filterActive = false;

// ─── Hook 1: LevelCell ─────────────────────────────────────

class $modify(MyLevelCell, LevelCell) {

    struct Fields {
        CCMenuItemSpriteExtra* heartBtn = nullptr;
    };

    void loadFromLevel(GJGameLevel* level) {
        LevelCell::loadFromLevel(level);

        if (!level || !level->m_isEditable) return;
        if (!m_mainMenu || !m_toggler) return;

        bool hearted = isFavorited(level);

        // cell recycled — just update sprite
        if (m_fields->heartBtn) {
            auto frame = CCSpriteFrameCache::get()->spriteFrameByName(
                hearted ? "gj_heartOn_001.png" : "gj_heartOff_001.png"
            );
            static_cast<CCSprite*>(m_fields->heartBtn->getNormalImage())->setDisplayFrame(frame);
            return;
        }

        auto spr = CCSprite::createWithSpriteFrameName(
            hearted ? "gj_heartOn_001.png" : "gj_heartOff_001.png"
        );
        spr->setScale(0.77f);

        auto btn = CCMenuItemSpriteExtra::create(
            spr, this,
            menu_selector(MyLevelCell::onHeartBtn)
        );
        btn->stopAllActions();
        btn->setScale(1.0f);
        btn->setSizeMult(1.0f);

        auto checkboxPos = this->convertToNodeSpace(
            m_toggler->getParent()->convertToWorldSpace(m_toggler->getPosition())
        );
        btn->setPosition({checkboxPos.x - 30.f, checkboxPos.y});
        btn->setID("fav-heart-btn"_spr);

        m_fields->heartBtn = btn;

        auto menu = CCMenu::create();
        menu->setPosition({0.f, 0.f});
        menu->addChild(btn);
        this->addChild(menu, 10);
    }

    void onHeartBtn(CCObject*) {
        if (!m_level || !m_fields->heartBtn) return;

        bool newState = !isFavorited(m_level);
        setFavorited(m_level, newState);

        auto frame = CCSpriteFrameCache::get()->spriteFrameByName(
            newState ? "gj_heartOn_001.png" : "gj_heartOff_001.png"
        );
        static_cast<CCSprite*>(m_fields->heartBtn->getNormalImage())->setDisplayFrame(frame);
    }
};

// ─── Hook 2: LevelBrowserLayer ─────────────────────────────

class $modify(MyLevelBrowser, LevelBrowserLayer) {

    struct Fields {
        bool filterActive = false;
        CCMenuItemSpriteExtra* filterBtn = nullptr;
        int favPage = 0;
        std::string pendingCountStr = "";
        int pendingTotalPages = 1;
        CCMenuItemSpriteExtra* favPrevBtn = nullptr;
        CCMenuItemSpriteExtra* favNextBtn = nullptr;
        CCMenu* favPageMenu = nullptr;
    };

    CCArray* getFavoritedLevels() {
        auto result = CCArray::create();
        auto allLevels = LocalLevelManager::get()->m_localLevels;
        if (allLevels) {
            for (unsigned int i = 0; i < allLevels->count(); i++) {
                auto level = static_cast<GJGameLevel*>(allLevels->objectAtIndex(i));
                if (level && isFavorited(level)) result->addObject(level);
            }
        }
        return result;
    }

    void updateFavUI(float) {
        if (auto list = this->getChildByID("GJListLayer")) {
            if (auto title = list->getChildByID("title"))
                static_cast<CCLabelBMFont*>(title)->setString("My Favorited Levels");
        }

        if (auto l = static_cast<CCLabelBMFont*>(this->getChildByID("level-count-label")))
            l->setString(m_fields->pendingCountStr.c_str());

        // hide GD's pagination completely
        if (auto n = this->getChildByID("next-page-menu")) n->setVisible(false);
        if (auto p = this->getChildByID("prev-page-menu")) p->setVisible(false);
        if (auto m = this->getChildByID("page-menu"))      m->setVisible(false);

        // show our own buttons based on actual fav page
        bool hasNext = m_fields->favPage < m_fields->pendingTotalPages - 1;
        bool hasPrev = m_fields->favPage > 0;

        if (m_fields->favPageMenu) m_fields->favPageMenu->setVisible(true);
        if (m_fields->favNextBtn)  m_fields->favNextBtn->setVisible(hasNext);
        if (m_fields->favPrevBtn)  m_fields->favPrevBtn->setVisible(hasPrev);
    }

    bool init(GJSearchObject* searchObj) {
        // restore session filter state BEFORE calling original
        // — loadLevelsFinished fires inside it
        if (searchObj->m_searchType == SearchType::MyLevels) {
            m_fields->filterActive = s_filterActive;
            m_fields->favPage = 0;
        }

        if (!LevelBrowserLayer::init(searchObj)) return false;
        if (searchObj->m_searchType != SearchType::MyLevels) return true;

        // ── filter toggle button ──
        auto spr = CCSprite::createWithSpriteFrameName(
            m_fields->filterActive ? "gj_heartOn_001.png" : "gj_heartOff_001.png"
        );
        spr->setScale(0.95f);

        auto btn = CCMenuItemSpriteExtra::create(
            spr, this, menu_selector(MyLevelBrowser::onFavFilter)
        );
        btn->stopAllActions();
        btn->setScale(1.0f);
        btn->setSizeMult(1.0f);
        m_fields->filterBtn = btn;

        auto filterMenu = CCMenu::create();
        filterMenu->addChild(btn);

        float btnX = 27.f;
        float btnY = CCDirector::get()->getWinSize().height - 175.f;
        if (auto anchor = this->getChildByID("my-levels-menu")) {
            btnX = anchor->getPositionX() + 40.f;
            btnY = anchor->getPositionY() - anchor->getContentSize().height / 2.f + 17.f;
        } else if (auto searchBtn = this->getChildByID("search-button")) {
            btnX = searchBtn->getPositionX();
            btnY = searchBtn->getPositionY() - 90.f;
        }
        filterMenu->setPosition(btnX, btnY);
        this->addChild(filterMenu, 10);

        // ── our own prev/next buttons for fav pagination ──
        auto prevSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        auto prevBtn = CCMenuItemSpriteExtra::create(
            prevSpr, this, menu_selector(MyLevelBrowser::onFavPrev)
        );
        prevBtn->setSizeMult(1.0f);

        auto nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        nextSpr->setFlipX(true);
        auto nextBtn = CCMenuItemSpriteExtra::create(
            nextSpr, this, menu_selector(MyLevelBrowser::onFavNext)
        );
        nextBtn->setSizeMult(1.0f);

        auto favPageMenu = CCMenu::create();
        favPageMenu->addChild(prevBtn);
        favPageMenu->addChild(nextBtn);
        favPageMenu->setVisible(false);

        // mirror GD's arrow positions
        auto winSize = CCDirector::get()->getWinSize();
        prevBtn->setPosition({-winSize.width / 2.f + 24.f, 0.f});
        nextBtn->setPosition({ winSize.width / 2.f - 24.f, 0.f});
        favPageMenu->setPosition(winSize.width / 2.f, winSize.height / 2.f);

        m_fields->favPrevBtn = prevBtn;
        m_fields->favNextBtn = nextBtn;
        m_fields->favPageMenu = favPageMenu;
        this->addChild(favPageMenu, 10);

        return true;
    }

    void loadLevelsFinished(CCArray* levels, const char* key, int p2) {
        if (m_fields->filterActive) {
            auto allFavs = getFavoritedLevels();
            int total = (int)allFavs->count();
            int pageSize = 10;
            int totalPages = std::max(1, (total + pageSize - 1) / pageSize);

            // clamp page index
            m_fields->favPage = std::max(0, std::min(m_fields->favPage, totalPages - 1));

            // slice 10 levels for this page
            auto pageItems = CCArray::create();
            int start = m_fields->favPage * pageSize;
            int end = std::min(start + pageSize, total);
            for (int i = start; i < end; i++) {
                pageItems->addObject(allFavs->objectAtIndex(i));
            }

            LevelBrowserLayer::loadLevelsFinished(pageItems, key, p2);

            // store state for the scheduled UI update
            int dispStart = total > 0 ? start + 1 : 0;
            m_fields->pendingCountStr = std::to_string(dispStart) + " TO " +
                                        std::to_string(end) + " OF " +
                                        std::to_string(total);
            m_fields->pendingTotalPages = totalPages;

            // delay one frame so GD doesn't overwrite our changes
            this->unschedule(SEL_SCHEDULE(&MyLevelBrowser::updateFavUI));
            this->scheduleOnce(SEL_SCHEDULE(&MyLevelBrowser::updateFavUI), 0.f);

        } else {
            // restore GD's menus BEFORE calling loadLevelsFinished
            // so it correctly sets button states inside them
            if (auto n = this->getChildByID("next-page-menu")) n->setVisible(true);
            if (auto p = this->getChildByID("prev-page-menu")) p->setVisible(true);
            if (auto m = this->getChildByID("page-menu"))      m->setVisible(true);

            LevelBrowserLayer::loadLevelsFinished(levels, key, p2);

            // restore title
            if (auto list = this->getChildByID("GJListLayer")) {
                if (auto title = list->getChildByID("title"))
                    static_cast<CCLabelBMFont*>(title)->setString("My Levels");
            }

            // hide our custom buttons
            if (m_fields->favPageMenu) m_fields->favPageMenu->setVisible(false);
        }
    }

    void onFavNext(CCObject*) {
        m_fields->favPage++;
        this->onRefresh(nullptr);
    }

    void onFavPrev(CCObject*) {
        m_fields->favPage--;
        this->onRefresh(nullptr);
    }

    void onFavFilter(CCObject*) {
        m_fields->filterActive = !m_fields->filterActive;
        m_fields->favPage = 0;

        // persist within session
        s_filterActive = m_fields->filterActive;

        auto frame = CCSpriteFrameCache::get()->spriteFrameByName(
            m_fields->filterActive ? "gj_heartOn_001.png" : "gj_heartOff_001.png"
        );
        static_cast<CCSprite*>(m_fields->filterBtn->getNormalImage())->setDisplayFrame(frame);

        this->onRefresh(nullptr);
    }
};