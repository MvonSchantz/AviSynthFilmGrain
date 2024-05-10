LoadPlugin("x64\Release\FilmGrain.dll")

OpenDMLSource("test.avi")

ConvertBits(16)
ConvertToYUV444()

Spline64Resize(160*2,90*2)

#FilmGrain(400, 0.2)
FilmGrainFast(5, 400, 0.2)

ConvertBits(8)
