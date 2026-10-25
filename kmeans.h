#ifndef __BTH_KMEANS_H__
#define __BTH_KMEANS_H__

#include <stddef.h>
#include <stdbool.h>

// Activate for debugging logs
// #define KC_LOG_


// Pre-defined as seen in input file (cluster id for last column)
#define NUM_COLUMNS 3

typedef struct {
    float** data;
    size_t numRows;
    size_t numCols;
} ParsedData;

// --------------------------- Core ---------------------------

// Reads a text file and return a 2D array
ParsedData ReadFile(const char* path);
// Write data points with centroid IDs to file
void WriteClustersToFile(const char* path, ParsedData* data);

// Get minimum distance and return cluster id
size_t GetClusterId(float* distanceArr, size_t* clusterCount);
// Returns a 2D array, used to store the sum total of data points (DPs) in for each cluster
// in the order: [sumX, sumY, ..., relatedDPCount]
float** CreateAccumulator(
    size_t* clusterCount,
    size_t* numColsPerDP
);
// Fills centroidsPositions with clusterCount distinct random row indices
// in [0, numRows - 1]. Returns false if memory allocation fails.
bool SelectRandomCentroids(size_t* centroidsPositions, size_t clusterCount, size_t numRows);
// Returns an array of centroids points based on indices from original DPs
float** GetCentroids(
    ParsedData* data,
    size_t* centroidsPositions, size_t* clusterCount
);
// The clustering algorithm: returns the DPs and their cluster labels
void KmeansClustering(
    ParsedData* outData,
    size_t* centroidsPositions, size_t* clusterCount
);

// --------------------------- Utilities ---------------------------

// Free up allocated 2D memory
void Deallocate2DArray(float** arr, size_t rows);
// Free up allocated ParsedData memory
void DeallocateMemory(ParsedData* dataPtr);
// Displays the content of some 2D array to the console
void PrintArray(float** someArr, size_t row, size_t col);
// Writes value into buffer with at most 2 decimals and no trailing zeroes
void FormatFloat(char* buffer, size_t size, float value);
// Euclidean Distance Squared for 2D Only
float EucDistSq2D(float* lhs, float* rhs);
// Naively selects a random number between min and max
int GetRandomNumber(int min, int max);

#endif