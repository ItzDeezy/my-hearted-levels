#include <Geode/Geode.hpp>
#include <Geode/modify/LevelCell.hpp>
#include <Geode/modify/LevelBrowserLayer.hpp>

using namespace geode::prelude;

// quick helper to get a unique key per level
static std::string getLevelKey(GJGameLevel* lvl) {
    if (!lvl) return "";
    int id = (int)lvl->m_levelID;
    if (id > 0)
        return "id_" + std::to_string(id);
    return "local_" + std::string(lvl->m_levelName);
}

static bool isFav(GJGameLevel* lvl) {
    return Mod::get()->getSavedValue<bool>("fav_" + getLevelKey(lvl), false);
}

static void saveFav(GJGameLevel* lvl, bool val) {
    Mod::get()->setSavedValue("fav_" + getLevelKey(lvl), val);
}

// keeps filter on while the game is open and resets on restart
static bool g_filterOn = false;

// heart button on each level cell

class $modify(MyLevelCell, LevelCell) {

    struct Fields {
        CCMenuItemSpriteExtra* heartBtn = nullptr;
    };

    void loadFromLevel(GJGameLevel* lvl) {
        LevelCell::loadFromLevel(lvl);

        // only show heart on editable (local) levels
        if (!lvl || !lvl->m_isEditable) return;
        if (!m_mainMenu || !m_toggler) return;

        bool faved = isFav(lvl);

        // if button already exists just swap the sprite
        if (m_fields->heartBtn) {
            auto frame = CCSpriteFrameCache::get()->spriteFrameByName(
                faved ? "gj_heartOn_001.png" : "gj_heartOff_001.png"
            );
            static_cast<CCSprite*>(m_fields->heartBtn->getNormalImage())->setDisplayFrame(frame);
            return;
        }

        auto spr = CCSprite::createWithSpriteFrameName(
            faved ? "gj_heartOn_001.png" : "gj_heartOff_001.png"
        );
        spr->setScale(0.77f);

        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(MyLevelCell::onHeart));
        btn->stopAllActions();
        btn->setScale(1.f);
        btn->setSizeMult(1.f);

        // put it just to the left of the checkbox (with offsetting)
        auto cbPos = this->convertToNodeSpace(
            m_toggler->getParent()->convertToWorldSpace(m_toggler->getPosition())
        );
        btn->setPosition({ cbPos.x - 30.f, cbPos.y });
        btn->setID("fav-heart-btn"_spr);

        m_fields->heartBtn = btn;

        auto menu = CCMenu::create();
        menu->setPosition(ccp(0, 0));
        menu->addChild(btn);
        this->addChild(menu, 10);
    }

    void onHeart(CCObject*) {
        if (!m_level || !m_fields->heartBtn) return;

        bool newVal = !isFav(m_level);
        saveFav(m_level, newVal);

        auto frame = CCSpriteFrameCache::get()->spriteFrameByName(
            newVal ? "gj_heartOn_001.png" : "gj_heartOff_001.png"
        );
        static_cast<CCSprite*>(m_fields->heartBtn->getNormalImage())->setDisplayFrame(frame);
    }
};

// filter (basically show only the level that are favorited)

class $modify(MyLevelBrowser, LevelBrowserLayer) {

    struct Fields {
        bool filterActive = false;
        bool isMyLevels   = false;

        CCMenuItemSpriteExtra* filterBtn  = nullptr;
        CCMenuItemSpriteExtra* favPrevBtn = nullptr;
        CCMenuItemSpriteExtra* favNextBtn = nullptr;
        CCMenu* favNavMenu = nullptr;

        int  favPage        = 0;
        int  totalFavPages  = 1;
        std::string countLabel = "";
    };

    // grab all local levels that are hearted
    CCArray* collectFavs() {
        auto out = CCArray::create();
        auto locals = LocalLevelManager::get()->m_localLevels;
        if (!locals) return out;
        for (unsigned int i = 0; i < locals->count(); i++) {
            auto lvl = static_cast<GJGameLevel*>(locals->objectAtIndex(i));
            if (lvl && isFav(lvl)) out->addObject(lvl);
        }
        return out;
    }

    void applyFavUI(float) {
        // rename the list header
        if (auto list = this->getChildByID("GJListLayer")) {
            if (auto title = list->getChildByID("title"))
                static_cast<CCLabelBMFont*>(title)->setString("My Favorited Levels");
        }

        // update count text
        if (auto lbl = static_cast<CCLabelBMFont*>(this->getChildByID("level-count-label")))
            lbl->setString(m_fields->countLabel.c_str());

        // hide the normal nav, show ours
        if (auto n = this->getChildByID("next-page-menu")) n->setVisible(false);
        if (auto p = this->getChildByID("prev-page-menu")) p->setVisible(false);
        if (auto m = this->getChildByID("page-menu"))      m->setVisible(false);

        if (m_fields->favNavMenu) m_fields->favNavMenu->setVisible(true);
        if (m_fields->favNextBtn) m_fields->favNextBtn->setVisible(m_fields->favPage < m_fields->totalFavPages - 1);
        if (m_fields->favPrevBtn) m_fields->favPrevBtn->setVisible(m_fields->favPage > 0);
    }

    void restoreNormalUI() {
        if (auto n = this->getChildByID("next-page-menu")) n->setVisible(true);
        if (auto p = this->getChildByID("prev-page-menu")) p->setVisible(true);
        if (auto m = this->getChildByID("page-menu"))      m->setVisible(true);
        if (m_fields->favNavMenu) m_fields->favNavMenu->setVisible(false);

        // reset title
        if (auto list = this->getChildByID("GJListLayer")) {
            if (auto title = list->getChildByID("title"))
                static_cast<CCLabelBMFont*>(title)->setString("My Levels");
        }
    }

    bool init(GJSearchObject* searchObj) {
        m_fields->isMyLevels = (searchObj->m_searchType == SearchType::MyLevels);

        if (m_fields->isMyLevels) {
            m_fields->filterActive = g_filterOn;
            m_fields->favPage = 0;
        }

        if (!LevelBrowserLayer::init(searchObj)) return false;
        if (!m_fields->isMyLevels) return true;

        // filter toggle (on and off)
        auto filterSpr = CCSprite::createWithSpriteFrameName(
            m_fields->filterActive ? "gj_heartOn_001.png" : "gj_heartOff_001.png"
        );
        filterSpr->setScale(0.95f);

        auto filterBtn = CCMenuItemSpriteExtra::create(
            filterSpr, this, menu_selector(MyLevelBrowser::onToggleFilter)
        );
        filterBtn->stopAllActions();
        filterBtn->setScale(1.f);
        filterBtn->setSizeMult(1.f);
        m_fields->filterBtn = filterBtn;

        auto filterMenu = CCMenu::create();
        filterMenu->addChild(filterBtn);

        // try to anchor near existing ui elements
        float bx = 27.f;
        float by = CCDirector::get()->getWinSize().height - 175.f;
        if (auto anchor = this->getChildByID("my-levels-menu")) {
            bx = anchor->getPositionX() + 40.f;
            by = anchor->getPositionY() - anchor->getContentSize().height / 2.f + 17.f;
        } else if (auto sb = this->getChildByID("search-button")) {
            bx = sb->getPositionX();
            by = sb->getPositionY() - 90.f;
        }
        filterMenu->setPosition(bx, by);
        this->addChild(filterMenu, 10);

        // a c'ustom favorite page navigation (similar to the normal gd)
        auto prevSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        auto prevBtn = CCMenuItemSpriteExtra::create(prevSpr, this, menu_selector(MyLevelBrowser::onFavPrev));
        prevBtn->setSizeMult(1.f);

        auto nextSpr = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        nextSpr->setFlipX(true);
        auto nextBtn = CCMenuItemSpriteExtra::create(nextSpr, this, menu_selector(MyLevelBrowser::onFavNext));
        nextBtn->setSizeMult(1.f);

        auto navMenu = CCMenu::create();
        navMenu->addChild(prevBtn);
        navMenu->addChild(nextBtn);
        navMenu->setVisible(false);
        auto ws = CCDirector::get()->getWinSize();
        prevBtn->setPosition({ -ws.width / 2.f + 24.f, 0.f });
        nextBtn->setPosition({  ws.width / 2.f - 24.f, 0.f });
        navMenu->setPosition(ws.width / 2.f, ws.height / 2.f);

        m_fields->favPrevBtn = prevBtn;
            m_fields->favNextBtn = nextBtn;
        m_fields->favNavMenu = navMenu;
        this->addChild(navMenu, 10);

        return true;
    }

    void loadLevelsFinished(CCArray* levels, const char* key, int p2) {
        if (!m_fields->isMyLevels || !m_fields->filterActive) {
            LevelBrowserLayer::loadLevelsFinished(levels, key, p2);
            return;
        }
        auto favs     = collectFavs();
        int  total    = (int)favs->count();
        int  pageSize = 10; // copying the vanilla one (kinda)
        int  pages    = std::max(1, (total + pageSize - 1) / pageSize);

        m_fields->favPage = std::max(0, std::min(m_fields->favPage, pages - 1));

        // slice out the current page
        auto slice = CCArray::create();
        int start = m_fields->favPage * pageSize;
        int end   = std::min(start + pageSize, total);
        for (int i = start; i < end; i++)
            slice->addObject(favs->objectAtIndex(i));

        LevelBrowserLayer::loadLevelsFinished(slice, key, p2);

        int dispStart = total > 0 ? start + 1 : 0;
        m_fields->countLabel   = std::to_string(dispStart) + " TO " + std::to_string(end) + " OF " + std::to_string(total);
        m_fields->totalFavPages = pages;

        this->unschedule(SEL_SCHEDULE(&MyLevelBrowser::applyFavUI));
        this->scheduleOnce(SEL_SCHEDULE(&MyLevelBrowser::applyFavUI), 0.f);
    }

    // "fake" page navigation cuz im using my own slice system
    void onFavNext(CCObject*) {
        m_fields->favPage++;
        this->onRefresh(nullptr);
    }
    void onFavPrev(CCObject*) {
        m_fields->favPage--;
        this->onRefresh(nullptr);
    }
    void onToggleFilter(CCObject*) {
        m_fields->filterActive = !m_fields->filterActive;
        m_fields->favPage = 0;
        g_filterOn = m_fields->filterActive;

        auto frame = CCSpriteFrameCache::get()->spriteFrameByName(
            m_fields->filterActive ? "gj_heartOn_001.png" : "gj_heartOff_001.png"
        );
        static_cast<CCSprite*>(m_fields->filterBtn->getNormalImage())->setDisplayFrame(frame);

        if (!m_fields->filterActive) restoreNormalUI();
        this->onRefresh(nullptr);
    }
};
