#include "PerlinNoise.h"
#include <iostream>

FastNoise::SmartNode<FastNoise::FractalFBm> PerlinNoise::perlinWithFbm(float gain, float weightedStrngth, int octaves, float lacunarity)
{
    auto fnPerlin = FastNoise::New<FastNoise::Perlin>(FastSIMD::CPUMaxSIMDLevel());
    //auto fnDS = FastNoise::New<FastNoise::DomainScale>();
    auto fnFractal = FastNoise::New<FastNoise::FractalFBm>(FastSIMD::CPUMaxSIMDLevel());

	//fnDS->SetSource(fnPerlin);
	//fnDS->SetScale(scale);

	//fnFractal->SetSource(fnDS);
	fnFractal->SetSource(fnPerlin);
	fnFractal->SetGain(gain);  // -2 to 2 (0 to 2)
    fnFractal->SetWeightedStrength(weightedStrngth);
    fnFractal->SetOctaveCount(octaves);
    fnFractal->SetLacunarity(lacunarity);

    return fnFractal;
}