# AviSynthFilmGrain
AviSynth filter for adding realistic film grain to videos using the algorithm described in "**Realistic Film Grain Rendering**" by *Alasdair Newson*, *Noura Faraj*, *Julie Delon* and *Bruno Galerne*.
Filter implementation copyright (c) 2022-2024  Mattias von Schantz.

The collection includes the following filters:

| Filter name   | Description                                     | Color support     |
|---------------|-------------------------------------------------|-------------------|
| FilmGrain	    | Realistic film grain simulation				          | YUV444 8 & 16 bit |
| FilmGrainFast	| Semi-realistic film grain simulation with cache	| YUV444 8 & 16 bit |

## FilmGrain

### Description
	
Realistic film grain filter for AviSynth. This is an implementation of the algorithm described in "**Realistic Film Grain Rendering**" by *Alasdair Newson*, *Noura Faraj*, *Julie Delon* and *Bruno Galerne*.

The algorithm is a Monte Carlo simulation which accurately render grain on video plates.

The filter is very slow and should only be used for very specific purposes on short clips.

### Syntax and parameters
	
	FilmGrain(int "quality", float "grainsize")

	int quality = 800
		The length of the Monte Carlo simulation.
		Higher values lead to more accurate renderings, but also takes more time.
		For larger grain sizes, higher values might be needed.

	float grainsize = 0.8
		The size of the simulated film grain.

### Examples

	FilmGrain(400, 0.2)



## FilmGrainFast

### Description
	
Semi-realistic film grain filter for AviSynth. This is an implementation of the algorithm described in "**Realistic Film Grain Rendering**" by *Alasdair Newson*, *Noura Faraj*, *Julie Delon* and *Bruno Galerne*, but unlike FilmGrain, it does not	simulate the grain of the actual video plate, but rather on a idealized gray plate. This gray grain plate is then cached and reused for future renderings.

The filter is very slow when first run, but subsequent runs will be much faster.

The cache is limited by resolution. For example, if a previous run has been made at 1080p and the next run is done at	2160p, the cache will have to be rewritten. However, the next time a 1080p video is being run, the filter will use a portion of the 2160p cache for the lower resolution.

The cache files can be found at *\<All users\>\\Cybercraft\\AviSynthFilmGrain\\GrainCache\\*

### Syntax and parameters
	
	FilmGrainFast(float "strength", int "quality", float "grainsize")

	float strength = 1.0
		The strength of the blending of the gray grain plate with the actual video frame.
		0 means no grain.
		Note: the strength can be larger than 1.0.

	int quality = 800
		The length of the Monte Carlo simulation.
		Higher values lead to more accurate renderings, but also takes more time.
		For larger grain sizes, higher values might be needed.

	float grainsize = 0.05
		The size of the simulated film grain.

### Examples

	FilmGrainFast(0.3, grainsize=0.2)

