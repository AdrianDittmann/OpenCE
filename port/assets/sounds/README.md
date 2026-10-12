# Sounds

`cryo_one_through_sixteen.wav` is the "Cryo one through Sixteen" of Captain
Keyes' line in network co-op's cryo bay (`network.coop_cryo_bay`,
`port/linux/game/coop_cryo_line.c`). When a10 loads, the game puts it in
place of the "Cryo Two" of the map's own `A10_350` ("Bridge to Cryo Two"),
and adds the /s/ of the line's "send" to its "Master Chief". 16-bit mono PCM
at 22,050 Hz, 39,031 frames.

It holds no sound of the game's. Adrian Dittmann spoke the words, and
seed-vc (GPL 3.0) converted them toward Keyes' voice with a model fine-tuned
on his lines in the game; they were then matched to the line around them in
tempo, level and tone.
