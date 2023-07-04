#include "main.hpp"
#include "config.hpp"
#include "generator.hpp"

using namespace GlobalNamespace;

static ModInfo modInfo;

Logger& getLogger() {
    static Logger* logger = new Logger(modInfo);
    return *logger;
}

#include "GlobalNamespace/BeatmapCharacteristicSO.hpp"

SafePtr<List<BeatmapCharacteristicSO*>> generatedCharacteristics;

#define SUFFIX_360 "-Generated360"
#define SUFFIX_90 "-Generated90"

#include "UnityEngine/Sprite.hpp"
#include "UnityEngine/HideFlags.hpp"

BeatmapCharacteristicSO* CreateCustomCharacteristic(UnityEngine::Sprite* icon, std::string characteristicName, std::string hintText, std::string serializedName, int sortingOrder) {
    auto characteristic = UnityEngine::ScriptableObject::CreateInstance<BeatmapCharacteristicSO*>();
    characteristic->set_hideFlags(characteristic->get_hideFlags() | UnityEngine::HideFlags::DontUnloadUnusedAsset);
    characteristic->icon = icon;
    characteristic->descriptionLocalizationKey = hintText;
    characteristic->serializedName = serializedName;
    // for now use hintText here instead of characteristicName because PinkCore messed up the hover hint
    characteristic->characteristicNameLocalizationKey = hintText;
    characteristic->compoundIdPartName = serializedName;
    characteristic->requires360Movement = true;
    characteristic->containsRotationEvents = true;
    characteristic->sortingOrder = sortingOrder;
    return characteristic;
}

#include "GlobalNamespace/MainSystemInit.hpp"
#include "GlobalNamespace/BeatmapCharacteristicCollectionSO.hpp"

void CreateCustomCharacteristics() {
    if (!generatedCharacteristics)
        generatedCharacteristics = List<BeatmapCharacteristicSO*>::New_ctor(1);

    getLogger().info("Creating generated custom characteristics");

    auto mainSystemInit = UnityEngine::Resources::FindObjectsOfTypeAll<MainSystemInit*>().First();
    auto currentChars = mainSystemInit->beatmapCharacteristicCollection->beatmapCharacteristics;

    auto normal360 = currentChars.First([](auto c) { return c->serializedName == "360Degree"; });
    auto normal90 = currentChars.First([](auto c) { return c->serializedName == "90Degree"; });

    for (auto characteristic : currentChars) {
        if (characteristic == normal360 || characteristic == normal90)
            continue;

        std::string basedOnName = characteristic->serializedName;
        auto generated360 = CreateCustomCharacteristic(normal360->icon, basedOnName + "GEN360", "Generated 360 Degree", basedOnName + SUFFIX_360, 98);
        auto generated90 = CreateCustomCharacteristic(normal90->icon, basedOnName + "GEN90", "Generated 90 Degree", basedOnName + SUFFIX_90, 99);

        generatedCharacteristics->Add(generated360);
        generatedCharacteristics->Add(generated90);
    }

    ArrayW<BeatmapCharacteristicSO*> newChars{currentChars.Length() + generatedCharacteristics->get_Count()};
    currentChars.CopyTo((Array<BeatmapCharacteristicSO*>*) newChars, 0);
    generatedCharacteristics->CopyTo(newChars, currentChars.Length());
    mainSystemInit->beatmapCharacteristicCollection->beatmapCharacteristics = newChars;
}

BeatmapCharacteristicSO* GetCustomCharacteristic(std::string_view name) {
    if (!generatedCharacteristics)
        return nullptr;
    for (auto& c : ListW<BeatmapCharacteristicSO*>(generatedCharacteristics.ptr())) {
        if (c->serializedName == name)
            return c;
    }
    return nullptr;
}

bool IsCustomCharacteristic(BeatmapCharacteristicSO* characteristic) {
    if (!generatedCharacteristics)
        return false;
    for (auto& c : ListW<BeatmapCharacteristicSO*>(generatedCharacteristics.ptr())) {
        if (c == characteristic)
            return true;
    }
    return false;
}

#include "GlobalNamespace/PlayerDataFileManagerSO.hpp"
#include "GlobalNamespace/PlayerSaveData.hpp"

MAKE_HOOK_MATCH(PlayerDataFileManagerSO_LoadFromCurrentVersion, &PlayerDataFileManagerSO::LoadFromCurrentVersion, PlayerData*, PlayerDataFileManagerSO* self, PlayerSaveData* playerSaveData){

    CreateCustomCharacteristics();

    return PlayerDataFileManagerSO_LoadFromCurrentVersion(self, playerSaveData);
}

#include "GlobalNamespace/IBeatmapLevel.hpp"
#include "GlobalNamespace/IBeatmapLevelData.hpp"
#include "GlobalNamespace/IDifficultyBeatmapSet.hpp"
#include "System/Collections/Generic/IReadOnlyList_1.hpp"
#include "System/Collections/Generic/IReadOnlyCollection_1.hpp"
#include "GlobalNamespace/IDifficultyBeatmap.hpp"

IDifficultyBeatmapSet* GetCharacteristic(IBeatmapLevel* level, std::string_view serializedName) {
    ArrayW<IDifficultyBeatmapSet*> existingChars(level->get_beatmapLevelData()->get_difficultyBeatmapSets());

    for (auto& existingChar : existingChars) {
        if (existingChar->get_beatmapCharacteristic()->serializedName == serializedName)
            return existingChar;
    }
    return nullptr;
}

#include "GlobalNamespace/BeatmapLevelSO_DifficultyBeatmap.hpp"
#include "GlobalNamespace/BeatmapLevelSO_DifficultyBeatmapSet.hpp"
#include "GlobalNamespace/BeatmapDataSO.hpp"
#include "GlobalNamespace/CustomDifficultyBeatmap.hpp"
#include "GlobalNamespace/CustomDifficultyBeatmapSet.hpp"
#include "BeatmapSaveDataVersion3/BeatmapSaveData.hpp"

IDifficultyBeatmapSet* CopyBeatmapSet(IDifficultyBeatmapSet* set, IBeatmapLevel* level, BeatmapCharacteristicSO* newCharacteristic) {
    ArrayW<IDifficultyBeatmap*> originalMaps(set->get_difficultyBeatmaps());
    int count = originalMaps.Length();

    if (count == 0)
        return nullptr;

    if (il2cpp_utils::try_cast<BeatmapLevelSO::DifficultyBeatmap>(originalMaps[0])) {
        ArrayW<BeatmapLevelSO::DifficultyBeatmap*> newMaps(count);
        auto newSet = BeatmapLevelSO::DifficultyBeatmapSet::New_ctor(newCharacteristic, newMaps)->i_IDifficultyBeatmapSet();

        for (int i = 0; i < count; i++) {
            auto cast = (BeatmapLevelSO::DifficultyBeatmap*) originalMaps[i];
            newMaps[i] = BeatmapLevelSO::DifficultyBeatmap::New_ctor(level, cast->difficulty, cast->difficultyRank, cast->noteJumpMovementSpeed, cast->noteJumpStartBeatOffset, cast->beatmapData);
            newMaps[i]->parentDifficultyBeatmapSet = newSet;
        }
        return newSet;
    }
    else if (il2cpp_utils::try_cast<CustomDifficultyBeatmap>(originalMaps[0])) {
        ArrayW<CustomDifficultyBeatmap*> newMaps(count);
        auto newSet = CustomDifficultyBeatmapSet::New_ctor(newCharacteristic);
        newSet->SetCustomDifficultyBeatmaps(newMaps);

        for (int i = 0; i < count; i++) {
            auto cast = (CustomDifficultyBeatmap*) originalMaps[i];
            newMaps[i] = CustomDifficultyBeatmap::New_ctor(level, newSet->i_IDifficultyBeatmapSet(), cast->difficulty, cast->difficultyRank, cast->noteJumpMovementSpeed, cast->noteJumpStartBeatOffset, cast->beatsPerMinute, cast->beatmapSaveData, cast->beatmapDataBasicInfo);
        }
        return newSet->i_IDifficultyBeatmapSet();
    }
    return nullptr;
}

#include "GlobalNamespace/StandardLevelDetailView.hpp"
#include "GlobalNamespace/BeatmapLevelData.hpp"

MAKE_HOOK_MATCH(StandardLevelDetailView_SetContent, &StandardLevelDetailView::SetContent, void, StandardLevelDetailView* self, IBeatmapLevel* level, BeatmapDifficulty defaultDifficulty, BeatmapCharacteristicSO* defaultBeatmapCharacteristic, PlayerData* playerData) {

    auto levelData = (BeatmapLevelData*) level->get_beatmapLevelData();
    // boy do I love it when interfaces are set to types that don't even inherit from them
    ArrayW<IDifficultyBeatmapSet*> originalSets(levelData->difficultyBeatmapSets);

    IDifficultyBeatmapSet* newSet360 = nullptr;
    IDifficultyBeatmapSet* newSet90 = nullptr;

    if (auto baseCharSet = GetCharacteristic(level, getConfig().BasedOn.GetValue())) {
        if (getConfig().Show360.GetValue()) {
            std::string name = getConfig().BasedOn.GetValue() + SUFFIX_360;
            if (auto preexisting = GetCharacteristic(level, name))
                newSet360 = preexisting;
            else {
                getLogger().info("Adding generated 360 degree level to standard detail view");
                newSet360 = CopyBeatmapSet(baseCharSet, level, GetCustomCharacteristic(name));
            }
        }
        if (getConfig().Show90.GetValue()) {
            std::string name = getConfig().BasedOn.GetValue() + SUFFIX_90;
            if (auto preexisting = GetCharacteristic(level, name))
                newSet90 = preexisting;
            else {
                getLogger().info("Adding generated 90 degree level to standard detail view");
                newSet90 = CopyBeatmapSet(baseCharSet, level, GetCustomCharacteristic(name));
            }
        }
    }
    if (newSet360 || newSet90) {
        auto newSets = List<IDifficultyBeatmapSet*>::New_ctor(originalSets.Length());
        for (auto& difficultySet : originalSets) {
            if (IsCustomCharacteristic(difficultySet->get_beatmapCharacteristic()))
                continue;
            newSets->Add(difficultySet);
        }
        if (newSet360)
            newSets->Add(newSet360);
        if (newSet90)
            newSets->Add(newSet90);
        // keep its underlying type consistent... T-T
        levelData->difficultyBeatmapSets = (System::Collections::Generic::IReadOnlyList_1<IDifficultyBeatmapSet*>*) newSets->ToArray().convert();
    }

    StandardLevelDetailView_SetContent(self, level, defaultDifficulty, defaultBeatmapCharacteristic, playerData);
}

bool startingGenerated360 = false;
bool startingGenerated90 = false;

#include "GlobalNamespace/SinglePlayerLevelSelectionFlowCoordinator.hpp"
#include "bs-utils/shared/utils.hpp"

MAKE_HOOK_MATCH(SinglePlayerLevelSelectionFlowCoordinator_StartLevel, &SinglePlayerLevelSelectionFlowCoordinator::StartLevel, void, SinglePlayerLevelSelectionFlowCoordinator* self, System::Action* beforeSceneSwitchCallback, bool practice) {

    StringW startingCharacteristic = self->get_selectedDifficultyBeatmap()->get_parentDifficultyBeatmapSet()->get_beatmapCharacteristic()->serializedName;
    startingGenerated360 = startingCharacteristic.ends_with(SUFFIX_360);
    startingGenerated90 = startingCharacteristic.ends_with(SUFFIX_90);

    // if ((startingGenerated360 || startingGenerated90) && !SettingsAreDefault(startingGenerated90))
    if (startingGenerated360 || startingGenerated90)
        bs_utils::Submission::disable(modInfo);
    else
        bs_utils::Submission::enable(modInfo);

    SinglePlayerLevelSelectionFlowCoordinator_StartLevel(self, beforeSceneSwitchCallback, practice);
}

#include "GlobalNamespace/BeatmapDataTransformHelper.hpp"
#include "GlobalNamespace/EnvironmentEffectsFilterPreset.hpp"

MAKE_HOOK_MATCH(BeatmapDataTransformHelper_CreateTransformedBeatmapData, &BeatmapDataTransformHelper::CreateTransformedBeatmapData,
        IReadonlyBeatmapData*, IReadonlyBeatmapData* beatmapData, IPreviewBeatmapLevel* beatmapLevel, GameplayModifiers* gameplayModifiers, bool leftHanded, EnvironmentEffectsFilterPreset environmentEffectsFilterPreset, EnvironmentIntensityReductionOptions* environmentIntensityReductionOptions, MainSettingsModelSO* mainSettingsModel) {

    auto ret = BeatmapDataTransformHelper_CreateTransformedBeatmapData(beatmapData, beatmapLevel, gameplayModifiers, leftHanded, environmentEffectsFilterPreset, environmentIntensityReductionOptions, mainSettingsModel);

    if (startingGenerated360 || startingGenerated90) {
        getLogger().info("Generating rotation events for Generated %s Degree mode", startingGenerated90 ? "90" : "360");

        ret = Generate(ret, beatmapLevel->get_beatsPerMinute(), startingGenerated90, leftHanded);
    }
    return ret;
}

extern "C" void setup(ModInfo& info) {
    info.id = MOD_ID;
    info.version = VERSION;
    modInfo = info;

    getLogger().info("Completed setup!");
}

#include "questui/shared/QuestUI.hpp"

extern "C" void load() {
    il2cpp_functions::Init();

    getConfig().Init(modInfo);

    QuestUI::Register::RegisterGameplaySetupMenu(modInfo, QuestUI::Register::MenuType::Solo, GameplaySetup);

    getLogger().info("Installing hooks...");
    INSTALL_HOOK(getLogger(), PlayerDataFileManagerSO_LoadFromCurrentVersion);
    INSTALL_HOOK(getLogger(), StandardLevelDetailView_SetContent);
    INSTALL_HOOK(getLogger(), SinglePlayerLevelSelectionFlowCoordinator_StartLevel);
    INSTALL_HOOK(getLogger(), BeatmapDataTransformHelper_CreateTransformedBeatmapData);
    getLogger().info("Installed all hooks!");
}
