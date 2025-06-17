# hardware
- esp32 wroom
- pcm5102
- sd card reader module

# software
- create a build script that installs all necessary dependencies
- create a flash script that flashes the firmware to the esp32

# implementation
- all implementation infos can be found in specs.md and wireing.md

# implementation pcm player
- esp should read the index.json file
- esp should open pcm files
- the pcm files contain all needed config information for the playback
- pcm should be sent to dac over i2c to establish audio playback
- the audio is sent with line level, no gain changes needed

# user interaction
- the user can press 3 buttons

# function
- 4 different modes
    - play all order
        - plays all files from the inxex.json in order
        - when the last song ends, start with the first song again
    - play all shuffle
        - plays all files from the index.json in a random order
    - play folder in order
        - plays all songs of one folder in order
        - when the last song ends it start with the first song of the folder again
    - play folder shuffled
        - plays all songs of a folder in a random order
- the modes are indicated with the neopixel, blinking once for half a second in a specific color
- after indicating the mode, the neopixel turns off
- the last mode is saved in a file on the sd card and reloaded after restart
- the current file playing is also saved and after restart it begins with that song
- the next button skips to the next song
- the back button starts the song over, when pressed again (under 5 seconds) the previous song is played
- the mode button cycles through the modes
- when the mode is one of the folder modes, a long press on the next button switches to the next folder and a long press on the back button switches to the previous folder
