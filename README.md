# PUBG2019PS
Experimental private server for PUBG 2019

# LICENSE NOTICE
This project uses the AGPLv3 license. This means that you are NOT allowed, under ANY circumstaces, to make proprietary forks or host a public server without providing players with the source code.

# KEEP IN MIND
The project is still in an early alpha stage. It hitches a lot and has a few performance and memory safety issues.

# BOTS
Bots and their corresponding logic are included in this server build. They aren't perfect, but their brains get smarter every update :) To customize them, check the settings.ini config file.

# HOW TO PLAY
- Download PUBG 6.0.6.3 from Steam Depots. Open the steam console (Win + R -> steam://open/console) and use the following commands:
- Download content:  download_depot 578080 578081 6844732025739669729
- Download binaries:  download_depot 578080 578082 442406709355752115
- Compile the DLL, preferrably using Visual Studio 2022
- Place the settings.ini config file in the same folder as the compiled DLL
- Copy the contents of the Win64 folder in the repository to /TslGame/Binaries/Win64/ in the game files
- Run the batch file corresponding to the map you want to play
- Inject the DLL after the map has loaded

- Made with <3 by H4TIUX