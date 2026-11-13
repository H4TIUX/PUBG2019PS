PUBG 2019 zombie voices

The four PCM files are mono, 22050 Hz, signed 16-bit little-endian audio.
They were converted from the user-provided MP3 files in the project's
zombivoice directory. The client helper reads them from the ZombieVoice
subdirectory next to TslGame.exe. Distribute all four files to every client;
the server alone cannot make a remote client play these local sound files.

Playback is scoped to living human clients in a zombie match, with the
nearest live zombie less than 20 metres away. At most one clip plays at a
time; volume falls with distance and follows the game's master/effects
volume and mute settings. The stereo position follows the originating zombie
and the camera heading throughout the clip. This is not the game's native 3D
voice/occlusion system and does not resolve front-versus-back ambiguity.
