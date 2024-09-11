#include <fnsim/simulator.hpp>


// Calculate Bandwidth blocking probability
double bandwidthBlockingProbability(double bitrateCountTotal[bitrateNumber], 
                                    double bitrateCountBlocked[bitrateNumber],
                                    double meanWeightBitrate[bitrateNumber])
    {
    double BBP = 0;
    double BP = 0;
    double total_weight = 0;

    for (int b = 0; b < bitrateNumber; b++){
        total_weight += meanWeightBitrate[b];
        if (bitrateCountTotal[b] == 0) continue;
        BP = bitrateCountBlocked[b] / bitrateCountTotal[b];
        BBP += meanWeightBitrate[b] * BP;
    }

    return (BBP/total_weight);
}