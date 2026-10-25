#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "kmeans.h"

// Max characters accepted for a single line of user input
#define INPUT_BUFFER_SIZE 256
// Dataset used when no file path is passed on the command line
#define DEFAULT_INPUT_FILE "kmeans-data.txt"
// Where the clustering results are always written
#define OUTPUT_FILE "kmeans-output.txt"


// --------------------------- User input helpers ---------------------------

// Reads one line of user input into buffer, without the line ending.
// Returns false on end of input (EOF) or a read error.
static bool ReadInputLine(char* buffer, size_t size);
// Parses the whole text as a non-negative whole number.
// Rejects empty input, signs, decimals, letters and values that overflow.
static bool ParseSize(const char* text, size_t* outValue);
// Asks for the number of clusters until a value in [1, maxClusters] is given.
// Returns false if the input ends before a valid answer.
static bool PromptClusterCount(size_t maxClusters, size_t* outCount);
// Asks a Y/N question until y/Y or n/N is given (surrounding spaces allowed).
// Returns false if the input ends before a valid answer.
static bool PromptYesNo(const char* question, bool* outYes);
// Asks the user for each centroid's row (1-based as shown to the user) and
// stores it 0-based. Rejects out-of-range rows and rows already chosen,
// since two identical centroids would leave one cluster empty.
// Returns false if the input ends before all rows are given.
static bool PromptCentroidRows(size_t* centroidsPositions, size_t clusterCount, size_t numRows);


int main(int argc, char* argv[]) {
    // --------------------------- Select the input file ---------------------------

    // Usage: ./kmeans [data-file]  (only one optional argument is allowed)
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [data-file]\n", argv[0]);
        fprintf(stderr, "  data-file  path to the 2D points to cluster (default: %s)\n",
            DEFAULT_INPUT_FILE
        );
        return EXIT_FAILURE;
    }

    // Use the path given on the command line, otherwise the default dataset
    const char* inputPath = (argc == 2) ? argv[1] : DEFAULT_INPUT_FILE;

    // Seed the random number gen once
    srand(time(NULL));

    printf("Reading data points from %s\n", inputPath);
    ParsedData data = ReadFile(inputPath);

    // ReadFile has already reported why the file could not be read
    if (!data.data)
        return EXIT_FAILURE;

    // printf("============ main.c: Logging 2D array ============\n");
    // PrintArray(data.data, data.numRows, data.numCols - 1);
    // printf("==================================================\n");

    // Nothing to cluster without data points
    if (data.numRows == 0) {
        fprintf(stderr, "Resources: No data points found in %s\n", inputPath);
        DeallocateMemory(&data);
        return EXIT_FAILURE;
    }

    size_t numClusters;

    // --------------------------- Logic to get input from user ---------------------------

    if (!PromptClusterCount(data.numRows, &numClusters)) {
        DeallocateMemory(&data);
        return EXIT_FAILURE;
    }

    // Allocate memory to store selected centroid rows(points)
    size_t* centroidsPositions = malloc(numClusters * sizeof *centroidsPositions);

    if (!centroidsPositions) {
        fprintf(stderr, "Memory: allocation for centroid positions failed\n");
        DeallocateMemory(&data);
        return EXIT_FAILURE;
    }

    bool useRandom;
    bool inputOk = PromptYesNo("Do you want random initial centroids", &useRandom);

    if (inputOk) {
        if (useRandom)
            // Randomly select distinct centroid rows
            inputOk = SelectRandomCentroids(centroidsPositions, numClusters, data.numRows);
        else
            // Manually select centroids using cluster count
            inputOk = PromptCentroidRows(centroidsPositions, numClusters, data.numRows);
    }

    if (!inputOk) {
        free(centroidsPositions);
        DeallocateMemory(&data);
        return EXIT_FAILURE;
    }

    // Run Algorithm
    KmeansClustering(&data, centroidsPositions, &numClusters);
    // Write updates to new file
    WriteClustersToFile(OUTPUT_FILE, &data);

#ifdef KC_LOG_
    printf("============ main: Update Array With Clusters ============\n");
    PrintArray(data.data, data.numRows, data.numCols);
    printf("==========================================================\n");
#endif

    // Free up all dynamic memory
    free(centroidsPositions);
    DeallocateMemory(&data);

    return 0;
}


// --------------------------- User input helpers ---------------------------

static bool ReadInputLine(char* buffer, size_t size)
{
    if (!fgets(buffer, (int)size, stdin))
        return false;

    char* newlinePos = strchr(buffer, '\n');

    if (newlinePos) {
        *newlinePos = '\0';
    }
    else if (!feof(stdin)) {
        // Line was longer than the buffer: discard the rest so it is not
        // read as the next answer, and blank the buffer so it is rejected
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF)
            ;
        buffer[0] = '\0';
    }

    // Strip a carriage return left by Windows (CRLF) input
    size_t length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\r')
        buffer[length - 1] = '\0';

    return true;
}

static bool ParseSize(const char* text, size_t* outValue)
{
    // Skip leading whitespace
    while (isspace((unsigned char)*text))
        ++text;

    // strtoull silently accepts '-' and wraps it around, so require a digit
    if (!isdigit((unsigned char)*text))
        return false;

    errno = 0;
    char* endPtr;
    unsigned long long value = strtoull(text, &endPtr, 10);

    if (errno == ERANGE || value > SIZE_MAX)
        return false;

    // Only trailing whitespace is allowed after the number (e.g. "3abc" fails)
    while (isspace((unsigned char)*endPtr))
        ++endPtr;

    if (*endPtr != '\0')
        return false;

    *outValue = (size_t)value;
    return true;
}

static bool PromptClusterCount(size_t maxClusters, size_t* outCount)
{
    char input[INPUT_BUFFER_SIZE];

    while (true) {
        printf("Enter the number of clusters: ");
        fflush(stdout);

        if (!ReadInputLine(input, sizeof input)) {
            fprintf(stderr, "\nNo input received!\n");
            return false;
        }

        size_t value;

        if (!ParseSize(input, &value)) {
            fprintf(stderr, "Please enter a valid number.\n");
            continue;
        }

        // Ensure cluster count is within range: every point may be its own cluster
        if (value < 1 || value > maxClusters) {
            fprintf(stderr, "Enter a number between 1 and %zu!\n", maxClusters);
            continue;
        }

        *outCount = value;
        return true;
    }
}

static bool PromptYesNo(const char* question, bool* outYes)
{
    char input[INPUT_BUFFER_SIZE];

    while (true) {
        printf("%s (Y/N)? ", question);
        fflush(stdout);

        if (!ReadInputLine(input, sizeof input)) {
            fprintf(stderr, "\nNo input received!\n\n");
            return false;
        }

        // Find the first non-space character
        char* answer = input;
        while (isspace((unsigned char)*answer))
            ++answer;

        int lowerDecision = tolower((unsigned char)*answer);

        // Anything after the letter must be whitespace (so "yes" or "nope" fail)
        char* rest = (*answer != '\0') ? answer + 1 : answer;
        while (isspace((unsigned char)*rest))
            ++rest;

        if (*rest == '\0' && (lowerDecision == 'y' || lowerDecision == 'n')) {
            *outYes = (lowerDecision == 'y');
            return true;
        }

        fprintf(stderr, "\nYour response must be Y/y or N/n!\n\n");
    }
}

static bool PromptCentroidRows(size_t* centroidsPositions, size_t clusterCount, size_t numRows)
{
    char input[INPUT_BUFFER_SIZE];

    for (size_t i = 0; i < clusterCount; ++i) {
        printf("Select up centroid(%zu) from rows [1 - %zu]: ", i + 1, numRows);
        fflush(stdout);

        if (!ReadInputLine(input, sizeof input)) {
            fprintf(stderr, "\nNo input received!\n");
            return false;
        }

        size_t userRow;

        if (!ParseSize(input, &userRow)) {
            fprintf(stderr, "Please enter a valid number.\n");
            --i; // Ask for the same centroid again
            continue;
        }

        if (userRow < 1 || userRow > numRows) {
            fprintf(stderr, "Enter a number between 1 and %zu!\n", numRows);
            --i; // Ask for the same centroid again
            continue;
        }

        // Convert from the 1-based row shown to the user to a 0-based index
        size_t rowIndex = userRow - 1;

        // Reject a row that was already picked for an earlier centroid
        bool alreadyChosen = false;
        for (size_t j = 0; j < i; ++j) {
            if (centroidsPositions[j] == rowIndex) {
                alreadyChosen = true;
                break;
            }
        }

        if (alreadyChosen) {
            fprintf(stderr, "Row %zu is already a centroid, pick a different row!\n", userRow);
            --i; // Ask for the same centroid again
            continue;
        }

        centroidsPositions[i] = rowIndex;
    }

    return true;
}
