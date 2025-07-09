#include "utils.hpp"
#include "Callback.hpp"

#include "CustomChest.hpp"

#include <Geode/modify/RewardUnlockLayer.hpp>

class $modify(MyRUL, RewardUnlockLayer) {
    struct Fields {
        bool m_hideStats;
        matjson::Value m_customSound = nullptr;
    };
    
    void step3() {
        RewardUnlockLayer::step3();
        if (m_fields->m_customSound != nullptr) playCustomSound(m_fields->m_customSound);
        if (!m_fields->m_hideStats) return;

        auto crl = getChildByType<CurrencyRewardLayer>(0);
        if (crl) crl->m_mainNode->setVisible(false);
    }
};

void CustomChest::showChest(std::string id) {

    auto data = getTextboxByID(CustomChest::jsonFilePath, CustomChest::jsonFileName, id, "Chest");
    if (data == nullptr) return;

    int chestType = getKey(data, "type", CHEST_TYPES, 1);
    
    matjson::Value rewardList = data["rewards"].asArray().unwrap();

    // ===== 

    auto chestRewards = CCArray::create();

    for (auto r : rewardList) {
        if (r.contains("iconID")) {
            int rewardType = 9;
            int iconID = getKey(r, "iconID", ITEM_TYPES, 1);
            int iconType = getKey(r, "iconType", ICON_TYPES, 1);

            chestRewards->addObject(GJRewardObject::createItemUnlock(static_cast<UnlockType>(iconType), iconID));
        }

        else {
            int rewardType = getKey(r, "type", REWARD_TYPES, 8);
            int rewardCount = getInt(r, "amount", 1);
            int rewardID = getInt(r, "id", 1);

            chestRewards->addObject(GJRewardObject::create(static_cast<SpecialRewardItem>(rewardType), rewardCount, rewardID));
        }

    }

    auto rewardLayer = RewardUnlockLayer::create(chestType, nullptr);
    rewardLayer->setID("custom_chest"_spr);

    GJRewardItem* item = GJRewardItem::createWithObjects(static_cast<GJRewardType>(chestType), chestRewards);

    MyRUL* customRUL = static_cast<MyRUL*>(rewardLayer);
    if (getBool(data, "hideStats")) customRUL->m_fields->m_hideStats = true;
    if (data.contains("openSound")) customRUL->m_fields->m_customSound = data["openSound"];

    if (data.contains("sound")) playCustomSound(data["sound"]);

    if (data.contains("onClaim")) {
        setCustomButtonCallback(rewardLayer->m_rewardBtn, rewardLayer, data["onClaim"]);
    }

    rewardLayer->show();
    rewardLayer->showCollectReward(item);
}