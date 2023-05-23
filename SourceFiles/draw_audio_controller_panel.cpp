#include "pch.h"
#include "draw_audio_controller_panel.h"
#include <string>

extern bool is_bass_working;
extern std::string audio_info;

extern LPFNBASSCHANNELFLAGS lpfnBassChannelFlags;
extern LPFNBASSCHANNELBYTES2SECONDS lpfnBassChannelBytes2Seconds;
extern LPFNBASSCHANNELGETLENGTH lpfnBassChannelGetLength;
extern LPFNBASSSTREAMGETFILEPOSITION lpfnBassStreamGetFilePosition;
extern LPFNBASSCHANNELGETINFO lpfnBassChannelGetInfo;
extern LPFNBASSCHANNELPLAY lpfnBassChannelPlay;
extern LPFNBASSCHANNELPAUSE lpfnBassChannelPause;
extern LPFNBASSCHANNELSTOP lpfnBassChannelStop;

void draw_audio_controller_panel(HSTREAM streamHandle)
{
    if (is_bass_working)
    {
        // Keep the audio repeating. I.e. continue restarting it from the beginning when the audio finishes.
        static bool repeat = false;
        if (repeat)
            lpfnBassChannelFlags(streamHandle, BASS_SAMPLE_LOOP, -1);



        ImGui::Begin("Audio Control");

        ImGui::Text("%s", audio_info.c_str());

        if (ImGui::Button("Play"))
        {
            lpfnBassChannelPlay(streamHandle, TRUE);
        }

        if (ImGui::Button("Pause"))
        {
            lpfnBassChannelPause(streamHandle);
        }

        if (ImGui::Button("Stop"))
        {
            lpfnBassChannelStop(streamHandle);
        }

        ImGui::Checkbox("Repeat", &repeat);

        ImGui::End();
    }
}
