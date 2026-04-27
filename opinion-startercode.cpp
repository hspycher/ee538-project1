#include <iostream>
#include <fstream>
#include <vector>
#include <utility>
#include <cstdint>
using namespace std;

/********************DO NOT EDIT**********************/
// Function prototypes. Defined later.
void read_opinions(string filename); // reads file into opinions vector and updates total_nodes as needed
void read_edges(string filename);    // reads file into a temporary edge buffer
void build_reverse_adj_csr();        // converts edge buffer into CSR reverse-adjacency

int total_nodes = 0; // We keep track of the total number of nodes based on largest node id.

// One byte per opinion -- 4x smaller than int, and vector<bool> is avoided
// because its proxy references make it slower in tight loops.
std::vector<uint8_t> opinions;

// Temporary buffer of edges, populated by read_edges and freed once
// build_reverse_adj_csr has consumed it. Kept compact via pair<int,int>
// (8 bytes) instead of vector<vector<int>> (~32 bytes per edge).
std::vector<std::pair<int,int>> edge_buffer;

// CSR reverse-adjacency. See header comment above.
std::vector<int> rev_off;
std::vector<int> rev_adj;

void build_reverse_adj_csr()
{
    // (1) Build CSR reverse-adjacency from edge_buffer in three linear passes.

    // Pass A: count the in-degree of each target node.
    // We store counts in rev_off[t+1] so the prefix-sum step below
    // produces the correct starting offsets directly.
    rev_off.assign(total_nodes + 1, 0);
    for (const auto& e : edge_buffer)
    {
        // edge.first influences edge.second  ==>  one in-edge to .second
        ++rev_off[e.second + 1];
    }

    // Pass B: prefix sum -- turn counts into starting offsets.
    for (int i = 1; i <= total_nodes; ++i)
    {
        rev_off[i] += rev_off[i - 1];
    }

    // Pass C: scatter sources into each target's slot.
    rev_adj.resize(edge_buffer.size());
    std::vector<int> cursor(rev_off.begin(), rev_off.begin() + total_nodes);
    for (const auto& e : edge_buffer)
    {
        rev_adj[cursor[e.second]++] = e.first;
    }

    // Free the edge buffer -- CSR replaces it from here on out.
    std::vector<std::pair<int,int>>().swap(edge_buffer);
}

double calculate_fraction_of_ones()
{
    // (3) Fraction of nodes whose opinion is 1.
    if (total_nodes == 0) return 0.0;

    // Opinions are 0 or 1, so a sum is the count of 1's.
    long long ones = 0;
    for (int i = 0; i < total_nodes; ++i)
    {
        ones += opinions[i];
    }
    return static_cast<double>(ones) / total_nodes;
}

// For a given node, count the majority opinion among its in-neighbors.
// Tie -> 0.
int get_majority_friend_opinions(int node)
{
    // (4) Walk only the in-neighbors of `node`. With CSR this is a
    //     contiguous slice of rev_adj -- O(in-degree(node)) and
    //     cache-friendly.
    const int begin = rev_off[node];
    const int end   = rev_off[node + 1];

    // Opinions are 0 or 1, so summing them gives count_1 directly.
    // count_0 = total - count_1, no second accumulator needed.
    int count_1 = 0;
    for (int k = begin; k < end; ++k)
    {
        count_1 += opinions[rev_adj[k]];
    }
    const int total = end - begin;
    const int count_0 = total - count_1;

    return (count_1 > count_0) ? 1 : 0; // tie -> 0
}

// Calculate new opinions for all voters and return whether anyone changed.
bool update_opinions()
{
    // (5) Synchronous update: write into a parallel buffer so updates
    //     within the same step don't cascade. swap() commits in O(1).
    bool changed = false;
    std::vector<uint8_t> next_opinions(total_nodes);

    for (int i = 0; i < total_nodes; ++i)
    {
        const uint8_t next = static_cast<uint8_t>(get_majority_friend_opinions(i));
        next_opinions[i] = next;
        if (next != opinions[i]) changed = true;
    }

    opinions.swap(next_opinions); // O(1) -- no per-element copy.
    return changed;
}

int main() {
    // Faster iostream: untie cin from cout and skip C-stdio sync.
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    // Read input files.
    read_opinions("opinions.txt");
    read_edges("edge_list.txt");

    // If edges reference voters not listed in opinions.txt, treat them
    // as opinion 0 by default.
    if (static_cast<int>(opinions.size()) < total_nodes)
        opinions.resize(total_nodes, 0);

    // Build CSR reverse-adjacency once total_nodes is known.
    build_reverse_adj_csr();

    cout << "Total nodes: " << total_nodes << endl;
    cout << "Total edges: " << rev_adj.size() << endl;

    // Run simulation.
    int max_iterations = 30;
    int iteration = 0;
    bool opinions_changed = true;

    cout << "Iteration " << iteration << ": fraction of 1's = "
         << calculate_fraction_of_ones() << endl;

    /// (6)  //////////////////////////////////////////////
    while (iteration < max_iterations)
    {
        opinions_changed = update_opinions();
        if (!opinions_changed) break; // consensus reached, don't count this round
        ++iteration;
        cout << "Iteration " << iteration << ": fraction of 1's = "
             << calculate_fraction_of_ones() << endl;
    }
    ////////////////////////////////////////////////////////

    double final_fraction = calculate_fraction_of_ones();
    if (final_fraction == 1.0)
        cout << "Consensus reached: all 1's" << endl;
    else if (final_fraction == 0.0)
        cout << "Consensus reached: all 0's" << endl;
    else
        cout << "No consensus reached after " << iteration << " iterations" << endl;

    return 0;
}


/*********** Functions to read files **************************/

// Read opinion vector from file. Tolerates non-contiguous IDs; missing
// IDs default to opinion 0.
void read_opinions(string filename)
{
    ifstream file(filename);
    int id, opinion;
    while (file >> id >> opinion)
    {
        if (id >= static_cast<int>(opinions.size()))
            opinions.resize(id + 1, 0);
        opinions[id] = static_cast<uint8_t>(opinion);
        if (id + 1 > total_nodes) total_nodes = id + 1;
    }
}

// Read edge list from file and update total_nodes as needed.
// Edges land in edge_buffer; build_reverse_adj_csr converts them to CSR.
void read_edges(string filename)
{
    ifstream file(filename);
    if (!file) return;

    // Reserve a sensible size up front so push_back doesn't reallocate
    // repeatedly on large files. We use the file size as a rough hint
    // (~8 bytes per edge line is a safe lower bound).
    file.seekg(0, std::ios::end);
    const auto size_hint = file.tellg();
    file.seekg(0, std::ios::beg);
    if (size_hint > 0)
        edge_buffer.reserve(static_cast<size_t>(size_hint) / 8);

    int source, target;
    while (file >> source >> target)
    {
        edge_buffer.emplace_back(source, target);
        if (source + 1 > total_nodes) total_nodes = source + 1;
        if (target + 1 > total_nodes) total_nodes = target + 1;
    }
}

/********************************************************************** */
