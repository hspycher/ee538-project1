#include <iostream>
#include <fstream>
#include <vector>
using namespace std;

/********************DO NOT EDIT**********************/
// Function prototype. Defined later.
void read_opinions(string filename); // reads file into opinions vector and updates total_nodes as needed
void read_edges(string filename); // reads file into edge_list, defined later
void build_adj_matrix(); // convert edge_list to adjacency matrix

int total_nodes = 0; // We keep track of the total number of nodes based on largest node id.


/****************************************************************/

/******** Create adjacency matrix and vector of opinions */
// simple vector to hold each node's opinion (0 or 1)
std::vector<int> opinions;

// global adjacency matrix initialized later
std::vector<std::vector<int>> adj;

// edge list: each row contains {source, target}
std::vector<std::vector<int>> edge_list;

void build_adj_matrix()
{
    // (1) allocate matrix adj of appropriate size
    adj.assign(total_nodes, std::vector<int>(total_nodes, 0));

    // (2) run through edge list and populate adj
    for(size_t i = 0; i < edge_list.size(); ++i)
    {
        int source = edge_list[i][0];
        int target = edge_list[i][1];
        
        // "0, 2 corresponds to: voter 0 influences voter 2"
        // Set adj[source][target] to 1 to denote this directional influence
        adj[source][target] = 1;
    }
}

double calculate_fraction_of_ones()
{
    // (3) Calculate the fraction of nodes with opinion 1 and return it.
    if(total_nodes == 0) return 0.0;
    
    int ones = 0;
    for(int i = 0; i < total_nodes; ++i)
    {
        if(opinions[i] == 1)
        {
            ones++;
        }
    }
    return static_cast<double>(ones) / total_nodes;
}

// For a given node, count majority opinion among its neighbours. Tie -> 0.
int get_majority_friend_opinions(int node)
{
    // (4) Count the number of neighbours with opinion 0 and opinion 1. 
    // Return the majority (0 or 1). If tie, return 0.
    int count_0 = 0;
    int count_1 = 0;
    
    for(int i = 0; i < total_nodes; ++i)
    {
        // If voter 'i' influences the current 'node'
        if(adj[i][node] == 1)
        {
            if(opinions[i] == 0) count_0++;
            else if(opinions[i] == 1) count_1++;
        }
    }
    
    if(count_1 > count_0)
    {
        return 1;
    }
    return 0; // Return 0 for both 0-majority and tie
}

// Calculate new opinions for all voters and return if anyone's opinion changed
bool update_opinions()
{
    // (5) For each node, calculate the majority opinion among its neighbours 
    // and update the node's opinion. 
    // Return true if any node's opinion changed, false otherwise.
    bool changed = false;
    
    // Create a temporary array to store the next state synchronously so 
    // updates don't cascade within the same simulation step
    std::vector<int> next_opinions(total_nodes);
    
    for(int i = 0; i < total_nodes; ++i)
    {
        next_opinions[i] = get_majority_friend_opinions(i);
        if(next_opinions[i] != opinions[i])
        {
            changed = true;
        }
    }
    
    opinions = next_opinions; // Commit the new states
    return changed;
}

int main() {
    // no preallocation; vectors grow on demand

    // Read input files
    read_opinions("opinions.txt"); 
    read_edges("edge_list.txt");

    // convert edge list into adjacency matrix once we know total_nodes
    build_adj_matrix();
    
    cout << "Total nodes: " << total_nodes << endl;
    
    // Run simulation
    int max_iterations = 30;
    int iteration = 0;
    bool opinions_changed = true;
    
    // Print initial state
    cout << "Iteration " << iteration << ": fraction of 1's = " 
         << calculate_fraction_of_ones() << endl;
    
    /// (6)  //////////////////////////////////////////////
    // Run until consensus or max iterations
    while(iteration < max_iterations)
    {
        opinions_changed = update_opinions();
        // If no opinions changed this round, break out immediately
        // so we don't count it as an extra iteration
        if(!opinions_changed) {
            break; 
        }
        iteration++;
        
        // Print the state of the system at regular intervals (e.g., every 10 iterations)
        if(opinions_changed)
        {
            cout << "Iteration " << iteration << ": fraction of 1's = " 
                 << calculate_fraction_of_ones() << endl;
        }
    }

    ////////////////////////////////////////////////////////
    // Print final result
    double final_fraction = calculate_fraction_of_ones();

    // RM redundant printing of final fraction since we already print it in the loop, but keeping it here for clarity
    // cout << "Iteration " << iteration << ": fraction of 1's = " 
    //     << final_fraction << endl;
    
    if(final_fraction == 1.0)
        cout << "Consensus reached: all 1's" << endl;
    else if(final_fraction == 0.0)
        cout << "Consensus reached: all 0's" << endl;
    else
        cout << "No consensus reached after " << iteration << " iterations" << endl;
    
    return 0;
}


/*********** Functions to read files **************************/ 

// Read opinion vector from file.
void read_opinions(string filename)
{
    ifstream file(filename);
    int id, opinion;
    while(file >> id >> opinion)
    {
        opinions.push_back(opinion);
        if(id >= total_nodes) total_nodes = id+1;
    }
    file.close();
}

// Read edge list from file and update total nodes as needed.
void read_edges(string filename)
{
    ifstream file(filename);
    int source, target;
    
    while(file >> source >> target)
    {
        edge_list.push_back({source, target});
        if(source >= total_nodes) total_nodes = source+1;
        if(target >= total_nodes) total_nodes = target+1;
    }
    file.close();
}

/********************************************************************** */