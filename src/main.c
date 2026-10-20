#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>

#include "kmeans.h"


int main() {
    // Seed the random number gen once
    srand(time(NULL));

    ParsedData data = ReadFile("resources/kmeans-data.txt");

    // printf("============ main.c: Logging 2D array ============\n");
    // PrintArray(data.data, data.numRows, data.numCols - 1);
    // printf("==================================================\n");

    size_t numClusters;
    size_t* centroidsPositions;

    // --------------------------- Logic to get input from user ---------------------------

    do {
        printf("Enter the number of clusters: ");
        int result = scanf("%zu", &numClusters);

        if (result == 1) {
            // Ensure cluster count is within range and not too large
            if (!(numClusters > 0 && numClusters <= (data.numRows/4))) {
                printf("Enter a number between 1 and %zu!\n", (data.numRows/4));
                continue;
            }
        }
        else if (result == EOF) {
            printf("\nNo input received!\n");
            continue;
        }
        else {
            printf("Please enter a valid number.\n");
            continue;
        }

        // Allocate memory to store selected centroid rows(points)
        centroidsPositions = malloc(numClusters * sizeof(size_t));

        while (true) {
            char decision;

            printf("Do you want random initial centroids (Y/N)? ");
            result = scanf(" %c", &decision);

            if (result == 1) {
                int lowerDecision = tolower((unsigned char)decision);

                if (lowerDecision == 'y') {
                    // Randomly select centroid points within cluster count
                    for (size_t i = 0; i < numClusters; ++i) {
                        centroidsPositions[i] = GetRandomNumber(0, data.numRows-1);
                    }

                    break;
                }
                else if (lowerDecision == 'n') {
                    // Manually select centroids using cluster count
                    size_t userCentroidPos;

                    for (size_t i = 0; i < numClusters; ++i) {
                        printf("Select up centroid(%zu) from rows [1 - %zu]: ",
                            i+1, data.numRows
                        );
                        result = scanf(" %zu", &userCentroidPos);

                        centroidsPositions[i] = userCentroidPos;
                    }

                    break;
                }
                else {
                    // For invalid entry, repeat
                    continue;
                }
            }
            else if (result == EOF) {
                printf("\nNo input received!\n\n");
                continue;
            } else {
                printf("\nYour response must be Y/y or N/n!\n\n");
                continue;
            }
        }

        // Terminate loop
        break;
    } while (true);

    // Run Algorithm
    KmeansClustering(&data, centroidsPositions, &numClusters);
    // Write updates to new file
    WriteClustersToFile("resources/kmeans-output.txt", &data);

    printf("============ main: Update Array With Clusters ============\n");
    PrintArray(data.data, data.numRows, data.numCols);
    printf("==========================================================\n");

    // Free up all dynamic memory
    free(centroidsPositions);
    DeallocateMemory(&data);

    return 0;
}