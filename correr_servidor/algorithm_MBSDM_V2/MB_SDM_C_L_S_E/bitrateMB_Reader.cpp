#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "./json.hpp"

using json = nlohmann::json;


int requiredReach = 0;
int requiredSlots = 0;

struct Band {
    int slots = 0;
    int reach = 0;
};

struct Modulation {
    std::unordered_map<std::string, Band> bands;
};

struct Bitrate {
    std::unordered_map<std::string, Modulation> modulations;
};

std::map<int, Bitrate> bitratesJS;
 

std::map<int, Bitrate> readBitrates(const std::string& filename) {
    std::map<int, Bitrate> bitratesJS;
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open the file.");
    }
    json j;
    file >> j;

    // Iterate over the bitrates
    for (auto& [key, value] : j.items()) {
        int bitrate = std::stoi(key);
        Bitrate bitrateData;

        // Iterate over the modulations
        for (auto& modulationObj : value) {
            std::vector<std::pair<std::string, Modulation>> tempModulations;

            for (auto& [modKey, bands] : modulationObj.items()) {
                Modulation modulationData;
                std::vector<std::pair<std::string, Band>> tempBands;

                // Iterate over the bands
                for (auto& bandObj : bands) {
                    for (auto& [bandKey, bandData] : bandObj.items()) {
                        Band bandDetails;
                        bandDetails.slots = bandData["slots"];
                        bandDetails.reach = bandData["reach"];
                        tempBands.emplace_back(bandKey, bandDetails);
                    }
                }

                // Reverse the order of bands before storing in modulationData
                std::reverse(tempBands.begin(), tempBands.end());
                for (auto& bandPair : tempBands) {
                    modulationData.bands[bandPair.first] = bandPair.second;
                }

                // Store modulation in temporary vector
                tempModulations.emplace_back(modKey, modulationData);
            }

            // Reverse the order of modulations before storing in bitrateData
            std::reverse(tempModulations.begin(), tempModulations.end());
            for (auto& modPair : tempModulations) {
                bitrateData.modulations[modPair.first] = modPair.second;
            }
        }

        bitratesJS[bitrate] = bitrateData;
    }

    return bitratesJS;
}

int numberOfBitrates(std::map<int, Bitrate> bitrates){
    int numberOfBitrates = bitrates.size();
    return numberOfBitrates;
}

int numberOfModulations(std::map<int, Bitrate> bitrates){ //this is hard coded [10] beacuase all bitrates have the same number of modulations
    int numberOfModulations = bitrates[10].modulations.size();
    return numberOfModulations;
}

int numberOfBands(std::map<int, Bitrate> bitrates, int bitrate, std::string modulation){
    int numberOfBands = bitrates[bitrate].modulations[modulation].bands.size();
    return numberOfBands;
}

std::vector<std::string> modulationNames(std::map<int, Bitrate> bitrates){
    std::vector<std::string> modulationNames;
    for (auto& [key, value] : bitrates[10].modulations) {
        modulationNames.push_back(key);
    }
    return modulationNames;
}


std::string modulationString(int bitrate, int modulation){
    std::string modulationString = modulationNames(bitratesJS)[modulation];
    return modulationString;
}

int requiredReachPerBand(int bitrate, std::string modulation, std::string band){
    int requiredReach = bitratesJS[bitrate].modulations[modulation].bands[band].reach;
    return requiredReach;
}

int requiredslotsPerBand(int bitrate, std::string modulation, std::string band){
    int requiredSlots = bitratesJS[bitrate].modulations[modulation].bands[band].slots;
    return requiredSlots;
}



// int main() {
//     bitratesJS = readBitrates("./networks/bitratesMB.json");

//     std::vector<std::string> modNames = modulationNames(bitratesJS);

//     //for loop to print the names of the modulations
//     for (int i = 0; i < modNames.size(); i++) {
//         std::cout << modNames[i] << std::endl;
//     }

//     int bitrate = 1000;
//     std::string modulation = modNames[1];
//     std::string band = "C";
    


//     std::cout << "Number of bitrates: " << numberOfBitrates(bitratesJS) << std::endl;

//     std::cout << "Number of modulations: " << numberOfModulations(bitratesJS) << std::endl;

//     std::cout << "Number of bands: " << numberOfBands(bitratesJS, 40, modNames[0]) << std::endl;

//     std::cout << "name of the modulation: "<< modulation << " band: " << band <<" required slots: " << requiredslotsPerBand(bitrate, modulation, band) << 
//                 " required reach:" << requiredReachPerBand(bitrate, modulation, band) << std::endl;



//     return 0;
// }