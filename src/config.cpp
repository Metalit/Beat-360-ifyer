#include "main.hpp"
#include "config.hpp"

#include "GlobalNamespace/MainSystemInit.hpp"
#include "GlobalNamespace/BeatmapCharacteristicCollectionSO.hpp"
#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"

using namespace GlobalNamespace;

std::vector<std::string> GetBaseCharacteristics() {
    auto init = UnityEngine::Object::FindObjectOfType<MainSystemInit*>();
    if (!init || !init->beatmapCharacteristicCollection)
        return {"Standard"};

    std::vector<std::string> ret{};
    auto characteristics = init->beatmapCharacteristicCollection->GetCharacteristicsWithout360Movement();
    for (auto characteristic : characteristics) {
        // I guess this technically doesn't have 360 movement
        if (characteristic->serializedName == "90Degree")
            continue;
        ret.emplace_back(characteristic->serializedName);
    }

    return ret;
}

#include "questui/shared/BeatSaberUI.hpp"

using namespace QuestUI;

void GameplaySetup(UnityEngine::GameObject* self, bool firstActivation) {
    if (!firstActivation)
        return;

    auto container = BeatSaberUI::CreateScrollableSettingsContainer(self);

    self->get_transform()->Find("QuestUIScrollableSettingsContainer")->GetComponent<UnityEngine::RectTransform*>()->set_sizeDelta({105, 65});

    AddConfigValueToggle(container, getConfig().Show360);
    AddConfigValueToggle(container, getConfig().Show90);

    auto characteristics = GetBaseCharacteristics();
    AddConfigValueDropdownString(container, getConfig().BasedOn, characteristics);

    auto text = BeatSaberUI::CreateText(container, "Generator Settings");
    text->set_alignment(TMPro::TextAlignmentOptions::Center);
    text->GetComponent<UnityEngine::RectTransform*>()->set_sizeDelta({90, 7});

    AddConfigValueIncrementFloat(container, getConfig().PreferredBarDuration, 2, 0.01, 0.1, 5);
    AddConfigValueIncrementInt(container, getConfig().LimitRotations360, 1, 0, 100);
    AddConfigValueIncrementInt(container, getConfig().BottleneckRotations360, 1, 0, 100);
    AddConfigValueIncrementInt(container, getConfig().LimitRotations90, 1, 0, 50);
    AddConfigValueIncrementInt(container, getConfig().BottleneckRotations90, 1, 0, 50);
    AddConfigValueToggle(container, getConfig().EnableSpin);
    AddConfigValueIncrementFloat(container, getConfig().TotalSpinTime, 1, 0.1, 0.1, 10);
    AddConfigValueIncrementFloat(container, getConfig().SpinCooldown, 1, 0.5, 0, 60);
    AddConfigValueIncrementFloat(container, getConfig().WallFrontCut, 2, 0.05, 0, 5);
    AddConfigValueIncrementFloat(container, getConfig().WallBackCut, 2, 0.05, 0, 5);
    AddConfigValueIncrementFloat(container, getConfig().MinWallDuration, 2, 0.05, 0, 5);
    AddConfigValueToggle(container, getConfig().WallGenerator);
    AddConfigValueToggle(container, getConfig().OnlyOneSaber);
}
