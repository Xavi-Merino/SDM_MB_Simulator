#include <fstream>
#include "../simulator.hpp"


// ############################## Setup Global Variables #################################
int nodes=21;
std::map<std::pair<int, int>, std::vector<int>> shorterIndex;
double median;

std::string bitrateName;

const int bitrateNumber=5;

// BBP variables
double bitrateCountTotal[bitrateNumber] = {0.0, 0.0, 0.0, 0.0, 0.0};
double bitrateCountBlocked[bitrateNumber] = {0.0, 0.0, 0.0, 0.0, 0.0};
// Weight C+L+S+E:
double meanWeightBitrate[bitrateNumber] = {1.0, 1.83, 3.5, 13.33, 32.83};
// Bitrate map
std::map<float, int> bitRates_map {{ 10.0 , 0 }, { 40.0 , 1 }, { 100.0 , 2 }, { 400.0 , 3 }, {1000.0, 4}};
// Modulations
std::vector<std::string> modulations {"16QAM","8QAM", "QPSK", "BPSK"};

typedef std::unordered_map<std::string, double> modulationMap;
typedef std::unordered_map<std::string, modulationMap> modulationsPerBand;
modulationsPerBand modulations_per_band = {
  {"C", {{"BPSK", 0}, {"QPSK", 0}, {"8QAM", 0}, {"16QAM", 0}}},
  {"L", {{"BPSK", 0}, {"QPSK", 0}, {"8QAM", 0}, {"16QAM", 0}}},
  {"S", {{"BPSK", 0}, {"QPSK", 0}, {"8QAM", 0}, {"16QAM", 0}}},
  {"E", {{"BPSK", 0}, {"QPSK", 0}, {"8QAM", 0}, {"16QAM", 0}}}
};

// Traffic loads
// double  lambdas[100] = {1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000, 5500, 6000, 6500, 7000, 7500};

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

// Result to TXT
void resultsToFile(std::fstream &output, double BBP, double BP, double confidenceInterval, int lambda_index,double erlang, double maxUtilization)
{
  output<< "erlang index,erlang,general blocking,Total current utilization,BBP,Confidence interval\n"  
        << "erlang index: " << lambda_index
        << ", erlang: " << erlang
        << ", general blocking: " << BP
        << ", Total current utilization: " << maxUtilization
        << ", BBP: " << BBP
        << ", Confidence interval: " << confidenceInterval
        << '\n';
}

// Result to csv
void resultsToFilecsv(std::fstream &output, double BBP, double BP, int lambda_index,
                    double erlang, double maxUtilization)
{
  output  << lambda_index
          << "," << erlang
          << "," << BP
          << "," << BBP
          << "," << maxUtilization
;
          
  for (const auto& modulation : modulations_per_band){
    for (const auto& mod : modulation.second){
      output << "," << mod.second;
    }
  }
  output << std::endl;
}

// Function to compute the median of a vector of doubles
double computeMedian(std::vector<double>& datos) {
    std::sort(datos.begin(), datos.end()); // Sort the data in ascending order

    size_t n = datos.size(); // Get the number of elements in the vector

    if (n % 2 == 0) {
        size_t mitad = n / 2; // Calculate the index of the middle element for even-sized vector
        return (datos[mitad - 1] + datos[mitad]) / 2.0; // Calculate and return the median of two middle elements
    } else {
        return datos[n / 2]; // Return the middle element for odd-sized vector
    }
}

// Function to calculate median of total lengths in paths
void getMedian(std::vector<std::vector<std::vector<std::vector<Link *>>>> *paths){
  int minLength=0;
  int sri=0;
  std::vector<double> totalLength; // Create a vector to store total lengths

  // Loop through all pairs of nodes
  for (int i = 0; i < nodes; ++i) {
    for (int j = 0; j < nodes; ++j) {
      if(i==j) continue; // Skip if source and destination nodes are the same
      for (int r = 0; r < paths->at(i)[j].size(); ++r) {
        double length = 0;

        // Calculate total length for each link
        for (int l = 0; l < paths->at(i)[j][r].size(); l++) {
          length+=paths->at(i)[j][r][l]->getLength();
        }

        if (r == 0) {
          minLength = length; // Set initial minimum length
          sri = r; // Store the index of the shortest path
        }
        
        if (length < minLength) {
          minLength = length; // Update minimum length if a shorter path is found
          sri = r; // Update the index of the shortest path
        }
      }
      
      std::vector<int> sriPlusLength={sri,minLength}; // Create a vector with shortest path index and length
      totalLength.push_back(minLength); // Add the minimum length to the total lengths vector
      shorterIndex[std::make_pair(i, j)] = sriPlusLength; // Store the shortest path index and length in a map
    }
  }

  median = computeMedian(totalLength); // Calculate the median of the total lengths
}