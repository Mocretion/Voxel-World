#pragma once

#include <vector>
#include <cmath>
#include <random>
#include <numeric>
#include <algorithm>

class PerlinNoise {
public:
    // Constructor with optional seed (like Random(seed) in C#)
    explicit PerlinNoise(unsigned int seed = 0);

    // Get noise value at 2D coordinates (returns value between -1 and 1)
    float noise2D(float x, float y) const;

    // Get noise value at 3D coordinates (if you need it later)
    float noise3D(float x, float y, float z) const;

    // Octave noise - combines multiple frequencies for more natural terrain
    // octaves: number of noise layers to combine
    // persistence: how much each octave contributes (usually 0.5)
    // scale: overall scale of the noise
    float octaveNoise2D(float x, float y, int octaves, float persistence, float scale) const;

private:
    // Permutation table - this is like a lookup table for randomness
    std::vector<int> p;  // In C#/Java this would be: int[] p = new int[512];

    // Helper functions
    float fade(float t) const;
    float lerp(float a, float b, float t) const;
    float grad2D(int hash, float x, float y) const;
    float grad3D(int hash, float x, float y, float z) const;
};