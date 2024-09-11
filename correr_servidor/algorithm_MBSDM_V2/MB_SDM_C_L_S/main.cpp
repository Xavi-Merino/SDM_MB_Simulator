#include "./simulator.hpp"
#include <bits/stdc++.h>
#include "./bitrateMB_Reader.cpp"

#define EPSILON 1e-6

// ############################## Setup Global Variables #################################

std::vector<std::string> networks = {"NSFNet", "Eurocore", "UKnet"};
std::string currentNetwork = networks[2];
std::map<std::string, int> currentNetworkLink {{"NSFNet", 44},{"Eurocore", 50},{"UKnet", 78}};
std::map<std::string , int> slotPerBand {{"S", 760}, {"C",344}, {"L", 480}};

int numberConnections = 1e6; // (requests)

// Traffic loads
// double  lambdas[10] = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};

// Number of cores per link
int const number_of_cores = 7;

// Number of slots per core
int const SlotsPerCore = 1584;

// Total capacity network: (number of links * number of cores * NUMBER OF SLOTS PER CORE)
double totalCapacity = currentNetworkLink[currentNetwork] * number_of_cores * SlotsPerCore;

// (XT)
// coupling coefficient (k)
double k = 4e-4;
// bend radius (r)
double r = 5e-2;
// propagation constant (b)
double b = 4e6;
// core pitch (w)
double w = 4e-5;
// XT Threshold                               (BPSK |QPSK |8QAM |16QAM)
std::vector<double> XT_Threshold_by_bitrate = {-14, -18.5, -21, -25};


// ############################## Other global variables #################################

// Global access
Simulator sim;

// Cores adjacency matrix
int adjacencyMatrix[number_of_cores][number_of_cores];

// blocked by XT counter
int blocked_by_XT = 0;

// Resource utilization variables

int succesfulConnections = 0;
double currentUtilization = 0;
double maxUtilization = 0;
typedef std::unordered_map<std::string, double> modulationMap;

double maxRouteLength = 0;


modulationMap modulation_counter = {{"BPSK", 0}, {"QPSK", 0}, {"8QAM", 0}, {"16QAM", 0}};
modulationMap modulation_counter_mean = {{"BPSK", 0}, {"QPSK", 0}, {"8QAM", 0}, {"16QAM", 0}};


// maximum crosstalk
modulationMap maxXT_perModulation = {{"BPSK", -99}, {"QPSK", -99}, {"8QAM", -99}, {"16QAM", -99}};
double totalXT = 0;
double totalmeanXTdbm = 0;

// modulation used for each max XT
std::string modulationStringUsedInXT;
// ################################## Aux functions ######################################

// Power coupling coeficient
double pwrCouplingCoef(double k, double r, double b, double w)
{
    double numerator = 2 * (pow(k, 2.0)) * r;
    double denominator = b * w;
    double h = numerator / denominator;
    return h;
}

// Mean crosstalk
double meanCrosstalk(int n, double L, double k, double r, double b, double w)
{
    double h = pwrCouplingCoef(k, r, b, w);
    double numerator = n - n * (exp(-(n + 1) * 2 * h * L * 1000));
    double denominator = 1 + n * (exp(-(n + 1) * 2 * h * L * 1000));
    double XT = numerator / denominator;
    return (XT); 
    // std::cout << numerator << ", " << denominator << ", " << n << ", " << XT << "\n";
}

// Add edge into the adjacency matrix
void addEdge(int u, int v)
{
    adjacencyMatrix[u][v] = 1;
    adjacencyMatrix[v][u] = 1;
}

// Display core adjacency matrix
void displayMatrix(int number_of_cores)
{
    for (int i = 0; i < number_of_cores; i++)
    {
        for (int j = 0; j < number_of_cores; j++)
        {
            std::cout << adjacencyMatrix[i][j] << " ";
        }
        std::cout << std::endl;
    }
}

// Get neighbours (returns neighbours indexes of a given core)
std::vector<int> getNeighbours(int core, int number_of_cores)
{
    std::vector<int> neighbours;
    for (int n = 0; n < number_of_cores; n++)
    {
        if (core == n)
            continue;
        if (adjacencyMatrix[core][n] == 1)
            neighbours.push_back(n);
    }
    return neighbours;
}

// Returns true or false if current connection would be over Threshold
bool isOverThreshold(int coreIndex, std::vector<Link *> links, int reqSlots, int slotBegin, int routeLength, double XT_Threshold)
{
    std::vector<int> neighbours = getNeighbours(coreIndex, number_of_cores);
    int slotEnd = reqSlots + slotBegin - 1;
    int activeNeighboursCurrent = 0;
    int activeNeighbours = 0;
    double currentXT = 0;

    for (int l = 0; l < links.size(); l++) {
      for (int s = slotBegin; s < slotEnd; s++) {
        activeNeighboursCurrent = 0;
        for (int neighbour = 0; neighbour < neighbours.size(); neighbour++) { //recorremos los vecinos del core
          if (links[l]->getSlot(neighbours[neighbour], 0, s) == true) { //si el slot esta ocupado en el core vecino
            if (coreIndex != 6) { //todos los cores menos el del centro
              if (activeNeighbours < 3)
                activeNeighboursCurrent++;
                if (activeNeighboursCurrent > activeNeighbours)
                  activeNeighbours = activeNeighboursCurrent;
              else {
                break;
              }
            }
            else { // core central
              if (activeNeighbours < 6)
                activeNeighboursCurrent++;
                if (activeNeighboursCurrent > activeNeighbours)
                  activeNeighbours = activeNeighboursCurrent;
              else {
                  break;
              }
            }
            currentXT = meanCrosstalk(activeNeighbours, routeLength, k, r, b, w);
            totalXT += currentXT;
          }
        }
      }
    }
    // totalXT = meanCrosstalk(activeNeighbours, routeLength, k, r, b, w); // por que aca no se hace una sumatoria del crosstalk de cada slot ocupado por el vecino???
    totalmeanXTdbm = 10 * std::log10(totalXT);
    if (totalmeanXTdbm >  XT_Threshold + EPSILON) {
        return true;
    }
    return false;
}

// ############################## Metrics and results functions #################################
const int bitrateNumber=5;
std::vector<double> coreUtilization(number_of_cores, 0.0);

// BBP variables
double bitrateCountTotal[bitrateNumber] = {0.0, 0.0, 0.0, 0.0, 0.0};
double bitrateCountBlocked[bitrateNumber] = {0.0, 0.0, 0.0, 0.0, 0.0};
double meanWeightBitrate[bitrateNumber] = {1.0, 1.83, 3.5, 13.33, 32.83}; //DE DONDE SALEN ESTOS PESOS?????

// Result to TXT

void XTresultsToFile(std::fstream &output, double BBP, double BP, int lambda_index,
                    double erlang, double maxUtilization, double blocked_by_XT, modulationMap maxXT_perModulation,
                    double time_elapsed)
{
  output  << "erlang index: " << lambda_index
          << ", erlang: " << erlang
          << ", general blocking: " << BP
          << ", BBP: " << BBP
          << ", Total current utilization: " << maxUtilization
          << ", Blocked by XT: " << blocked_by_XT
          << ", Ruta mas larga: " << maxRouteLength
          << ", tiempo transcurrido: " << time_elapsed
          << ", conexiones exitosas: " << succesfulConnections;
          
  for (const auto& modulation : maxXT_perModulation) {
    output << ", Max XT for modulation " << modulation.first << ": " << modulation.second;
  }
  for (const auto& modulation : modulation_counter){
    output << ", conexiones exitosas con " << modulation.first << " :" << modulation.second;
    /* code */
  }
  

  
  output << std::endl;
}

std::map<float, int> bitRates_map {{ 10.0 , 0 }, { 40.0 , 1 }, { 100.0 , 2 }, { 400.0 , 3 }, {1000.0, 4}};


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

BEGIN_ALLOC_FUNCTION(MCMB_DA){
  bitratesJS = readBitrates("./networks/bitratesMB_C_L_S.json");
  int currentNumberSlots = 0;
  int currentSlotIndex = 0;
  int bitrateInt = bitRates_map[REQ_BITRATE];
  // Bitrate median(modificar si es esque se modifican los valores de bitrates)
  double bitRateMedian = 100;
  int reqReachPerBand = 0;
  int reqSlotsPerBand = 0;

  std::vector<std::string> orderOfBands;
  char band;
  int numberOfSlots = 0;
  std::vector<bool> totalSlots;



  bitrateCountTotal[bitrateInt] += 1;

  // Declared and assigned the range for each band
  std::map<std::string, std::pair<float,float>> bands = {{"S", std::make_pair(0, 759)}, {"C", std::make_pair(759, 1103)}, {"L", std::make_pair(1103, 1583)}};

  // Declared and assigned the order in which the bands will be used
  std::map<std::string , std::vector<std::string>> setOfBands = {{"set_1",{"L", "C", "S"}}, {"set_2",{"S", "C", "L"}}}; // Definimos el orden de las bandas en dos sets, el set 2 es un set de equilibrio
  



  if ( REQ_BITRATE >= bitRateMedian) {
    orderOfBands = setOfBands["set_2"];
  } else {
    orderOfBands = setOfBands["set_1"];
  }


  for(int r = 0 ; r < NUMBER_OF_ROUTES; r++){
    for(int c = 0 ; c < NUMBER_OF_CORES(r,0); c++){ 

      double routeLength = 0;

      /*The code initializes the totalSlots vector with false values. 
      The size of the vector is determined by the number of slots in the first link(0) 
      of the route.*/
      totalSlots = std::vector<bool>(LINK_IN_ROUTE(r, 0)->getSlots(), false);

      for(int l = 0 ; l < NUMBER_OF_LINKS(r); l++){ // recorremos los links de la ruta para obtener la distancia total de la ruta
        routeLength += LINK_IN_ROUTE(r,l)->getLength();
        
        if (maxRouteLength < routeLength) 
        maxRouteLength = routeLength; // esto para tener la mayor distancia de las rutas

        for (int s = 0; s < LINK_IN_ROUTE(r, l)->getSlots(); s++){ // actualizamos el vector totalSlots con la disponibilidad de los slots en cada link
        totalSlots[s] = totalSlots[s] | LINK_IN_ROUTE(r, l)->getSlot(c, 0, s);
        }
      }
      
      for (int b = 0; b < orderOfBands.size();b++){ // recorremos las bandas en el orden definido
        for (int m = numberOfModulations(bitratesJS)-1; m >= 0; m--) { // recorremos las modulaciones desde la mas eficiente a la menos eficiente
          currentNumberSlots = 0;
          int requiredBitrate = REQ_BITRATE;
          std::string modString = modulationString(REQ_BITRATE,m);
          reqReachPerBand = requiredReachPerBand(requiredBitrate, modString, orderOfBands[b]);
          reqSlotsPerBand = requiredslotsPerBand(requiredBitrate, modString, orderOfBands[b]);
          totalXT = 0;
          
          if (routeLength <= reqReachPerBand){ // verificamos si la distancia de la ruta es menor a la distancia maxima de la modulacion m en la banda b
              
            numberOfSlots = reqSlotsPerBand;
            currentSlotIndex = bands[orderOfBands[b]].first;

            for(int s = bands[orderOfBands[b]].first; s < bands[orderOfBands[b]].second; s++){ //buscamos los slots necesarios en las bandas utilizando FF
              if(totalSlots[s] == false){
                currentNumberSlots++;
                }
              else{
                currentNumberSlots = 0;
                currentSlotIndex = s+1;
              }
              if (currentNumberSlots >= numberOfSlots){
                double XT_Threshold = XT_Threshold_by_bitrate[m];
                // Check if crosstalk satisfied
                if (!isOverThreshold(c, sim.getPaths()->at(SRC)[DST][r], numberOfSlots, currentSlotIndex, routeLength, XT_Threshold)){
                  for (int l = 0; l < NUMBER_OF_LINKS(r); l++) {
                    ALLOC_SLOTS_SDM(LINK_IN_ROUTE_ID(r, l), c, 0, currentSlotIndex, numberOfSlots)
                  }

                  // UPDATE METRICS 
                  //max utilization
                  currentUtilization = currentUtilization + (numberOfSlots * NUMBER_OF_LINKS(r));
                  if (currentUtilization/totalCapacity > maxUtilization) maxUtilization = currentUtilization/totalCapacity;
                  
                  // esto para tener el mayor XT de cada modulación
                  if (totalmeanXTdbm > maxXT_perModulation[modString]) {
                    maxXT_perModulation[modString] = totalmeanXTdbm;
                    modulationStringUsedInXT = modString;
                    }
                  
                  // esto para contar la cantidad de veces que se ocupa una modulación
                  for(auto it = modulation_counter.begin(); it!=modulation_counter.end();it++){
                    if(it -> first == modString){
                      it -> second += 1;
                    }

                  }
                  
                  succesfulConnections += 1;
                  return ALLOCATED; 
                }
                else {
                  currentSlotIndex++;
                }
              }
            }
          }
          else{ // probamos con la siguiente modulación en la misma banda
            continue;
          }
        }
      }
    }
  }
  bitrateCountBlocked[bitrateInt] += 1;
  if (totalmeanXTdbm > XT_Threshold_by_bitrate[3] + EPSILON) { //chequeamos si fue bloqueado por XT
    blocked_by_XT++;
  }
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
// #############################################################################################

int main(int argc, char* argv[]) {

  std::fstream XTsimulationOutput;
  XTsimulationOutput.open(std::string("./out/resultados").append(currentNetwork).append(".txt"), std::ios::out | std::ios::app);
  XTsimulationOutput << "erlang index,erlang,general blocking,BBP,Total current utilization,Blocked by XT,Ruta mas larga,"
  <<"Max XT for modulation 16QAM,Max XT for modulation 8QAM,Max XT for modulation QPSK,Max XT for modulation BPSK,conexiones exitosas con 16QAM"
  <<",conexiones exitosas con 8QAM,conexiones exitosas con QPSK,conexiones exitosas con BPSK" << std::endl;
  XTsimulationOutput.close();

  // Set adjacent cores matrix
  addEdge(0, 1);
  addEdge(0, 5);
  addEdge(0, 6);
  addEdge(1, 2);
  addEdge(1, 6);
  addEdge(2, 3);
  addEdge(2, 6);
  addEdge(3, 4);
  addEdge(3, 6);
  addEdge(4, 5);
  addEdge(4, 6);
  addEdge(5, 6);
  displayMatrix(number_of_cores);

  // Simulation
  // int lambdas[100];
  // for (int i = 0; i < 100; i++) {
  // lambdas[i] = (i + 1) * 1000;
  // }
  for (int lambda = 54; lambda < 100; lambda++) {

    if (lambda == 0) continue;
    // Simulator object
    sim = Simulator(std::string("./networks/").append(currentNetwork).append(".json"), // Network nodes, links and cores
                    std::string("./networks/").append(currentNetwork).append("_routes.json"),  // Network Routes
                    std::string("./networks/bitratesSDM.json"),                    // BitRates (eg. BPSK)
                    SDM);                                                       // Network type (SDM, EON)
                                             

    // Sim parameters
    USE_ALLOC_FUNCTION(MCMB_DA, sim);

    USE_UNALLOC_FUNCTION_SDM(sim);
    sim.setGoalConnections(numberConnections);
    // sim.setLambda(100000);
    sim.setLambda(lambda * 1000);
    // sim.setSeedArrive(505);
    // sim.setSeedDeparture(505);
    sim.setMu(1);
    sim.init();

    // Run simulation
    std::clock_t c_start = std::clock();
    sim.run();
    std::clock_t c_end = std::clock();

    // CPU time
    double time_elapsed_ms = 1000.0 * (c_end-c_start) / CLOCKS_PER_SEC;


// ############################## Out results #################################
    std::string outputFileName = "./out/resultados";
  
    double BBP_results;
    double confidenceInterval =  sim.wilsonCI();
    BBP_results = bandwidthBlockingProbability(bitrateCountTotal, bitrateCountBlocked, meanWeightBitrate);
    currentUtilization = currentUtilization/totalCapacity;

    std::fstream XTsimulationOutput;
    XTsimulationOutput.open(std::string("./out/resultados").append(currentNetwork).append(".txt"), std::ios::out | std::ios::app);

    XTresultsToFile(XTsimulationOutput, BBP_results, sim.getBlockingProbability(), lambda, lambda * 1000, 
                    maxUtilization, blocked_by_XT, maxXT_perModulation, time_elapsed_ms);
    
// ############################## reset metric variables #################################

    for (int b = 0; b < 5; b++){
      bitrateCountTotal[b] = 0.0;
      bitrateCountBlocked[b] = 0.0;
    }
    for (auto it = modulation_counter.begin(); it != modulation_counter.end(); it++){
      it -> second = 0;
    }
    blocked_by_XT = 0;
    currentUtilization = 0;
    maxUtilization = 0; 
    succesfulConnections = 0;

  }
  return 0;
}