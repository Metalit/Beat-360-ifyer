// copied and adapted to C++ from https://github.com/CodeStix/Beat-360fyer-Plugin/blob/master/Beat-360fyer-Plugin/Generator360.cs

#include "main.hpp"
#include "config.hpp"
#include "generator.hpp"

#define CHECK_VAL(name) if (getConfig().name.GetValue() != getConfig().name.GetDefaultValue()) return false;

bool SettingsAreDefault(bool for90Degree) {
    CHECK_VAL(BasedOn);
    CHECK_VAL(PreferredBarDuration);
    if (for90Degree) {
        CHECK_VAL(LimitRotations360);
        CHECK_VAL(BottleneckRotations360);
    }
    else {
        CHECK_VAL(LimitRotations90);
        CHECK_VAL(BottleneckRotations90);
    }
    CHECK_VAL(EnableSpin);
    CHECK_VAL(TotalSpinTime);
    CHECK_VAL(SpinCooldown);
    CHECK_VAL(WallFrontCut);
    CHECK_VAL(WallBackCut);
    CHECK_VAL(MinWallDuration);
    CHECK_VAL(WallGenerator);
    CHECK_VAL(OnlyOneSaber);
    return true;
}

#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/NoteCutDirection.hpp"

#include "GlobalNamespace/BeatmapData.hpp"
#include "GlobalNamespace/SpawnRotationBeatmapEventData.hpp"
#include "System/Collections/Generic/LinkedList_1.hpp"
#include "GlobalNamespace/ObstacleData.hpp"
#include "GlobalNamespace/ColorType.hpp"
#include "GlobalNamespace/NoteLineLayer.hpp"

#include <queue>

using namespace GlobalNamespace;

int SoftFloor(float f) {
    int i = (int)f;
    return f - i >= 0.999 ? i + 1 : i;
}

std::pair<int, int> LeftAndRightCounts(std::vector<NoteData*> notes) {
    int leftCount = 0;
    int rightCount = 0;

    for (auto& note : notes) {
        auto dir = note->cutDirection;
        if (dir == NoteCutDirection::Left || dir == NoteCutDirection::UpLeft || dir == NoteCutDirection::DownLeft)
            leftCount++;
        else if (dir == NoteCutDirection::Right || dir == NoteCutDirection::UpRight || dir == NoteCutDirection::DownRight)
            rightCount++;
    }
    return { leftCount, rightCount };
}

IReadonlyBeatmapData* Generate(IReadonlyBeatmapData* base, float bpm, bool is90Degree, bool leftHanded) {
    auto data = base->GetCopy();
    auto items = data->get_allBeatmapDataItems();

    // TODO
    bool containsCustomWalls = false;

    // amount of rotation events emitted
    int eventCount = 0;
    // current rotation
    int totalRotation = 0;
    // moments where a wall should be cut
    std::vector<std::pair<float, int>> wallCutMoments{};
    // previous spin direction, false is left, true is right
    bool previousDirection = true;
    float previousSpinTime = -1;

    int rotLimit = is90Degree ? getConfig().LimitRotations90.GetValue() : getConfig().LimitRotations360.GetValue();

    auto Rotate = [&](float time, int amount, SpawnRotationBeatmapEventData::SpawnRotationEventType moment, bool enableLimit = true) {
        if (amount == 0)
            return;
        if (amount < -4)
            amount = -4;
        if (amount > 4)
            amount = 4;

        if (enableLimit) {
            if (totalRotation + amount > rotLimit)
                amount = std::min(amount, std::max(0, rotLimit - totalRotation));
            else if (totalRotation + amount < -rotLimit)
                amount = std::max(amount, std::min(0, -(rotLimit + totalRotation)));
            if (amount == 0)
                return;

            totalRotation += amount;
        }

        previousDirection = amount > 0;
        eventCount++;
        wallCutMoments.emplace_back(time, amount);

        data->InsertBeatmapEventDataInOrder(SpawnRotationBeatmapEventData::New_ctor(time, moment, amount * 15));
    };

    float beatDuration = 60 / bpm;
    float preferredDuration = getConfig().PreferredBarDuration.GetValue();

    // align beat duration to between 75% and 150% of preferred
    float barLength = beatDuration;
    while (barLength >= preferredDuration * 1.5)
        barLength /= 2;
    while (barLength < preferredDuration * 0.75)
        barLength *= 2;

    // filter the beatmap data to find all notes and walls (walls are used later)
    std::vector<NoteData*> notes{};
    std::vector<ObstacleData*> walls{};
    auto enumerator = items->GetEnumerator();
    while (enumerator.MoveNext()) {
        if (auto note = il2cpp_utils::try_cast<NoteData>(enumerator.current))
            notes.emplace_back(*note);
        if (auto wall = il2cpp_utils::try_cast<ObstacleData>(enumerator.current))
            walls.emplace_back(*wall);
    }
    std::vector<NoteData*> notesInBar{};
    std::vector<NoteData*> notesInBarBeat{};

    // align bars to first note, the first note (almost always) identifies the start of the first bar
    float firstBeatmapNoteTime = notes[0]->time;

    getLogger().info("Setup bpm=%.2f beatDuration=%.2f barLength=%.2f firstNoteTime=%.2f", bpm, beatDuration, barLength, firstBeatmapNoteTime);

    for (int i = 0; i < notes.size(); ) {
        // find the start and end of the current bar, discarding offset by using the first note
        float currentBarStart = SoftFloor((notes[i]->time - firstBeatmapNoteTime) / barLength) * barLength;
        float currentBarEnd = currentBarStart + barLength - 0.001;

        // get all the non bomb notes in the current bar
        notesInBar.clear();
        for (; i < notes.size() && notes[i]->time - firstBeatmapNoteTime < currentBarEnd; i++) {
            // not bomb
            if (notes[i]->cutDirection != NoteCutDirection::None)
                notesInBar.emplace_back(notes[i]);
        }

        // no rotations if no notes
        if (notesInBar.size() == 0)
            continue;

        // find if all the notes are basically at the same time, to determine if we do a spin
        bool allSameTime = true;
        for (auto& note : notesInBar) {
            if (abs(note->time - notesInBar[0]->time) >= 0.001)
                allSameTime = false;
        }

        // spin around if there are 2+ notes at the same time, respecting the cooldown
        if (getConfig().EnableSpin.GetValue() && notesInBar.size() >= 2 && currentBarStart - previousSpinTime > getConfig().SpinCooldown.GetValue() && allSameTime) {
            getLogger().info("Generator | Spin effect at %.2f", firstBeatmapNoteTime + currentBarStart);

            auto [leftCount, rightCount] = LeftAndRightCounts(notesInBar);

            // determine the spin direction based on which way the notes are pointing
            // continuing the last direction if they are equal
            int spinDirection;
            if (leftCount == rightCount)
                spinDirection = previousDirection ? -1 : 1;
            else if (leftCount > rightCount)
                spinDirection = -1;
            else
                spinDirection = 1;

            float spinStep = getConfig().TotalSpinTime.GetValue() / 24;
            for (int s = 0; s < 24; s++)
                Rotate(firstBeatmapNoteTime + currentBarStart + spinStep * s, spinDirection, SpawnRotationBeatmapEventData::SpawnRotationEventType::Early, false);

            // do not emit more rotation events after this
            previousSpinTime = currentBarStart;
            continue;
        }

        // divide the current bar in x pieces (or notes), for each piece, a rotation event CAN be emitted
        // calculated from the amount of notes in the current bar
        // barDivider | rotations
        // 0          | . . . . (no rotations)
        // 1          | r . . . (only on first beat)
        // 2          | r . r . (on first and third beat)
        // 4          | r r r r
        // 8          | rrrrrrrr
        // ...        | ...
        // TODO: create formula out of these if statements
        int barDivider;
        if (notesInBar.size() >= 58)
            barDivider = 0; // too many notes, do not rotate
        else if (notesInBar.size() >= 38)
            barDivider = 1;
        else if (notesInBar.size() >= 26)
            barDivider = 2;
        else if (notesInBar.size() >= 8)
            barDivider = 4;
        else
            barDivider = 8;

        if (barDivider <= 0)
            continue;

        std::stringstream debugStream;

        // iterate all the notes in the current bar in barDiviver pieces (bar is split in barDiviver pieces)
        float dividedBarLength = barLength / barDivider;
        for (int j = 0, k = 0; j < barDivider && k < notesInBar.size(); j++) {
            // find all the notes in the current division of the bar
            notesInBarBeat.clear();
            for (; k < notesInBar.size() && SoftFloor((notesInBar[k]->time - firstBeatmapNoteTime - currentBarStart) / dividedBarLength) == j; k++)
                notesInBarBeat.emplace_back(notesInBar[k]);

            if (j != 0)
                debugStream << ',';
            debugStream << notesInBarBeat.size();

            if (notesInBarBeat.size() == 0)
                continue;

            float currentBarBeatStart = firstBeatmapNoteTime + currentBarStart + j * dividedBarLength;

            // determine the rotation direction based on the last notes in the bar
            float lastNoteTime = notesInBarBeat.back()->time;
            std::vector<NoteData*> lastNotes{};
            for (auto& note : notesInBarBeat) {
                if (abs(note->time - lastNoteTime) < 0.005)
                    lastNotes.emplace_back(note);
            }

            // amount of notes pointing to the left/right of the last notes in the bar segment
            auto [leftCount, rightCount] = LeftAndRightCounts(lastNotes);

            // the next note after the bar segment
            NoteData* afterLastNote = (k < notesInBar.size() ? notesInBar[k] : i < notes.size() ? notes[i] : nullptr);

            // determine amount to rotate at once
            // TODO: create formula out of these if statements
            int rotationCount = 1;
            if (afterLastNote != nullptr) {
                float timeDiff = afterLastNote->time - lastNoteTime;
                // only rotate once if there is only one note in the current bar segment
                if (notesInBarBeat.size() >= 1) {
                    // rotate thrice if you have an entire bar to react
                    if (timeDiff >= barLength)
                        rotationCount = 3;
                    // rotate twice if you have an eighth of a bar to react
                    else if (timeDiff >= barLength / 8)
                        rotationCount = 2;
                }
            }

            int bottleneckRotations = is90Degree ? getConfig().BottleneckRotations90.GetValue() : getConfig().BottleneckRotations360.GetValue();

            int rotation = 0;
            // most of the notes at the end are pointing to the left, rotate to the left
            if (leftCount > rightCount)
                rotation = -rotationCount;
            // most of the notes at the end are pointing to the right, rotate to the right
            else if (rightCount > leftCount)
                rotation = rotationCount;
            // equal direction in the last notes of the bar
            else {
                // prefer rotating to the left if moved a lot to the right
                if (totalRotation >= bottleneckRotations)
                    rotation = -rotationCount;
                // prefer rotating to the right if moved a lot to the left
                else if (totalRotation <= -bottleneckRotations)
                    rotation = rotationCount;
                // rotate based on previous direction
                else
                    rotation = previousDirection ? rotationCount : -rotationCount;
            }

            // don't rotate more than once (15 degrees) if rotating the other direction is preferred
            if (totalRotation >= bottleneckRotations && rotationCount > 1)
                rotationCount = 1;
            else if (totalRotation <= -bottleneckRotations && rotationCount < -1)
                rotationCount = -1;

            // always rotate the other direction if past the rotation limit
            if (totalRotation >= rotLimit - 1 && rotationCount > 0)
                rotationCount = -rotationCount;
            else if (totalRotation <= -rotLimit + 1 && rotationCount < 0)
                rotationCount = -rotationCount;

            // finally rotate after the last note with the calculated values
            Rotate(lastNoteTime, rotation, SpawnRotationBeatmapEventData::SpawnRotationEventType::Late);

            // TODO: change to preserve parity
            if (getConfig().OnlyOneSaber.GetValue()) {
                for (auto& note : notesInBarBeat) {
                    // remove note
                    if (note->colorType == (rotation > 0 ? ColorType::ColorA : ColorType::ColorB))
                        items->Remove(note);
                    else {
                        // switch all notes to just one color
                        if (note->colorType == (leftHanded ? ColorType::ColorB : ColorType::ColorA))
                            note->Mirror(data->numberOfLines);
                    }
                }
            }

            // generate walls
            if (getConfig().WallGenerator.GetValue() && !containsCustomWalls) {
                float wallTime = currentBarBeatStart;
                float wallDuration = dividedBarLength;

                // check if there already is a wall
                bool generateWall = true;
                for (auto& wall : walls) {
                    if (wall->time + wall->duration >= wallTime && wall->time < wallTime + wallDuration) {
                        generateWall = false;
                        break;
                    }
                }

                if (generateWall && afterLastNote != nullptr) {
                    bool anyLine0 = false;
                    bool anyLine1 = false;
                    bool anyLine2 = false;
                    bool anyLine3 = false;
                    for (auto& note : notesInBarBeat) {
                        if (note->lineIndex == 0)
                            anyLine0 = true;
                        if (note->lineIndex == 1)
                            anyLine1 = true;
                        if (note->lineIndex == 2)
                            anyLine2 = true;
                        if (note->lineIndex == 3)
                            anyLine3 = true;
                        if (anyLine0 && anyLine1 && anyLine2 && anyLine3)
                            break;
                    }
                    if (!anyLine0) {
                        int wallHeight = anyLine1 ? 1 : 3;

                        if (afterLastNote->lineIndex == 0 && !(wallHeight == 1 && afterLastNote->noteLineLayer == NoteLineLayer::Base))
                            wallDuration = afterLastNote->time - getConfig().WallBackCut.GetValue() - wallTime;

                        if (wallDuration > getConfig().MinWallDuration.GetValue())
                            data->AddBeatmapObjectData(ObstacleData::New_ctor(wallTime, 0, wallHeight == 1 ? NoteLineLayer::Top : NoteLineLayer::Base, wallDuration, 1, wallHeight));
                    }
                    if (!anyLine3) {
                        int wallHeight = anyLine2 ? 1 : 3;

                        if (afterLastNote->lineIndex == 3 && !(wallHeight == 1 && afterLastNote->noteLineLayer == NoteLineLayer::Base))
                            wallDuration = afterLastNote->time - getConfig().WallBackCut.GetValue() - wallTime;

                        if (wallDuration > getConfig().MinWallDuration.GetValue())
                            data->AddBeatmapObjectDataInOrder(ObstacleData::New_ctor(wallTime, 3, wallHeight == 1 ? NoteLineLayer::Top : NoteLineLayer::Base, wallDuration, 1, wallHeight));
                    }
                }
            }

            getLogger().info("%.2f | Rotate %d (c=%lu, lc=%d, rc=%d, lastNotes=%lu, rotationTime=%.2f, afterLastNote=%.2f, rotc=%d)",
                currentBarBeatStart, rotation, notesInBarBeat.size(), leftCount, rightCount, lastNotes.size(), lastNoteTime + 0.01, afterLastNote ? afterLastNote->time : 0, rotationCount);
        }

        getLogger().info("%.2f (%.2f) -> %.2f(%.2f) | count=%lu segments=%s barDiviver=%d",
            currentBarStart + firstBeatmapNoteTime, (currentBarStart + firstBeatmapNoteTime) / beatDuration, currentBarEnd + firstBeatmapNoteTime, (currentBarEnd + firstBeatmapNoteTime) / beatDuration, notesInBar.size(), debugStream.str().c_str(), barDivider);
    }

    // cut walls, walls will be cut when a rotation event is emitted
    std::deque<ObstacleData*> wallQueue{walls.begin(), walls.end()};

    float wallFrontCut = getConfig().WallFrontCut.GetValue();
    float wallBackCut = getConfig().WallBackCut.GetValue();
    float minWallDur = getConfig().MinWallDuration.GetValue();

    while (wallQueue.size() > 0) {
        auto wall = wallQueue.front();
        wallQueue.pop_front();
        for (auto& [cutTime, cutAmount] : wallCutMoments) {
            if (wall->duration <= 0)
                break;

            // do not cut a margin around the wall if the wall is at a custom position
            bool isCustomWall = false;
            // if (wall->customData != null)
            //     isCustomWall = wall->customData.ContainsKey("_position");
            float frontCut = isCustomWall ? 0 : wallFrontCut;
            float backCut = isCustomWall ? 0 : wallBackCut;

            // walls with this criteria are not fun in 360, remove it
            if (!isCustomWall && (wall->lineIndex == 1 || wall->lineIndex == 2 || (wall->lineIndex == 0 && wall->width > 1)))
                items->Remove(wall);
            // ff moved in direction of wall
            else if (isCustomWall || (wall->lineIndex <= 1 && cutAmount < 0) || (wall->lineIndex >= 2 && cutAmount > 0)) {
                int cutMultiplier = abs(cutAmount);
                if (cutTime > wall->time - frontCut && cutTime < wall->time + wall->duration + backCut * cutMultiplier) {
                    float originalTime = wall->time;
                    float originalDuration = wall->duration;

                    // 225.431: 225.631(0.203476) -> 225.631() <|> 225.631(0.203476)
                    float firstPartTime = wall->time; // 225.631
                    float firstPartDuration = (cutTime - backCut * cutMultiplier) - firstPartTime; // -0.6499969
                    float secondPartTime = cutTime + frontCut; // 225.631
                    float secondPartDuration = (wall->time + wall->duration) - secondPartTime; //0.203476

                    // update duration of existing obstacle
                    if (firstPartDuration >= minWallDur && secondPartDuration >= minWallDur) {
                        wall->duration = firstPartDuration;

                        // And create a new obstacle after it
                        auto secondPart = ObstacleData::New_ctor(secondPartTime, wall->lineIndex, wall->lineLayer, secondPartDuration, wall->width, wall->height);
                        data->AddBeatmapObjectDataInOrder(secondPart);
                        wallQueue.emplace_back(secondPart);
                    }
                    // just update the existing obstacle, the second piece of the cut wall is too small
                    else if (firstPartDuration >= minWallDur)
                        wall->duration = firstPartDuration;
                    // Reuse the obstacle and use it as second part
                    else if (secondPartDuration >= minWallDur) {
                        if (secondPartTime != wall->time && secondPartDuration != wall->duration) {
                            wall->time = secondPartTime;
                            wall->duration = secondPartDuration;
                            wallQueue.emplace_back(wall);
                        }
                    }
                    // When this wall is cut, both pieces are too small, remove it
                    else
                        items->Remove(wall);

                    getLogger().info("Split wall at %.2f: %.2f(%.2f) -> %.2f(%.2f) <|> %.2f(%.2f) cutMultiplier=%d",
                        cutTime, originalTime, originalDuration, firstPartTime, firstPartDuration, secondPartTime, secondPartDuration, cutMultiplier);
                }
            }
        }
    }

    // remove bombs around cut walls
    for (auto& note : notes) {
        if (note->cutDirection != NoteCutDirection::None)
            continue;

        for (auto& [cutTime, cutAmount] : wallCutMoments) {
            if (note->time >= cutTime - wallFrontCut && note->time < cutTime + wallBackCut &&
                    ((note->lineIndex <= 2 && cutAmount < 0) || (note->lineIndex >= 1 && cutAmount > 0))) {
                items->Remove(note);
                break;
            }
        }
    }

    getLogger().info("Emitted %d rotation events", eventCount);

    return data->i_IReadonlyBeatmapData();
}
