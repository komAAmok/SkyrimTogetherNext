#pragma once

struct SubtitleEvent
{
    SubtitleEvent(uint32_t aSpeakerID, String aText)
        : SpeakerID(aSpeakerID)
        , Text(aText)
    {
    }

    SubtitleEvent(uint32_t aSpeakerID, String aText, uint32_t aTopicFormID)
        : SpeakerID(aSpeakerID)
        , Text(aText)
        , TopicFormID(aTopicFormID)
    {
    }

    uint32_t SpeakerID{};
    String Text{};
    // This is only relevant to fallout 4
    uint32_t TopicFormID{};
    // Scene the line is spoken in, 0 for player dialogue menu lines or lines outside scenes
    uint32_t SceneID{};
    // The scene's current phase when the line started
    uint32_t ScenePhase{};
};
