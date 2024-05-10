//------------------------------------------------------------------------------
#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <math.h>
#include <avisynth.h>
#include <shlwapi.h>
#pragma comment(lib,"shlwapi.lib")
#include <shlobj.h>
#include <directxpackedvector.h>
#include <filesystem>
#include "film_grain_rendering.h"
//------------------------------------------------------------------------------
using namespace DirectX::PackedVector;
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
#define SAMPLE_SIZE 128
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
class CFilmGrainFast : public GenericVideoFilter
{
private:
	const int NumberOfGrainFrames = 25;
	float Strength;
	unsigned Algorithm = GRAIN_WISE;
	int Quality;
	float GrainSize;

	matrix<float>** GrainFrames = new matrix<float>*[NumberOfGrainFrames];
public:
	CFilmGrainFast(PClip clip, const float strength, const int quality, const float grainSize);
	~CFilmGrainFast();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;
	int __stdcall SetCacheHints(int cacheHints, int frame_range) override { return cacheHints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0; }
};
//------------------------------------------------------------------------------
CFilmGrainFast::CFilmGrainFast(PClip clip, const float strength, const int quality, const float grainSize) : GenericVideoFilter(clip), Strength(strength), Quality(quality), GrainSize(grainSize)
{
	if (!vi.Is444())
	{
		MessageBox(NULL, _T("At the moment the FilmGrainFast filter only supports YUV444 color formats."), _T("Error"), MB_OK | MB_ICONSTOP);
		return;
	}
	if (vi.BitsPerComponent() != 8 && vi.BitsPerComponent() != 16)
	{
		MessageBox(NULL, _T("At the moment the FilmGrainFast filter only supports 8 and 16 bit color formats."), _T("Error"), MB_OK | MB_ICONSTOP);
		return;
	}


	filmGrainOptionsStruct<float> filmGrainParams;

	filmGrainParams.muR = GrainSize; //0.8f;
	filmGrainParams.sigmaR = 0.0f;
	filmGrainParams.s = 1.0f; 	//zoom
	filmGrainParams.sigmaFilter = 0.8f;
	//number of monte carlo iterations
	filmGrainParams.NmonteCarlo = Quality; //800;
	filmGrainParams.xA = 0;
	filmGrainParams.yA = 0;
	filmGrainParams.xB = static_cast<float>(SAMPLE_SIZE);
	filmGrainParams.yB = static_cast<float>(SAMPLE_SIZE);
	filmGrainParams.mOut = static_cast<int>(floor(filmGrainParams.s * (filmGrainParams.yB - filmGrainParams.yA)));
	filmGrainParams.nOut = static_cast<int>(floor(filmGrainParams.s * (filmGrainParams.xB - filmGrainParams.xA)));

	noise_prng pSeedColour{};
	mysrand(&pSeedColour, static_cast<unsigned>(0));

	const auto imgIn = new matrix<float>(SAMPLE_SIZE, SAMPLE_SIZE);
	matrix<float>* imgOut = nullptr;

	filmGrainParams.grainSeed = myrand(&pSeedColour);

	for (int y = 0; y < SAMPLE_SIZE; y++)
	{
		for (int x = 0; x < SAMPLE_SIZE; x++)
		{
			imgIn->set_value(y, x, 0.5f);
		}
	}

	auto startTime = GetTickCount64();
	filmGrainParams.algorithmID = GRAIN_WISE;
	imgOut = film_grain_rendering_grain_wise(imgIn, filmGrainParams);
	auto endTime = GetTickCount64();
	delete imgOut;
	const auto grainTime = endTime - startTime;

	startTime = GetTickCount64();
	filmGrainParams.algorithmID = PIXEL_WISE;
	imgOut = film_grain_rendering_pixel_wise(imgIn, filmGrainParams);
	endTime = GetTickCount64();
	delete imgOut;
	const auto pixelTime = endTime - startTime;

	if (grainTime <= pixelTime)
	{
		Algorithm = GRAIN_WISE;
	}
	else
	{
		Algorithm = PIXEL_WISE;
	}

	delete imgIn;





	filmGrainParams.muR = GrainSize;
	filmGrainParams.sigmaR = 0.0f;
	filmGrainParams.s = 1.0f; 	//zoom
	filmGrainParams.sigmaFilter = 0.8f;
	//number of monte carlo iterations
	filmGrainParams.NmonteCarlo = Quality; // 800
	//name of the algorithm (grain-wise or pixel-wise)
	//filmGrainParams.algorithmID = PIXEL_WISE;
	//filmGrainParams.algorithmID = GRAIN_WISE;
	filmGrainParams.algorithmID = Algorithm;
	filmGrainParams.xA = 0;
	filmGrainParams.yA = 0;
	filmGrainParams.xB = static_cast<float>(vi.width);
	filmGrainParams.yB = static_cast<float>(vi.height);
	filmGrainParams.mOut = static_cast<int>(floor(filmGrainParams.s * (filmGrainParams.yB - filmGrainParams.yA)));
	filmGrainParams.nOut = static_cast<int>(floor(filmGrainParams.s * (filmGrainParams.xB - filmGrainParams.xA)));

	TCHAR cachePath[MAX_PATH];
	// Get path for each computer, non-user specific and non-roaming data.
	SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, cachePath);
	PathAppend(cachePath, _T("\\Cybercraft"));
	CreateDirectory(cachePath, nullptr);
	PathAppend(cachePath, _T("\\AviSynthFilmGrain"));
	CreateDirectory(cachePath, nullptr);
	PathAppend(cachePath, _T("\\GrainCache"));
	CreateDirectory(cachePath, nullptr);

	int maxWidth = vi.width;
	int maxHeight = vi.height;
	int bestWidth = vi.width;
	int bestHeight = vi.height;
	WCHAR largestCacheFile[MAX_PATH];
	int largestCacheFileSize = 0;
	bool largeEnoughCacheFileFound = false;
	for(auto &p : std::filesystem::directory_iterator(cachePath))
	{
		if (!p.is_directory())
		{
			continue;
		}

		auto path = std::filesystem::path(p);
		auto subFolderStr = path.filename().wstring();
		auto subFolder = subFolderStr.c_str();

		int width;
		int height;
		int nMonteCarlo;
		float muR;
		float sigmaR;
		float sigmaFilter;
		if (swscanf_s(subFolder, L"%ix%i_%i_%f_%f_%f", &width, &height, &nMonteCarlo, &muR, &sigmaR, &sigmaFilter) != 6)
		{
			continue;
		}

#define FEQUALS(a,b) (fabs((a) - (b)) < 0.00001f)

		if (nMonteCarlo == filmGrainParams.NmonteCarlo && FEQUALS(muR, filmGrainParams.muR) && FEQUALS(sigmaR, filmGrainParams.sigmaR) && FEQUALS(sigmaFilter, filmGrainParams.sigmaFilter))
		{
			if (width > maxWidth)
			{
				maxWidth = width;
			}
			if (height > maxHeight)
			{
				maxHeight = height;
			}

			if (width >= vi.width && height >= vi.height)
			{
				largeEnoughCacheFileFound = true;
				int cacheFileSize = width * height;
				if (cacheFileSize > largestCacheFileSize) {
					largestCacheFileSize = cacheFileSize;
					wcscpy_s(largestCacheFile, MAX_PATH, subFolder);
					bestWidth = width;
					bestHeight = height;
				}
			}
		}

#undef FEQUALS
	}

	if (!largeEnoughCacheFileFound)
	{
		bestWidth = maxWidth;
		bestHeight = maxHeight;
	}

	const auto BlankFrame = new matrix<float>(bestHeight, bestWidth);
	for (int y = 0; y < bestHeight; y++)
	{
		for (int x = 0; x < bestWidth; x++)
		{
			BlankFrame->set_value(y, x, 0.5f);
		}
	}

	auto halfBuffer = new HALF[bestWidth * bestHeight];
	auto floatBuffer = new float[bestWidth * bestHeight];
	for (int i = 0; i < NumberOfGrainFrames; ++i)
	{
		FILE* grainFile = nullptr;
		TCHAR framePath[MAX_PATH];
		_tcscpy_s(framePath, MAX_PATH, cachePath);

		if (largeEnoughCacheFileFound)
		{
			PathAppend(framePath, largestCacheFile);
			TCHAR frameName[MAX_PATH];
			_stprintf_s(frameName, MAX_PATH, _T("\\%04i.float16"), i);
			PathAppend(framePath, frameName);

			if (PathFileExists(framePath))
			{
				_tfopen_s(&grainFile, framePath, _T("rb"));
				fread(halfBuffer, 1, bestWidth * bestHeight * sizeof(HALF), grainFile);
				fclose(grainFile);
				XMConvertHalfToFloatStream(floatBuffer, sizeof(float), halfBuffer, sizeof(HALF), bestWidth * bestHeight);

				GrainFrames[i] = new matrix<float>(vi.height, vi.width);
				for (int y = 0; y < vi.height; ++y)
				{
					for (int x = 0; x < vi.width; ++x)
					{
						GrainFrames[i]->set_value(y, x, floatBuffer[y * bestWidth + x]);
					}
				}

				continue;
			}
		} else
		{
			TCHAR cacheName[MAX_PATH];
			_stprintf_s(cacheName, MAX_PATH, _T("\\%ix%i_%i_%f_%f_%f"), bestWidth, bestHeight, filmGrainParams.NmonteCarlo, filmGrainParams.muR, filmGrainParams.sigmaR, filmGrainParams.sigmaFilter);
			PathAppend(framePath, cacheName);
			CreateDirectory(framePath, nullptr);
			TCHAR frameName[MAX_PATH];
			_stprintf_s(frameName, MAX_PATH, _T("\\%04i.float16"), i);
			PathAppend(framePath, frameName);
		}

		noise_prng pSeedColour{};
		mysrand(&pSeedColour, static_cast<unsigned>(i));
		filmGrainParams.grainSeed = myrand(&pSeedColour);

		filmGrainParams.xB = static_cast<float>(bestWidth);
		filmGrainParams.yB = static_cast<float>(bestHeight);
		filmGrainParams.mOut = static_cast<int>(floor(filmGrainParams.s * (filmGrainParams.yB - filmGrainParams.yA)));
		filmGrainParams.nOut = static_cast<int>(floor(filmGrainParams.s * (filmGrainParams.xB - filmGrainParams.xA)));

		GrainFrames[i] = new matrix<float>(bestHeight, bestWidth);

		if (Algorithm == GRAIN_WISE) {
			GrainFrames[i] = film_grain_rendering_grain_wise(BlankFrame, filmGrainParams);
		}
		else
		{
			GrainFrames[i] = film_grain_rendering_pixel_wise(BlankFrame, filmGrainParams);
		}

		
		for (int y = 0; y < bestHeight; ++y)
		{
			for (int x = 0; x < bestWidth; ++x)
			{
				halfBuffer[y * bestWidth + x] = XMConvertFloatToHalf(GrainFrames[i]->get_value(y, x));
			}
		}

		_tfopen_s(&grainFile, framePath, _T("wb"));
		fwrite(halfBuffer, 1, bestWidth * bestHeight * sizeof(HALF), grainFile);
		fclose(grainFile);
	}
	delete[] floatBuffer;
	delete[] halfBuffer;
}
//------------------------------------------------------------------------------
CFilmGrainFast::~CFilmGrainFast()
{
	for (int i = NumberOfGrainFrames - 1; i >= 0; --i)
	{
		delete GrainFrames[i];
	}
	delete[] GrainFrames;
}
//------------------------------------------------------------------------------
PVideoFrame CFilmGrainFast::GetFrame(int n, IScriptEnvironment* env)
{
	PVideoFrame Src = child->GetFrame(n, env);
	PVideoFrame Dst = env->NewVideoFrame(vi);

	const int Width = vi.width;
	const int Height = vi.height;

	const int SrcPitchY = Src->GetPitch(PLANAR_Y);
	const int DstPitchY = Dst->GetPitch(PLANAR_Y);
	const int SrcPitchU = Src->GetPitch(PLANAR_U);
	const int DstPitchU = Dst->GetPitch(PLANAR_U);
	const int SrcPitchV = Src->GetPitch(PLANAR_V);
	const int DstPitchV = Dst->GetPitch(PLANAR_V);

	if (vi.BitsPerComponent() == 8) {
		unsigned char* __restrict pDst = Dst->GetWritePtr(PLANAR_Y);
		unsigned char* __restrict dstU = Dst->GetWritePtr(PLANAR_U);
		unsigned char* __restrict dstV = Dst->GetWritePtr(PLANAR_V);
		const unsigned char* __restrict pSrc = Src->GetReadPtr(PLANAR_Y);
		const unsigned char* __restrict srcU = Src->GetReadPtr(PLANAR_U);
		const unsigned char* __restrict srcV = Src->GetReadPtr(PLANAR_V);

		for (int y = 0; y < Height; y++)
		{
			for (int x = 0; x < Width; x++)
			{
				const float floatPixel = pSrc[y * SrcPitchY + x];
				const float floatNoise = GrainFrames[n % NumberOfGrainFrames]->get_value(y, x) - 0.5f;

				const int newPixel = static_cast<int>(floatPixel + floatNoise * Strength * 128.0f);
				const byte intPixel = newPixel < 0 ? 0 : (newPixel > 255 ? 255 : static_cast<byte>(newPixel));
				pDst[y * DstPitchY + x] = intPixel;
			}
		}

		for (int y = 0; y < Height; y++) {
			memcpy(dstU + y * DstPitchU, srcU + y * SrcPitchU, Width);
			memcpy(dstV + y * DstPitchV, srcV + y * SrcPitchV, Width);
		}
	}
	else
	{
		unsigned __int16* __restrict pDst = (unsigned __int16*)Dst->GetWritePtr(PLANAR_Y);
		unsigned __int16* __restrict dstU = (unsigned __int16*)Dst->GetWritePtr(PLANAR_U);
		unsigned __int16* __restrict dstV = (unsigned __int16*)Dst->GetWritePtr(PLANAR_V);
		const unsigned __int16* __restrict pSrc = (unsigned __int16*)Src->GetReadPtr(PLANAR_Y);
		const unsigned __int16* __restrict srcU = (unsigned __int16*)Src->GetReadPtr(PLANAR_U);
		const unsigned __int16* __restrict srcV = (unsigned __int16*)Src->GetReadPtr(PLANAR_V);

		for (int y = 0; y < Height; y++)
		{
			for (int x = 0; x < Width; x++)
			{
				const float floatPixel = pSrc[y * (SrcPitchY / 2) + x];
				const float floatNoise = GrainFrames[n % NumberOfGrainFrames]->get_value(y, x) - 0.5f;

				const int newPixel = static_cast<int>(floatPixel + floatNoise * Strength * 32768.0f);
				const unsigned short intPixel = newPixel < 0 ? 0 : (newPixel > 65535 ? 65535 : static_cast<unsigned short>(newPixel));
				pDst[y * (DstPitchY / 2) + x] = intPixel;
			}
		}

		for (int y = 0; y < Height; y++) {
			memcpy(dstU + y * (DstPitchU / 2), srcU + y * (SrcPitchU / 2), Width * 2);
			memcpy(dstV + y * (DstPitchV / 2), srcV + y * (SrcPitchV / 2), Width * 2);
		}
	}

	return Dst;
}
//------------------------------------------------------------------------------


//------------------------------------------------------------------------------
class CFilmGrain : public GenericVideoFilter 
{
private:
	int Quality;
	float GrainSize;
	unsigned Algorithm = GRAIN_WISE;
public: 
	CFilmGrain(PClip clip, const int quality, const float grainSize);
	~CFilmGrain();
	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;
	int __stdcall SetCacheHints(int cacheHints, int frame_range) override { return cacheHints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0; }
};
//------------------------------------------------------------------------------



//------------------------------------------------------------------------------
CFilmGrain::CFilmGrain(PClip clip, const int quality, const float grainSize) : GenericVideoFilter(clip), Quality(quality), GrainSize(grainSize)
{
	if (!vi.Is444())
	{
		MessageBox(NULL, _T("At the moment the FilmGrain filter only supports YUV444 color formats."), _T("Error"), MB_OK | MB_ICONSTOP);
		return;
	}
	if (vi.BitsPerComponent() != 8 && vi.BitsPerComponent() != 16)
	{
		MessageBox(NULL, _T("At the moment the FilmGrain filter only supports 8 or 16 bit color formats."), _T("Error"), MB_OK | MB_ICONSTOP);
		return;
	}



	filmGrainOptionsStruct<float> filmGrainParams;

	filmGrainParams.muR = GrainSize; //0.8f;
	filmGrainParams.sigmaR = 0.0f;
	filmGrainParams.s = 1.0f; 	//zoom
	filmGrainParams.sigmaFilter = 0.8f;
	//number of monte carlo iterations
	filmGrainParams.NmonteCarlo = Quality; // 800
	filmGrainParams.xA = 0;
	filmGrainParams.yA = 0;
	filmGrainParams.xB = static_cast<float>(SAMPLE_SIZE);
	filmGrainParams.yB = static_cast<float>(SAMPLE_SIZE);
	filmGrainParams.mOut = static_cast<int>(floor(filmGrainParams.s * (filmGrainParams.yB - filmGrainParams.yA)));
	filmGrainParams.nOut = static_cast<int>(floor(filmGrainParams.s * (filmGrainParams.xB - filmGrainParams.xA)));

	noise_prng pSeedColour{};
	mysrand(&pSeedColour, static_cast<unsigned>(0));

	const auto imgIn = new matrix<float>(SAMPLE_SIZE, SAMPLE_SIZE);
	imgIn->allocate_memory(SAMPLE_SIZE, SAMPLE_SIZE);

	matrix<float>* imgOut = nullptr;

	filmGrainParams.grainSeed = myrand(&pSeedColour);

	for (int y = 0; y < SAMPLE_SIZE; y++)
	{
		for (int x = 0; x < SAMPLE_SIZE; x++)
		{
			imgIn->set_value(y, x, 0.5f);
		}
	}

	auto startTime = GetTickCount64();
	filmGrainParams.algorithmID = GRAIN_WISE;
	imgOut = film_grain_rendering_grain_wise(imgIn, filmGrainParams);
	auto endTime = GetTickCount64();
	delete imgOut;
	const auto grainTime = endTime - startTime;

	startTime = GetTickCount64();
	filmGrainParams.algorithmID = PIXEL_WISE;
	imgOut = film_grain_rendering_pixel_wise(imgIn, filmGrainParams);
	endTime = GetTickCount64();
	delete imgOut;
	const auto pixelTime = endTime - startTime;

	if (grainTime <= pixelTime)
	{
		Algorithm = GRAIN_WISE;
	} else
	{
		Algorithm = PIXEL_WISE;
	}

	delete imgIn;
}
//------------------------------------------------------------------------------
CFilmGrain::~CFilmGrain()
{
}
//------------------------------------------------------------------------------
PVideoFrame __stdcall CFilmGrain::GetFrame(int n, IScriptEnvironment* env) 
{

	PVideoFrame Src = child->GetFrame(n, env);
	PVideoFrame Dst = env->NewVideoFrame(vi);

	const int Width = vi.width;
	const int Height = vi.height;

	const int SrcPitchY = Src->GetPitch(PLANAR_Y);
	const int DstPitchY = Dst->GetPitch(PLANAR_Y);
	const int SrcPitchU = Src->GetPitch(PLANAR_U);
	const int DstPitchU = Dst->GetPitch(PLANAR_U);
	const int SrcPitchV = Src->GetPitch(PLANAR_V);
	const int DstPitchV = Dst->GetPitch(PLANAR_V);

	filmGrainOptionsStruct<float> filmGrainParams;

	filmGrainParams.muR = GrainSize; //0.8f;
	filmGrainParams.sigmaR = 0.0f;
	filmGrainParams.s = 1.0f; 	//zoom
	filmGrainParams.sigmaFilter = 0.8f;
	//number of monte carlo iterations
	filmGrainParams.NmonteCarlo = Quality; // 800
	//name of the algorithm (grain-wise or pixel-wise)
	//filmGrainParams.algorithmID = PIXEL_WISE;
	//filmGrainParams.algorithmID = GRAIN_WISE;
	filmGrainParams.algorithmID = Algorithm;
	filmGrainParams.xA = 0;
	filmGrainParams.yA = 0;
	filmGrainParams.xB = static_cast<float>(Width);
	filmGrainParams.yB = static_cast<float>(Height);
	filmGrainParams.mOut = static_cast<int>(floor(filmGrainParams.s * (filmGrainParams.yB - filmGrainParams.yA)));
	filmGrainParams.nOut = static_cast<int>(floor(filmGrainParams.s * (filmGrainParams.xB - filmGrainParams.xA)));

	noise_prng pSeedColour{};
	mysrand(&pSeedColour, static_cast<unsigned>(n));

	const auto imgIn = new matrix<float>(Height, Width);
	imgIn->allocate_memory(Height, Width);

	matrix<float>* imgOut = nullptr;

	filmGrainParams.grainSeed = myrand(&pSeedColour);

	if (vi.BitsPerComponent() == 8)
	{
		unsigned char* __restrict pDst = Dst->GetWritePtr(PLANAR_Y);
		unsigned char* __restrict dstU = Dst->GetWritePtr(PLANAR_U);
		unsigned char* __restrict dstV = Dst->GetWritePtr(PLANAR_V);
		const unsigned char* __restrict pSrc = Src->GetReadPtr(PLANAR_Y);
		const unsigned char* __restrict srcU = Src->GetReadPtr(PLANAR_U);
		const unsigned char* __restrict srcV = Src->GetReadPtr(PLANAR_V);

		for (int y = 0; y < Height; y++)
		{
			for (int x = 0; x < Width; x++)
			{
				imgIn->set_value(y, x, pSrc[y * SrcPitchY + x]);
			}
		}

		imgIn->divide(static_cast<float>((MAX_GREY_LEVEL_8 + EPSILON_GREY_LEVEL)));

		if (filmGrainParams.algorithmID == GRAIN_WISE) {
			imgOut = film_grain_rendering_grain_wise(imgIn, filmGrainParams);
		} else
		{
			imgOut = film_grain_rendering_pixel_wise(imgIn, filmGrainParams);
		}

		imgOut->multiply(static_cast<float>((MAX_GREY_LEVEL_8 + EPSILON_GREY_LEVEL)));

		for (int y = 0; y < Height; y++)
		{
			for (int x = 0; x < Width; x++)
			{
				pDst[y * DstPitchY + x] = static_cast<unsigned char>((*imgOut)(y, x));
			}
		}

		for (int y = 0; y < Height; y++) {
			memcpy(dstU + y * DstPitchU, srcU + y * SrcPitchU, Width);
			memcpy(dstV + y * DstPitchV, srcV + y * SrcPitchV, Width);
		}
	} else if (vi.BitsPerComponent() == 16)
	{
		unsigned __int16* __restrict pDst = (unsigned __int16*)Dst->GetWritePtr(PLANAR_Y);
		unsigned __int16* __restrict dstU = (unsigned __int16*)Dst->GetWritePtr(PLANAR_U);
		unsigned __int16* __restrict dstV = (unsigned __int16*)Dst->GetWritePtr(PLANAR_V);
		const unsigned __int16* __restrict pSrc = (unsigned __int16*)Src->GetReadPtr(PLANAR_Y);
		const unsigned __int16* __restrict srcU = (unsigned __int16*)Src->GetReadPtr(PLANAR_U);
		const unsigned __int16* __restrict srcV = (unsigned __int16*)Src->GetReadPtr(PLANAR_V);

		for (int y = 0; y < Height; y++)
		{
			for (int x = 0; x < Width; x++)
			{
				imgIn->set_value(y, x, pSrc[y * (SrcPitchY / 2) + x]);
			}
		}

		imgIn->divide(static_cast<float>((MAX_GREY_LEVEL_16 + EPSILON_GREY_LEVEL)));

		if (filmGrainParams.algorithmID == GRAIN_WISE) {
			imgOut = film_grain_rendering_grain_wise(imgIn, filmGrainParams);
		}
		else
		{
			imgOut = film_grain_rendering_pixel_wise(imgIn, filmGrainParams);
		}

		imgOut->multiply(static_cast<float>((MAX_GREY_LEVEL_16 + EPSILON_GREY_LEVEL)));

		for (int y = 0; y < Height; y++)
		{
			for (int x = 0; x < Width; x++)
			{
				pDst[y * (DstPitchY / 2) + x] = static_cast<unsigned __int16>((*imgOut)(y, x));
			}
		}

		for (int y = 0; y < Height; y++) {
			memcpy(dstU + y * (DstPitchU / 2), srcU + y * (SrcPitchU / 2), Width * 2);
			memcpy(dstV + y * (DstPitchV / 2), srcV + y * (SrcPitchV / 2), Width * 2);

			/*for (int x = 0; x < Width; x++) {
				dstU[y * SrcPitchU / 2 + x] = srcU[y * SrcPitchU / 2 + x];
				dstV[y * SrcPitchV / 2 + x] = srcV[y * SrcPitchV / 2 + x];
			}*/
		}
	}
	
	delete imgOut;
	delete imgIn;
	
	return Dst;
}
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
AVSValue __cdecl Create_FilmGrain(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	return new CFilmGrain(args[0].AsClip(), args[1].AsInt(800), static_cast<float>(args[2].AsFloat(0.8f)));
}
//------------------------------------------------------------------------------
AVSValue __cdecl Create_FilmGrainFast(AVSValue args, void* user_data, IScriptEnvironment* env)
{
	return new CFilmGrainFast(args[0].AsClip(), static_cast<float>(args[1].AsFloat(1.0f)), args[2].AsInt(800), static_cast<float>(args[3].AsFloat(0.05f)));
}
//------------------------------------------------------------------------------
const AVS_Linkage* AVS_linkage = nullptr;
//------------------------------------------------------------------------------
extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors)
{
	AVS_linkage = vectors;
	env->AddFunction("FilmGrain", "c[quality]i[grainsize]f", Create_FilmGrain, nullptr);
	env->AddFunction("FilmGrainFast", "c[strength]f[quality]i[grainsize]f", Create_FilmGrainFast, nullptr);
	return "FilmGrain plugin"; 
}
//------------------------------------------------------------------------------


