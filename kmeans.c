#include "kmeans.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <errno.h>


// --------------------------- Core ---------------------------

ParsedData ReadFile(const char* path)
{
    // --------------------------- Open file and count lines ---------------------------
    
    // Input file pointer
    FILE* ifilePtr;
    
    ifilePtr = fopen(path, "r");

    if (!ifilePtr) {
        // strerror explains why (e.g. "No such file or directory")
        fprintf(stderr, "Resources: Could not open %s: %s\n", path, strerror(errno));

        // Return an empty result (data == NULL) so the caller can stop safely
        ParsedData empty = { NULL, 0, NUM_COLUMNS };
        return empty;
    }

    // Get the number of lines (same as num of rows) in file
    size_t numLines = 0;
    int ch, endsWithNewLine = 0;

    while ((ch = fgetc(ifilePtr)) != EOF) {
        endsWithNewLine = (ch == '\n');

        if (endsWithNewLine)
            ++numLines;
    }

    // To account for the last line
    ++numLines;
  
    // printf("\nNumber of lines in rest.txt: %zu\n\n", numLines);

    // --------------------------- Allocate memory for the data ---------------------------

    // Stores the data from file as a 2D array: [numLines, 2]
    float** data;

    // Allocate memory for the rows
    data = malloc(numLines * sizeof *data);

    // Display error if memory allocation fails for rows
    if (!data) {
        perror("Memory: allocation for rows failed\n");
        fclose(ifilePtr);
    }

    // Allocate for columns in a row
    for (size_t i = 0; i < numLines; ++i) {
        // Allocates bytes for entire column
        *(data + i) = malloc(NUM_COLUMNS * sizeof(float));

        if (!(*(data + i))) {
            fprintf(stderr, "Memory: columns allocation at row(%zu)  failed\n", i);
            fclose(ifilePtr);
        }
    }

    // --------------------------- Parse file content to 2D array ---------------------------
    
    // Rewind the file to read again from start
    rewind(ifilePtr);

    // Helps track active row
    size_t currRow = 0;

    // Read data from each line in input file
    while (currRow < numLines &&
        fscanf(ifilePtr, "%f %f", &data[currRow][0], &data[currRow][1]) == 2
    ) {
        // Set last column (cluster id) to -1 for starters
        data[currRow][2] = -1;
        // Move to next row
        ++currRow;
    }

    // Display data extracted from input file
#if defined(KC_LOG_ALL) || defined(KC_LOG_RF)
    printf("=============== Read File ===============\n");
    PrintArray(data, numLines, NUM_COLUMNS);
    printf("=========================================\n\n");
#endif

    // Close the file
    fclose(ifilePtr);

    ParsedData result;
    result.data = data;
    result.numRows = numLines;
    result.numCols = NUM_COLUMNS;

    // Return parsed data
    return result;
}

void WriteClustersToFile(const char* path, ParsedData* data)
{
    // Output file pointer
    FILE* ofilePtr;
    
    ofilePtr = fopen(path, "w");

    if (!ofilePtr) {
        fprintf(stderr, "Resources: Could not open %s\n", path);
        return;
    }

    // Text buffers for each formatted column (large enough for any float)
    char xStr[64], yStr[64], clusterIdStr[64];

    // TODO: Optimize to work with n-Dimensions data points.
    // Write each row to the output file
    for (size_t i = 0; i < data->numRows; ++i) {
        FormatFloat(xStr, sizeof xStr, data->data[i][0]);
        FormatFloat(yStr, sizeof yStr, data->data[i][1]);
        FormatFloat(clusterIdStr, sizeof clusterIdStr, data->data[i][2]);

        int written = fprintf(
            ofilePtr, "%s %s %s\n",
            xStr, yStr, clusterIdStr
        );

        if (written < 0) {
            fprintf(stderr, "Resources: Could not write row(%zu)\n", i);
            break;
        }
    }

    fclose(ofilePtr);
}

size_t GetClusterId(float* distanceArr, size_t* clusterCount)
{
    size_t minDistIndex = 0;

    for (size_t i = 0; i < *clusterCount; ++i) {

        if (distanceArr[i] < distanceArr[minDistIndex])
            minDistIndex = i;
    }
    
#if defined(KC_LOG_ALL) || defined(KC_LOG_GCI)
    printf("\n=========== Get Cluster ID ===========\n");
    printf("MinDist to current DP: %f ", distanceArr[minDistIndex]);
    printf("\n==========================================\n");
#endif

    return minDistIndex;
}

float** CreateAccumulator(
    size_t* clusterCount,
    size_t* numColsPerDP
) {
    // Allocate a row for each cluster
    float** accumulator = malloc(*clusterCount * sizeof *accumulator);

    if (!accumulator) {
        perror("CreateAccumulator: mem allocation for rows failed\n");
        return NULL;
    }

    // Allocate the columns for each cluster
    for (size_t i = 0; i < *clusterCount; ++i) {
        *(accumulator + i) = calloc(*numColsPerDP, sizeof(float));

        if (!(*(accumulator + i))) {
            fprintf(stderr, "CreateAccumulator: column mem allocation at row(%zu) failed\n", i);
            return NULL;
        }
           
        // Write each cluster ID to the first column
        // accumulator[i][0] = centroidPositions[i];
    }

#if defined(KC_LOG_ALL) || defined(KC_LOG_CA)
    printf("\n=========== Create Accumulator ===========\n");
    PrintArray(accumulator, *clusterCount, *numColsPerDP);
    printf("==========================================\n");
#endif

    return accumulator;
}

bool SelectRandomCentroids(size_t* centroidsPositions, size_t clusterCount, size_t numRows)
{
    // Holds every row index once; the front part gets shuffled into the picks
    size_t* rowIndices = malloc(numRows * sizeof *rowIndices);

    if (!rowIndices) {
        fprintf(stderr, "SelectRandomCentroids: mem allocation failed\n");
        return false;
    }

    for (size_t i = 0; i < numRows; ++i)
        rowIndices[i] = i;

    // Partial Fisher-Yates shuffle: swap a random not-yet-picked row into
    // slot i, so every row can be picked at most once (no duplicate centroids)
    for (size_t i = 0; i < clusterCount; ++i) {
        size_t j = (size_t)GetRandomNumber((int)i, (int)(numRows - 1));

        size_t temp = rowIndices[i];
        rowIndices[i] = rowIndices[j];
        rowIndices[j] = temp;

        centroidsPositions[i] = rowIndices[i];
    }

    free(rowIndices);
    return true;
}

float** GetCentroids(
    ParsedData* data,
    size_t* centroidsPositions, size_t* clusterCount
) {
    // Allocate rows
    float** values = malloc(*clusterCount * sizeof *values);

    if (!values) {
        perror("CreateAccumulator: mem allocation for rows failed\n");
        return NULL;
    }
        
    // Allocate columns for each row
    for (size_t i = 0; i < *clusterCount; ++i) {
        // Only allocate columns for the dimension of the data point i.e 2D, 3D, 4D...
        *(values + i) = malloc((data->numCols - 1) * sizeof(float));

        if (!(*(values + i))) {
            fprintf(stderr, "GetCentroids: column mem allocation at row(%zu) failed\n", i);
            return NULL;
        }
    }

    // Get the value for each centroid from data
    for (size_t i = 0; i < *clusterCount; ++i) {
        // Ignore last column since that is for cluster ID
        for (size_t j = 0; j < data->numCols - 1; ++j) {
            values[i][j] = data->data[centroidsPositions[i]][j];
        }
    }

#if defined(KC_LOG_ALL) || defined(KC_LOG_GC)
    printf("\n=========== GetCentroids ===========\n");
    PrintArray(values, *clusterCount, data->numCols - 1);
    printf("=========================================\n");
#endif

    return values;
}

void KmeansClustering(
    ParsedData* outData,
    size_t* centroidsPositions, size_t* clusterCount
) {
    float** centroids = GetCentroids(outData, centroidsPositions, clusterCount);
    if (!centroids) return;

    // Distance to centroids for each data point(Allocate once here for optimization)
    float* distToCentroids = malloc(*clusterCount * sizeof(float));
    if (!distToCentroids) return;

    // Main logic for Kmeans starts here
    for (size_t i = 0; i < 100; ++i) {
        // Will hold essential records each cluster
        float** accumulator = CreateAccumulator(clusterCount, &outData->numCols);
        if (!accumulator) return;
        
        // Iterate through each DP (data point)
        for (size_t j = 0; j < outData->numRows; ++j) {
            // Compute distance to centroids
            for (size_t k = 0; k < *clusterCount; ++k) {
                // Cache the distance result
                distToCentroids[k] = EucDistSq2D(
                    outData->data[j],
                    centroids[k]
                );
            }

            // Get the index for the closest cluster to DP
            size_t clusterId = GetClusterId(distToCentroids, clusterCount);

            // Update the DP's cluster ID column
            outData->data[j][outData->numCols - 1] = clusterId;

            // Update clusters' records in accumulator using the cluster ID
            for (size_t l = 0; l < outData->numCols; l++) {
                // On last column (which represents number of DP for a cluster)
                if (l == outData->numCols - 1)
                    // Increment number of data points in the cluster
                    accumulator[clusterId][l] += 1;
                else
                    // Build with current DP SumX, SumY and so forth
                    accumulator[clusterId][l] += outData->data[j][l];
            }
        }

        // Update centroids' values by getting the mean
        for (size_t m = 0; m < *clusterCount; ++m) {
            // Note: centroids match the data point's actual dimension
            for (size_t n = 0; n < (outData->numCols - 1); ++n) {
                // Divide column by number of related DP to cluster
                centroids[m][n] = accumulator[m][n] / accumulator[m][outData->numCols - 1];
            }
        }

        // Free memory before reset
        Deallocate2DArray(accumulator, *clusterCount);
    }

    Deallocate2DArray(centroids, *clusterCount);
    free(distToCentroids);
}

// --------------------------- Utilities ---------------------------

void DeallocateMemory(ParsedData* dataPtr)
{
    // Free up columns row by row
    for (size_t i = 0; i < dataPtr->numRows; ++i) {
        free(*(dataPtr->data + i));
    }

    // Free up rows
    free(dataPtr->data);

    // To prevent dangling pointer
    dataPtr->data = NULL;
}

void Deallocate2DArray(float** arr, size_t row)
{
    // Free allocated columns by row
    for (size_t i = 0; i < row; ++i) {
        free(*(arr + i));
    }

    // Finally, free the rows
    free(arr);
}

void PrintArray(float** someArr, size_t row, size_t col)
{
    for (size_t i = 0; i < row; ++i) {
        for (size_t j = 0; j < col; ++j) {
            // At starting col, add opening bracket
            if (j == 0) {
                printf("[");
            }

            printf("%f", someArr[i][j]);

            // Add commas, between columns except the last one
            if (j != (col - 1))
                printf(", ");

            // For last col, add closing bracket
            if (j == (col - 1))
                printf("]");
        }

        // For spacing
        printf("\n");
    }
}

void FormatFloat(char* buffer, size_t size, float value)
{
    // Round to at most 2 decimal places
    snprintf(buffer, size, "%.2f", value);

    // Nothing to chop if there is no decimal point (e.g. nan, inf)
    char* dotPos = strchr(buffer, '.');
    if (!dotPos)
        return;

    // Walk back from the end, removing trailing zeroes
    char* endPos = buffer + strlen(buffer) - 1;
    while (endPos > dotPos && *endPos == '0') {
        *endPos = '\0';
        --endPos;
    }

    // Remove the decimal point if no digits are left after it
    if (endPos == dotPos)
        *endPos = '\0';

    // Small negatives that round to zero would otherwise print as "-0"
    if (strcmp(buffer, "-0") == 0)
        strcpy(buffer, "0");
}

float EucDistSq2D(float* lhs, float* rhs)
{
    return pow((lhs[0] - rhs[0]), 2) + pow((lhs[1] - rhs[1]), 2);
}

int GetRandomNumber(int min, int max)
{
    // Ensure min is not greater than max (min == max simply returns min)
    if (min > max)
        return -1;

    // rand() may only reach 32767 (e.g. on Windows), so combine two calls
    // to cover ranges larger than RAND_MAX
    unsigned long long randomValue =
        (unsigned long long)rand() * ((unsigned long long)RAND_MAX + 1) + rand();
    unsigned long long rangeSize = (unsigned long long)((long long)max - min + 1);

    return (int)(randomValue % rangeSize) + min;
}