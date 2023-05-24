#include "pch.h"
#include "draw_audio_controller_panel.h"
#include <string>

extern bool is_bass_working;
extern std::string audio_info;
extern bool repeat_audio = false;
extern float playback_speed = 1.0f;

extern LPFNBASSCHANNELFLAGS lpfnBassChannelFlags;
extern LPFNBASSCHANNELBYTES2SECONDS lpfnBassChannelBytes2Seconds;
extern LPFNBASSCHANNELGETLENGTH lpfnBassChannelGetLength;
extern LPFNBASSSTREAMGETFILEPOSITION lpfnBassStreamGetFilePosition;
extern LPFNBASSCHANNELGETINFO lpfnBassChannelGetInfo;
extern LPFNBASSCHANNELPLAY lpfnBassChannelPlay;
extern LPFNBASSCHANNELPAUSE lpfnBassChannelPause;
extern LPFNBASSCHANNELSTOP lpfnBassChannelStop;
extern LPFNBASSCHANNELSETPOSITION lpfnBassChannelSetPosition;
extern LPFNBASSCHANNELGETPOSITION lpfnBassChannelGetPosition;
extern LPFNBASSCHANNELSECONDS2BYTES lpfnBassChannelSeconds2Bytes;
extern LPFNBASSCHANNELSETATTRIBUTE lpfnBassChannelSetAttribute;

void draw_audio_controller_panel(HSTREAM streamHandle)
{
    if (is_bass_working)
    {
        // Get current position and duration in seconds.
        QWORD bytePos = lpfnBassChannelGetPosition(streamHandle, BASS_POS_BYTE);
        double currPosDouble = lpfnBassChannelBytes2Seconds(streamHandle, bytePos);
        QWORD byteLen = lpfnBassChannelGetLength(streamHandle, BASS_POS_BYTE);
        double totalDurDouble = lpfnBassChannelBytes2Seconds(streamHandle, byteLen);

        // Convert to float for ImGui functions
        float currPos = static_cast<float>(currPosDouble);
        float totalDur = static_cast<float>(totalDurDouble);

        ImGui::Begin("Audio Control");

        // Separate sections for better layout
        ImGui::Text("Track Information");
        ImGui::Separator();
        ImGui::Text("%s", audio_info.c_str());

        ImGui::Spacing();
        ImGui::Text("Playback Controls");
        ImGui::Separator();

        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 220) / 2); // Center buttons

        if (ImGui::Button("Play"))
        {
            lpfnBassChannelPlay(streamHandle, FALSE);
        }

        ImGui::SameLine();

        if (ImGui::Button("Pause"))
        {
            lpfnBassChannelPause(streamHandle);
        }

        ImGui::SameLine();

        if (ImGui::Button("Restart"))
        {
            lpfnBassChannelPlay(streamHandle, TRUE);
        }

        ImGui::SameLine();

        if (ImGui::Checkbox("Repeat", &repeat_audio))
        {
            if (repeat_audio)
            {
                // Turn on looping
                lpfnBassChannelFlags(streamHandle, BASS_SAMPLE_LOOP, BASS_SAMPLE_LOOP);
            }
            else
            {
                // Turn off looping
                lpfnBassChannelFlags(streamHandle, 0, BASS_SAMPLE_LOOP);
            }
        }

        ImGui::Spacing();
        ImGui::Text("Track Navigation");
        ImGui::Separator();

        // Allow modification of the current position using a slider
        auto label = std::format("{:02d}:{:02d} / {:02d}:{:02d}", static_cast<int>(currPos) / 60,
                                 static_cast<int>(currPos) % 60, static_cast<int>(totalDur) / 60,
                                 static_cast<int>(totalDur) % 60);

        if (ImGui::SliderFloat(label.c_str(), &currPos, 0.0f, totalDur))
        {
            // If the slider position has changed, update the audio position
            lpfnBassChannelSetPosition(
              streamHandle, lpfnBassChannelSeconds2Bytes(streamHandle, static_cast<double>(currPos)),
              BASS_POS_BYTE);
        }

        ImGui::Spacing();
        ImGui::Text("Playback Speed");
        ImGui::Separator();

        // New control for playback speed
        if (ImGui::SliderFloat("Speed", &playback_speed, 0.01f, 10.0f))
        {
            lpfnBassChannelSetAttribute(streamHandle, BASS_ATTRIB_TEMPO, (playback_speed - 1.0f) * 100.0f);
        }

        // Add tooltips for additional user information
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("Change the speed of the playback. 1.0 is normal speed.");
        }

        ImGui::End();
    }
}
