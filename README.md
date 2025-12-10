# Guild Wars Map Browser
Browse the Guild Wars .dat file. Included features:
- Search and filter the internal files in the dat.
- Render all maps (including maps not accesible in-game).
- Fly around and explore the maps in first person.
- Export and import full maps and models to Blender.
- View and export different model/mesh LODs (level of detail): High, Medium Low.
- Compare and switch between multiple .dat files easily.
- Give each file inside the .dat a custom name for easy reference. Or load the data provided by other users (shared through csv files).
- Hex editor viewer of files.
- Playback of audio files.
- Extract 3D models, textures, rendered maps and more.
- And more.

## How to use
- Acquire a Guild Wars .dat file. It is automatically downloaded to the same folder as Gw.exe when you first launch Guild Wars.
  You can download Guild Wars at: https://www.guildwars.com/en/download. It doesn't require an account.
- Download GuildWarsMapBrowser.exe from [releases](https://github.com/Jonathan-Greve/GuildWarsMapBrowser/releases) and run it.
- To import into Blender see the guide in the release notes or check [this reddit post](https://www.reddit.com/r/GuildWars/comments/17wnlj3/guild_wars_map_browser_v50_exporting_to_blender)

## Preview
 
 Here is an preview of Monastery Overlook as seen rendered in GuildWarsMapBrowser (with fog disabled):
![Monastery Overlook](images/v6_0_1_monastery_overlook.png)
and pre-Searing Ascalon City (with fog enabled):
![Ascalon City](images/v6_0_1_pre_ascalon_city.png)
And here is an example of rendering some models:
![A Golem](images/v5_0_view_model_golem.png)
![Lich](images/v5_0_view_model_lich.png)

And imported Pre-searing Ascalon City in Blender:
![Blender Ascalon City](images/v5_0_pre_ascalon_city_blender.png)
![Blender Ascalon City Solid](images/v5_0_pre_ascalon_city_blender_1.png)

Golem imported in Blender:
![Blender Golem](images/v5_0_view_model_golem_blender.png)

Viewing textures in Guild Wars Map Browser:
![Blender Golem](images/v5_0_view_texture_file.png)

Audio playback and controls:
![Blender Golem](images/v5_0_audio_playback.png)

Export all maps to png or dds in any resolution (up to 16384x16384):
![image](https://github.com/Jonathan-Greve/GuildWarsMapBrowser/assets/16852003/9abbeafe-18c8-4427-ab20-85b043cbefa9)

Select props and get info about them, hide/show them or cleanup maps to your preference before extracting to dds or png:
![image](https://github.com/Jonathan-Greve/GuildWarsMapBrowser/assets/16852003/899d97b9-2c11-49ee-8733-81b88d26329d)
![Lakeside_extract_before_after_cleanup](https://github.com/Jonathan-Greve/GuildWarsMapBrowser/assets/16852003/5d4c2980-4841-4747-84a3-0928492bd8ec)


## Building
To build just clone the repository and open the .Sln in Visual Studio. It's suggested to build in x86 mode. x64 was recently made possible but the executable runs slower than the 32 bit version

## Contributing
See *CONTRIBUTING.MD*

## Credits:
Decompressing the .dat file uses the source code from:
 - [GWDatBrowser](https://github.com/kytulendu/GWDatBrowser)
     - Specifically I use: AtexAsm.h/cpp, AtexReader.h/cpp, GWUnpacker.h/cpp, xentax.h/cpp
 - The Guild Wars community for being supportive, reporting bugs and showing interest in the project.
 - Thanks to [Dubble](https://github.com/DubbleClick) for rewriting some texture decoding functions from ASM to C++.
 - Thanks to [Laurent Dufresne](https://github.com/ldufr) for reverse engineering how GW computes the pathingmap from the .dat files.
