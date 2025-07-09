#pragma once
#include <Geode/Geode.hpp>
#include <matjson.hpp>

#include "utils.hpp"
#include "CustomChest.hpp"
#include "CustomTextbox.hpp"

using namespace geode::prelude;

class CallerBackerInator {
    public:
        void buttonCallback(CCObject* sender) {
            CCNode* node = static_cast<CCNode*>(sender);

            matjson::Value data = static_cast<ObjWrapper<matjson::Value>*>(node->getUserObject("customAlertCallback"_spr))->getValue();
            
            // Sequences
            if (data.isArray()) {
                auto arr = data.asArray().unwrap();
                int idx = static_cast<ObjWrapper<int>*>(node->getUserObject("customAlertCallbackIndex"_spr))->getValue();
                idx = std::clamp(idx, 0, (int)(arr.size() - 1));
                data = arr[idx];

                int newIndex = getInt(data, "goto", idx + 1);
                node->setUserObject("customAlertCallbackIndex"_spr, ObjWrapper<int>::create(newIndex));
            }

            bool isIgnore = data["ignore"].asBool().unwrapOr(false);

            runCallback(data);

            auto parentNode = node->getUserObject("customAlertCallbackParent"_spr);

            // Chests
            auto chestCheck = typeinfo_cast<RewardUnlockLayer*>(parentNode);
            if (chestCheck != nullptr) return chestCheck->removeMeAndCleanup();

            // Alert stuff
            if (isIgnore) return;

            // Close the popup using black magic and the indomitable human spirit
            // Thanks Alphalaneous for the help
            static_cast<FLAlertLayer*>(parentNode)->keyBackClicked();
        }

        void runCallback(matjson::Value data, bool forced = false) {
            if (!data.isObject()) return;

            if (data.contains("instantSound")) playCustomSound(data["instantSound"]);

            double delay = getNum(data, "delay", 0);
            
            if (delay <= 0) return handleCallback(data, forced);

            auto timer = CCNode::create();
            timer->setID("custom_callback_timer"_spr);
            CCScene::get()->addChild(timer);

            timer->runAction(
                CCSequence::create(
                    CCDelayTime::create(delay),
                    CallFuncExt::create([this, data, timer, forced] { handleCallback(data, forced); timer->removeMeAndCleanup(); }),
                    nullptr
                )
            );
        }

        void handleCallback(matjson::Value data, bool forced = false) {
            if (data.contains("spawnPopup")) (new CustomAlert())->showPopup(getStr(data, "spawnPopup", ""));
            else if (data.contains("spawnChest")) (new CustomChest())->showChest(getStr(data, "spawnChest", ""));
            else if (data.contains("spawnDialogue")) (new CustomTextbox())->showTextbox(getStr(data, "spawnDialogue", ""));
            
            if (data.contains("spawnIcon")) {
                IconData icon = getIcon(data, "spawnIcon");
                
                auto winSize = CCDirector::sharedDirector()->getWinSize();
        
                auto cur = CurrencyRewardLayer::create(0, 0, 0, 0, CurrencySpriteType::Icon, 0, CurrencySpriteType::Icon, 0, winSize / 2, static_cast<CurrencyRewardType>(0), 0, 1);
                auto sceneChildren = CCScene::get()->getChildren();
                auto layerIndex = sceneChildren->count() - (forced ? 1 : 2);
                if (layerIndex < 0) layerIndex = 0;
                auto layer = sceneChildren->objectAtIndex(layerIndex);
                if (layer) typeinfo_cast<CCNode*>(layer)->addChild(cur, 1000);
                
                auto boncyIcon = GJItemIcon::createBrowserItem(static_cast<UnlockType>(icon.type), icon.id);
                cur->createUnlockObject(boncyIcon, winSize / 2, 0.5f);
            }

            if (data.contains("playSound")) playCustomSound(data["playSound"]);

            if (data.contains("pauseMusic")) FMODAudioEngine::sharedEngine()->m_backgroundMusicChannel->setPaused(getBool(data, "pauseMusic"));
        }
};

inline void setCustomButtonCallback(CCMenuItemSpriteExtra* buttonBase, CCObject* parent, matjson::Value data) {
    buttonBase->setUserObject("customAlertCallback"_spr, ObjWrapper<matjson::Value>::create(data));
    buttonBase->setUserObject("customAlertCallbackParent"_spr, parent);
    if (data.isArray()) buttonBase->setUserObject("customAlertCallbackIndex"_spr, ObjWrapper<int>::create(0));

    buttonBase->setTarget(buttonBase, menu_selector(CallerBackerInator::buttonCallback));
}