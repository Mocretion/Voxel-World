#include "PerlinNoise.h"
#include <iostream>

PerlinNoise::PerlinNoise(unsigned int seed)
{
    // Initialize permutation vector with values 0-255
    p.resize(512);

    // Fill p with values from 0 to 255
    std::iota(p.begin(), p.begin() + 256, 0);  // Like: for(int i = 0; i < 256; i++) p[i] = i;

    // Shuffle using the given seed
    std::default_random_engine engine(seed);
    std::shuffle(p.begin(), p.begin() + 256, engine);

    // Duplicate the permutation table to avoid overflow
    // This is like: for(int i = 0; i < 256; i++) p[256 + i] = p[i];
    std::copy(p.begin(), p.begin() + 256, p.begin() + 256);
}

float PerlinNoise::fade(float t) const
{
    // Smoothing function: 6t^5 - 15t^4 + 10t^3
    // This creates smooth transitions between values
    return t * t * t * (t * (t * 6 - 15) + 10);
}

float PerlinNoise::lerp(float a, float b, float t) const
{
    // Linear interpolation between a and b
    // Same as Mathf.Lerp in Unity/C#
    return a + t * (b - a);
}

float PerlinNoise::grad2D(int hash, float x, float y) const
{
    // Convert low 2 bits of hash to gradient direction
    int h = hash & 3;  // Same as: hash % 4

    // Select gradient vector based on hash
    switch (h)
    {
    case 0: return x + y;    // (1, 1)
    case 1: return x - y;    // (1, -1)
    case 2: return -x + y;   // (-1, 1)
    case 3: return -x - y;   // (-1, -1)
    default: return 0;       // Should never happen
    }
}

float PerlinNoise::grad3D(int hash, float x, float y, float z) const
{
    // Convert low 4 bits of hash to gradient direction
    int h = hash & 15;
    float u = h < 8 ? x : y;
    float v = h < 4 ? y : h == 12 || h == 14 ? x : z;
    return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
}

float PerlinNoise::noise2D(float x, float y) const
{
    // Find unit square that contains point
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;

    // Find relative x, y of point in square
    x -= std::floor(x);
    y -= std::floor(y);

    // Compute fade curves for x and y
    float u = fade(x);
    float v = fade(y);

    // Hash coordinates of the 4 square corners
    int A = p[X] + Y;
    int AA = p[A];
    int AB = p[A + 1];
    int B = p[X + 1] + Y;
    int BA = p[B];
    int BB = p[B + 1];

    // Blend the results from the 4 corners
    float res = lerp(
        lerp(grad2D(p[AA], x, y),
            grad2D(p[BA], x - 1, y),
            u),
        lerp(grad2D(p[AB], x, y - 1),
            grad2D(p[BB], x - 1, y - 1),
            u),
        v
    );

    // Return value in range [-1, 1]
    return res;
}

float PerlinNoise::noise3D(float x, float y, float z) const
{
    // Find unit cube that contains point
    int X = static_cast<int>(std::floor(x)) & 255;
    int Y = static_cast<int>(std::floor(y)) & 255;
    int Z = static_cast<int>(std::floor(z)) & 255;

    // Find relative x, y, z of point in cube
    x -= std::floor(x);
    y -= std::floor(y);
    z -= std::floor(z);

    // Compute fade curves
    float u = fade(x);
    float v = fade(y);
    float w = fade(z);

    // Hash coordinates of the 8 cube corners
    int A = p[X] + Y;
    int AA = p[A] + Z;
    int AB = p[A + 1] + Z;
    int B = p[X + 1] + Y;
    int BA = p[B] + Z;
    int BB = p[B + 1] + Z;

    // Blend results from 8 corners of cube
    float res = lerp(
        lerp(lerp(grad3D(p[AA], x, y, z),
            grad3D(p[BA], x - 1, y, z),
            u),
            lerp(grad3D(p[AB], x, y - 1, z),
                grad3D(p[BB], x - 1, y - 1, z),
                u),
            v),
        lerp(lerp(grad3D(p[AA + 1], x, y, z - 1),
            grad3D(p[BA + 1], x - 1, y, z - 1),
            u),
            lerp(grad3D(p[AB + 1], x, y - 1, z - 1),
                grad3D(p[BB + 1], x - 1, y - 1, z - 1),
                u),
            v),
        w
    );

    return res;
}

float PerlinNoise::octaveNoise2D(float x, float y, int octaves, float persistence, float scale) const
{
    float total = 0.0f;
    float frequency = scale;
    float amplitude = 1.0f;
    float maxValue = 0.0f;  // Used for normalizing result

    for (int i = 0; i < octaves; i++)
    {
        total += noise2D(x * frequency, y * frequency) * amplitude;

        maxValue += amplitude;

        amplitude *= persistence;
        frequency *= 2.0f;
    }

    // Normalize to [-1, 1]
    return total / maxValue;
}