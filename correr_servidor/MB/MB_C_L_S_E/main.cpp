// This is already in functions.cpp
// #include <fstream>
// #include "./simulator.hpp"
#include "./aux_functions/functions.cpp"

// ############################## Setup Global Variables #################################

Simulator sim;
std::vector<std::string> networks = {"NSFNet", "Eurocore", "UKnet"};
std::string currentNetwork = networks[2];
std::map<std::string, int> currentNetworkLink {{"NSFNet", 44},{"Eurocore", 50},{"UKnet", 78}};


int numberConnections = 1e6; // (requests)
std::map<std::string, std::vector<char>> bandsPreference;

// total capacity of the network ( number of links * number of slots per link)
double totalCapacity = currentNetworkLink[currentNetwork] * 2720;
double currentUtilization = 0;
double maxUtilization = 0;

// Definition of allocation function
BEGIN_ALLOC_FUNCTION(MB_Alloc) {

  // Declare variables
  int currentNumberSlots;
  int currentSlotIndex;
  int numberOfSlots;
  int numberOfBands;
  char band;
  std::vector<char> bands;
  std::map<char, std::vector<bool>> totalSlots;
  // We define the order in which the bands will be used
  std::string orderOfBands;

  int bitrateInt = bitRates_map[REQ_BITRATE];

  int r=shorterIndex[std::make_pair(SRC, DST)][0]; // Shorter route index
  int minLength = shorterIndex[std::make_pair(SRC, DST)][1];  // Shorter route Length

  // Define the set to use
  if(minLength<median) orderOfBands="set_1";
  else orderOfBands="set_2";

  // Declare the order of the bands
  std::vector<char> bandsOrder = bandsPreference[orderOfBands];
 
  // Bitrate count
  bitrateCountTotal[bitrateInt] += 1;
  bands = VECTOR_OF_BANDS(r, 0); // The vector of bands (chars) for the first Link

  // Initialize the availability of slots for each band
  for (int bn = 0; bn < NUMBER_OF_BANDS(r, 0); bn++) {
    band = bands[bn];
    totalSlots[band] = std::vector<bool>(LINK_IN_ROUTE(r, 0)->getSlots(band), false);
  }

  // Iterate through links in the route
  for (int l = 0; l < NUMBER_OF_LINKS(r); l++) {
    bands = LINK_IN_ROUTE(r, l)->getBands();
    numberOfBands = LINK_IN_ROUTE(r, l)->getNumberOfBands();

    // Iterate through bands and slots within each link
    for (int bn = 0; bn < numberOfBands; bn++) {
      band = bands[bn];
      // 
      for (int s = 0; s < LINK_IN_ROUTE(r, l)->getSlots(band); s++) { // this loops through the slots on the current Band of the Link  to fill
                   // the total slots vector with the slot status information
        totalSlots[band][s] = totalSlots[band][s] | LINK_IN_ROUTE(r, l)->getSlot(s, band);
      }
    }
  }

  // Iterate through modulation
  for (int m = 0; m < NUMBER_OF_MODULATIONS; m++) {
    
    // get the position of the bands inside the modulation
    std::map<char, int> bandPos = REQ_POS_BANDS(m);

    // Iterate through bands
    for (int bo = 0; bo < bandsOrder.size(); bo++) {
      band = bandsOrder[bo]; //char of the current band
      int bandIndex = bandPos[band]; //index in the modulation of the current band  
      numberOfSlots = REQ_SLOTS_BDM(m, bandIndex);

      // Check if the route length exceeds the reach of the modulation scheme
      if (minLength > REQ_REACH_BDM(m, bandIndex)) continue;

      // Find consecutive free slots for allocation
      currentNumberSlots = 0;
      currentSlotIndex = 0;
      for (int s = 0; s < totalSlots[band].size(); s++) {
        if (totalSlots[band][s] == false) {
          currentNumberSlots++;
        } else {
          currentNumberSlots = 0;
          currentSlotIndex = s + 1;
        }
        if (currentNumberSlots == numberOfSlots) {
          // Allocate slots for the selected band
          for (int l = 0; l < NUMBER_OF_LINKS(r); l++) {
            ALLOC_SLOTS_BDM(LINK_IN_ROUTE_ID(r, l), band, currentSlotIndex, numberOfSlots)
          }

          // Update max utilization
          currentUtilization = currentUtilization + (numberOfSlots * NUMBER_OF_LINKS(r));
          if (currentUtilization/totalCapacity > maxUtilization) maxUtilization = currentUtilization/totalCapacity;

          return ALLOCATED;
        }
      }
    }
  }
  bitrateCountBlocked[bitrateInt] += 1;
  return NOT_ALLOCATED;
}
END_ALLOC_FUNCTION

// ############################## Unalloc callback function #################################

BEGIN_UNALLOC_CALLBACK_FUNCTION {
  
  // Store max utilization
  currentUtilization = currentUtilization - (c.getSlots()[0].size() * c.getLinks().size());
  if (currentUtilization/totalCapacity > maxUtilization) maxUtilization = currentUtilization/totalCapacity;

}
END_UNALLOC_CALLBACK_FUNCTION

int main(int argc, char* argv[]) {

  // Define band preferences
  bandsPreference["set_1"] = {'C', 'L', 'S', 'E'};
  bandsPreference["set_2"] = {'E', 'S', 'L', 'C'};
  int lambdas[100];
  for (int i = 0; i < 100; i++) {
    lambdas[i] = (i + 1) * 1000;
  }
  for (int lambda = 0; lambda < 100; lambda++) {
    sim = Simulator(std::string("./networks/").append(currentNetwork).append(".json"), 
                    std::string("./networks/").append(currentNetwork).append("_routes.json"),
                    std::string("./networks/bitrates.json"), BDM);

    // Sim parameters
    USE_ALLOC_FUNCTION(MB_Alloc, sim);
    USE_UNALLOC_FUNCTION_BDM(sim);

    sim.setGoalConnections(numberConnections);
    sim.setLambda(lambdas[lambda]);
    sim.setMu(1);
    sim.init();

    if(lambda==0) getMedian(sim.getPaths());

    sim.run();
    
    
// ############################## Out results #################################
    std::string outputFileName = "./out/resultados";

    std::fstream output;
    output.open(std::string("./out/resultados").append(currentNetwork).append(".txt"), std::ios::out | std::ios::app);

    double BBP_results;
    double confidenceInterval =  sim.wilsonCI();
    BBP_results = bandwidthBlockingProbability(bitrateCountTotal, bitrateCountBlocked, meanWeightBitrate);
    currentUtilization = currentUtilization/totalCapacity;

    resultsToFile(output, BBP_results, sim.getBlockingProbability(), confidenceInterval,
                lambda, lambdas[lambda], maxUtilization);
    for (int b = 0; b < bitrateNumber; b++){
      bitrateCountTotal[b] = 0.0;
      bitrateCountBlocked[b] = 0.0;
    }
    currentUtilization = 0;
    maxUtilization = 0;
  }
  return 0;
}
