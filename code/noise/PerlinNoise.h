#pragma once

#include <vector>
#include <cmath>
#include <random>
#include <numeric>
#include <algorithm>
#include <FastNoise/FastNoise.h>

class PerlinNoise {
public:
    static FastNoise::SmartNode<FastNoise::FractalFBm> perlinWithFbm(float gain, float weightedStrngth, int octaves, float lacunarity);
};