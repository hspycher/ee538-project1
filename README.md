# EE538: Project 1: Opinion Diffusion using Linked Lists

## Overview
In this project, you will simulate how opinions spread in a simple "society" of voters. Each voter holds one of two opinions — represented by 0 or 1. Voters influence one another: at each step, to fit into their social circle, voters adopt the majority opinion of their friends. This is repeated multiple times until no one is changing their opinions anymore or enough time (number of iterations) has passed.


## Data Representation
- The startercode loads the inputs from files in your workspace: opinions.txt and edge_list.txt.
opinions,txt contains a list of initial opinions. The file looks like

0, 0

1, 1

2, 1

...

Each line corresponding to a voter -- a line 2, 1 means voter 2 has opinion 1.

The other file "edge_list.txt" contains a pair of indices denoting who is influenced by whom. The file looks like

0, 2

2, 3

1, 5

...

0, 2 corresponds to: voter 0 influences voter 2.

## Sample Output
The program should print the state of the system at regular intervals. For example:

Iteration 0: fraction of 1's = 0.52

Iteration 100: fraction of 1's = 0.47

Iteration 200: fraction of 1's = 0.38

Consensus reached: all 0’s

## Part 1
Your task is to clone this repo and complete the startercode and complete the following:

```
void build_adj_matrix()
{
    // (1) allocate matrix adj of appropriate size


    // (2) run through edge list and populate adj
}

double calculate_fraction_of_ones()
{
    // (3) Calculate the fraction of nodes with opinion 1 and return it.
}

// For a given node, count majority opinion among its neighbours. Tie -> 0.
int get_majority_friend_opinions(int node)
{
    // (4) Count the number of neighbours with opinion 0 and opinion 1. Return the majority (0 or 1). 
    //If tie, return 0.
}

// Calculate new opinions for all voters and return if anyone's opinion changed
bool update_opinions()
{
    // (5) For each node, calculate the majority opinion among its neighbours and update the node's opinion.
    // Return true if any node's opinion changed, false otherwise.
    
}
```

Then, in main():

```
 cout << "Iteration " << iteration << ": fraction of 1's = " 
         << calculate_fraction_of_ones() << endl;
    
    // (6) Run until consensus or max iterations
    //while( ... )
    
    // Print final result
    double final_fraction = calculate_fraction_of_ones();
    cout << "Iteration " << iteration << ": fraction of 1's = " 
         << final_fraction << endl;
```

Test your code by editing the input .txt files.

## Part 2: Efficiency Upgrade

For Part 2 the brief was to make the simulation suitable for a *large* social network — think millions of voters and tens of millions of edges — by picking better data structures. The Part 1 implementation works on the toy 40-node input but does not scale, so this section documents what changed and why.

### What was wrong with the Part 1 design

The Part 1 code stored the graph as a dense N×N adjacency matrix and walked it column-by-column to find each voter's influencers. That has three problems on a real-world graph:

- **Quadratic memory.** `adj` is `vector<vector<int>>` of size N². At N = 10⁶ that is 10¹² ints (~4 TB), which simply will not fit. Social graphs are extremely sparse — average degree is at most a few hundred, never N — so almost every entry in the matrix is a wasted zero.
- **Quadratic work per step.** `get_majority_friend_opinions(node)` loops `for i in 0..N` reading `adj[i][node]`. That is O(N) per voter and O(N²) per simulation step regardless of how few real edges exist. It also touches a column of a row-major matrix, which is cache-hostile.
- **Wasteful per-edge overhead.** `edge_list` was `vector<vector<int>>`, which costs ~32 bytes per edge (vector header + heap allocation) instead of the 8 bytes the data actually needs.

### What changed

The matrix is gone. The graph is now stored as a **CSR (Compressed Sparse Row) reverse-adjacency list**, which is the textbook representation for large, static, sparse graphs:

- `opinions` is `vector<uint8_t>` — one byte per voter instead of four.
- `rev_off` is a `vector<int>` of size N+1. Entry `rev_off[v]` is where v's in-neighbors begin.
- `rev_adj` is a `vector<int>` of size E. The voters that influence v are exactly the contiguous slice `rev_adj[ rev_off[v] .. rev_off[v+1] )`.
- `edge_buffer` is a temporary `vector<pair<int,int>>` populated by `read_edges` and freed (via `swap` with an empty vector) the moment CSR is built — no double-storage at steady state.

CSR was chosen over `vector<vector<int>>` for two reasons. First, `vector<vector<int>>` carries a 24-byte header plus a separate heap allocation per node — for N = 10⁶ that is ~24 MB of pure overhead before any edges are stored. Second, CSR keeps every node's neighbor list contiguous in one flat array, so the inner loop streams through memory linearly and the CPU prefetcher can keep up.

We store *reverse* adjacency (in-neighbors per node) rather than forward adjacency because that is exactly what `get_majority_friend_opinions` needs — for each voter v it asks "who influences me?", which is the set of sources s with edge (s → v).

`build_reverse_adj_csr` constructs the CSR in three linear passes over the edge buffer: count in-degrees into `rev_off[t+1]`, prefix-sum to turn counts into offsets, then scatter sources into each target's slot. No sorting required; total work is O(N + E).

A handful of smaller changes that matter at scale:

- `get_majority_friend_opinions` uses a single accumulator (`count_1 += opinions[...]`) since values are 0 or 1; `count_0` is `total − count_1`. Half the writes, no branches inside the hot loop.
- `update_opinions` commits the new state with `opinions.swap(next_opinions)` — O(1) instead of copying N bytes.
- `read_opinions` now writes to `opinions[id]` instead of `push_back`, so non-contiguous IDs in `opinions.txt` no longer slide voters out of alignment with their edges.
- `read_edges` reserves capacity from the file-size hint to avoid repeated reallocation, and `ios::sync_with_stdio(false)` removes the C-stdio synchronization tax on input.

### Complexity, before and after

|                          | Part 1 (matrix)    | Part 2 (CSR rev-adj) |
|--------------------------|--------------------|----------------------|
| Graph storage            | O(N²)              | O(N + E)             |
| One simulation step      | O(N²)              | O(N + E)             |
| Build cost               | O(N² + E)          | O(N + E)             |
| Memory at N=10⁶, E=10⁷   | ~4 TB              | ~50 MB               |

For sparse graphs (E ≪ N²) the simulation step drops from quadratic to effectively linear in the number of edges, which is the relevant regime for any realistic social network.