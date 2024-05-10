LoadPlugin("x64\Debug\FilmGrain.dll")

OpenDMLSource("test.avi")

ConvertBits(16)
ConvertToYUV444()

#Spline64Resize(160*2,90*2)
Spline64Resize(160*3,90*3)

#FilmGrain(400, 0.1)
FilmGrainFast(0.75, grainsize=0.1)
#FilmGrainFast(0.5)

ConvertBits(8)

