# Guild Wars Map Browser
Browse the Guild Wars .dat file. Included features:
- Searching and filtering the internal files in the dat.
- Rendering 3D terrains for all maps.
- Fly around in and explore the maps in first person.
- Export and import full maps and models to Blender.
- Hex editor viewer of files.
- Playback of audio files.
- Extract 3D models and textures.
- And more

## How to use
- You might need vc_redist_x86 (Microsoft Visual c++ Redistributable) that you can get [here](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170) or [direct download link](https://aka.ms/vs/17/release/vc_redist.x86.exe).
- Download GuildWarsMapBrowser.exe from [releases](https://github.com/Jonathan-Greve/GuildWarsMapBrowser/releases) and run it.
- To import into Blender see the guide in the release notes or check [this reddit post](https://www.reddit.com/r/GuildWars/comments/17wnlj3/guild_wars_map_browser_v50_exporting_to_blender)

## Preview
 
 Here is an preview of Monastery Overlook as seen rendered in GuildWarsMapBrowser and pre-searing Ascalon City
![Monastery Overlook](images/v5_0_monastery_overlook.png)
![Ascalon City](images/v5_0_pre_ascalon_city.png)
And here is an example of rendering some models:
![A Golem](images/v5_0_view_model_golem.png)
![Lich](images/v5_0_view_model_lich.png)

And imported Pre-searing Ascalon City in Blender:
![Blender Ascalon City](images/v5_0_pre_ascalon_city_blender.png)
![Blender Ascalon City Solid](images/v5_0_pre_ascalon_city_blender_1.png)

Golem imported in Blender
![Blender Golem](images/v5_0_view_model_golem_blender.png)

Viewing textures in Guild Wars Map Browser:
![Blender Golem](images/v5_0_view_texture_file.png)

Audio playback and controls
![Blender Golem](images/v5_0_audio_playback.png)

## Building
To build just clone the repository and open the .Sln in Visual Studio. Build in x86 mode (release or debug). Cannot build 64-bit.

## Contributing
See *CONTRIBUTING.MD*

## Credits:
Decompressing the .dat file uses the source code from:
 - [GWDatBrowser](https://github.com/kytulendu/GWDatBrowser)
     - Specifically I use: AtexAsm.h/cpp, AtexReader.h/cpp, GWUnpacker.h/cpp, xentax.h/cpp
 - The Guild Wars community for being supportive and showing interest in the project.
