#include "utils.hpp"
#include "Callback.hpp"

#include "CustomTextbox.hpp"

#include <Geode/modify/DialogLayer.hpp>
#include <Geode/modify/DialogObject.hpp>
#include <Geode/modify/CCFadeIn.hpp>


struct CustomDialogData {
    std::string customTexture = "";
    float iconScale = 1.0f;
    matjson::Value customSound = nullptr;
    matjson::Value blipSound = nullptr;
    matjson::Value changeBG = nullptr;
    bool autoSkip = false;
    Toggler pauseMusic = Toggler::Default;
};
std::map<int, CustomDialogData> storedCustomData;

matjson::Value blipSound;
bool activeBlipSound = false;

void dialogCallback(matjson::Value data, bool forced = false) {
    CallerBackerInator cb;
    cb.runCallback(data["onComplete"], forced);
};

// Custom dialog portraits, blips, and more
class $modify(CustomDialogLayer, DialogLayer) {
    struct Fields {
        int messageIndex = -1;
        bool autoSkip = false;
        std::map<int, CustomDialogData> customData;
    };

    void displayDialogObject(DialogObject* obj) {

        activeBlipSound = false;

        DialogLayer::displayDialogObject(obj);

        m_fields->messageIndex += 1;

        // Load custom data
        if (m_fields->messageIndex == 0) {
            m_fields->customData = storedCustomData;
            storedCustomData.clear();
        }

        // Stop here if no custom data
        if (m_fields->customData.empty()) return;

        // Check if has custom data
        CustomDialogData data;
        if (m_fields->customData.contains(m_fields->messageIndex)) data = m_fields->customData[m_fields->messageIndex];

        // Blip sound
        if (data.blipSound != nullptr) {
            blipSound = data.blipSound;
            activeBlipSound = true;
        }

        // Custom appear sound
        if (data.customSound != nullptr) playCustomSound(data.customSound);

        // Pause music
        if (data.pauseMusic != Toggler::Default) FMODAudioEngine::sharedEngine()->m_backgroundMusicChannel->setPaused(data.pauseMusic == Toggler::On);

        // Auto skip
        if (data.autoSkip) m_fields->autoSkip = true;

        // Change background
        if (data.changeBG != nullptr) setBorderFromData(m_mainLayer, data.changeBG, 2);

        // Clear custom portrait
        m_mainLayer->removeChildByID("custom_portrait"_spr);

        // Just in case portrait was set to 0
        if (!m_characterSprite) return;

        // Hide default portrait and create custom one
        if (!data.customTexture.empty()) {
            m_characterSprite->setVisible(false);
            
            CCSprite* newPortrait = getCustomTexture(data.customTexture.c_str(), "exMark_001.png");
            newPortrait->setID("custom_portrait"_spr);
            newPortrait->setPosition(m_characterSprite->getPosition());
            newPortrait->setScale(data.iconScale);
            newPortrait->setZOrder(4);
            m_mainLayer->addChild(newPortrait);
        }

        // Scale default portrait
        else {
            m_characterSprite->setScale(data.iconScale);
        }
    }

    void fadeInTextFinished(TextArea* p0) {
        activeBlipSound = false;
        DialogLayer::fadeInTextFinished(p0);
        if (m_fields->autoSkip) {
            m_fields->autoSkip = false;
            return DialogLayer::handleDialogTap();
        }
    }

    void handleDialogTap() {
        activeBlipSound = false;
        if (m_fields->autoSkip && !m_skippable && m_animating) {  // m_skippable is actually if it's unskippable
            m_fields->autoSkip = false;
            DialogLayer::fadeInTextFinished(m_textArea);
            return DialogLayer::handleDialogTap();
        }
        DialogLayer::handleDialogTap();
    }
};

// I absolutely cannot believe this works
class $modify(CCFadeIn) {
    virtual void update(float time) {
        if (m_bFirstTick && time < 1 && activeBlipSound && getDuration() == 0.2f) {
            m_bFirstTick = false;
            playCustomSound(blipSound);
        }
        CCFadeIn::update(time);
    }
};

void CustomTextbox::showTextbox(std::string id) {
    auto data = getTextboxByID(CustomTextbox::jsonFilePath, CustomTextbox::jsonFileName, id, "Dialogue");
    if (data == nullptr) return;

    bool pausesMusic = false;

    auto dialogLines = CCArray::create();
    std::map<int, CustomDialogData> customData;

    // If no messages, just run the callback
    if (!data.contains("messages") || !data["messages"].isArray()) {
        if (data.contains("onComplete")) dialogCallback(data, true);
        return;
    } 

    matjson::Value messageList = data["messages"].asArray().unwrap();
    
    for (auto m : messageList) {
        ccColor3B col = getColor(m, "nameColor");

        std::string content = getStr(m, "content", "");
        if (!content.empty()) {
            content = utils::string::replace(content, "<...>", "<d010>.<d010>.<d010>.");
            content = utils::string::replace(content, "<username>", GameManager::get()->m_playerName);
        }

        DialogObject* dObj = DialogObject::create(
            getStr(m, "name", "").c_str(),
            content.c_str(),
            getPortrait(m, 8),
            getNum(m, "scale", 1.0f),
            getBool(m, "unskippable", false),
            col
        );

        // Custom message data
        CustomDialogData data;

        if (m.contains("customIcon")) data.customTexture = getStr(m, "customIcon", "");
        if (m.contains("iconScale")) data.iconScale = getNum(m, "iconScale", 1.0f);
        if (m.contains("sound")) data.customSound = m["sound"];
        if (m.contains("textSound")) data.blipSound = m["textSound"];
        if (m.contains("setBackground")) data.changeBG = m["setBackground"];
        if (m.contains("autoSkip")) data.autoSkip = getBool(m, "autoSkip");
        if (m.contains("pauseMusic")) {
            Toggler pauseVal = getBool(m, "pauseMusic") ? Toggler::On : Toggler::Off;
            data.pauseMusic = pauseVal; 
            if (pauseVal == Toggler::On) pausesMusic = true;
        }
        customData[dialogLines->count()] = data;

        dialogLines->addObject(dObj);
    }

    storedCustomData = customData;

    int defaultBG = getKey(data, "background", TEXTBOX_COLORS, 0); // 0 = try custom BG
    DialogLayer* textboxLayer = DialogLayer::createWithObjects(dialogLines, defaultBG == 0 ? 2 : defaultBG);

    textboxLayer->setID("custom_textbox"_spr);
    textboxLayer->updateChatPlacement(static_cast<DialogChatPlacement>(getKey(data, "position", TEXTBOX_POSITIONS, 0)));
    textboxLayer->animateInRandomSide();
    textboxLayer->addToMainScene();

    if (defaultBG == 0) setBorder(textboxLayer->m_mainLayer, data, 2, "background");

    // Post-dialogue callback
    if (data.contains("onComplete") || pausesMusic) {
        
        std::function<void()> customCallback = [data]() {
            FMODAudioEngine::sharedEngine()->m_backgroundMusicChannel->setPaused(false);
            dialogCallback(data);
        };
    
        CustomDialogCallback* del = new CustomDialogCallback();
        textboxLayer->addChild(del);
        del->autorelease();
        del->m_callback = customCallback;
        textboxLayer->m_delegate = del;

    }
}

int CustomTextbox::getPortrait(matjson::Value data, int fallback) {
    if (!data.contains("icon")) return fallback;

    else if (data["icon"].isString()) {
        std::string key = data["icon"].asString().unwrapOr("");
        if (TEXTBOX_PORTRAITS.contains(key)) return TEXTBOX_PORTRAITS[key];

        std::erase_if(key, ::isdigit);  // remove all numbers and try again
        if (TEXTBOX_PORTRAITS.contains(key)) return TEXTBOX_PORTRAITS[key];
        
        else return fallback;
    }

    else return getInt(data, "icon", fallback);
}
