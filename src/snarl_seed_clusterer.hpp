#ifndef VG_SNARL_SEED_CLUSTERER_HPP_INCLUDED
#define VG_SNARL_SEED_CLUSTERER_HPP_INCLUDED

#include "snarls.hpp"
#include "snarl_distance_index.hpp"
#include "seed_clusterer.hpp"
#include "hash_map.hpp"
#include "small_bitset.hpp"
#include <structures/union_find.hpp>


namespace vg{

class NewSnarlSeedClusterer {



    public:

        /// Seed information used in Giraffe.
        struct Seed {
            pos_t  pos;
            size_t source; // Source minimizer.


            //Cached values from the minimizer
            //node length, root component, prefix sum, chain component, is_reversed
            tuple<size_t, size_t, size_t, size_t, bool, bool, bool, bool, size_t, size_t> minimizer_cache  = 
                make_tuple(MIPayload::NO_VALUE, MIPayload::NO_VALUE, MIPayload::NO_VALUE, MIPayload::NO_VALUE, false, false, false, false, MIPayload::NO_VALUE, MIPayload::NO_VALUE);

            //The distances to the left and right of whichever cluster this seed represents
            //This gets updated as clustering proceeds
            size_t distance_left = std::numeric_limits<size_t>::max();
            size_t distance_right = std::numeric_limits<size_t>::max();

            net_handle_t node_handle;

        };

        /// Cluster information used in Giraffe.
        //struct Cluster {
        //    std::vector<size_t> seeds; // Seed ids.
        //    size_t fragment; // Fragment id.
        //    double score; // Sum of scores of distinct source minimizers of the seeds.
        //    double coverage; // Fraction of read covered by the seeds.
        //    SmallBitset present; // Minimizers that are present in the cluster.
        //};
        typedef SnarlSeedClusterer::Cluster Cluster;

        NewSnarlSeedClusterer(const SnarlDistanceIndex& distance_index, const HandleGraph* graph);
        NewSnarlSeedClusterer(const SnarlDistanceIndex* distance_index, const HandleGraph* graph);

        //TODO: I don't want to be too tied to the minimizer_mapper implementation with seed structs

        ///Given a vector of seeds and a distance limit, 
        //cluster the seeds such that two seeds whose minimum distance
        //between them (including both of the positions) is less than
        // the distance limit are in the same cluster

        vector<Cluster> cluster_seeds ( vector<Seed>& seeds, size_t read_distance_limit) const;
        
        ///The same thing, but for paired end reads.
        //Given seeds from multiple reads of a fragment, cluster each read
        //by the read distance and all seeds by the fragment distance limit.
        //fragment_distance_limit must be greater than read_distance_limit
        //Returns clusters for each read and clusters of all the seeds in all reads
        //The read clusters refer to seeds by their indexes in the input vectors of seeds
        //The fragment clusters give seeds the index they would get if the vectors of
        // seeds were appended to each other in the order given
        // TODO: Fix documentation
        // Returns: For each read, a vector of clusters.

        vector<vector<Cluster>> cluster_seeds ( 
                vector<vector<Seed>>& all_seeds, size_t read_distance_limit, size_t fragment_distance_limit=0) const;

    private:


        //Actual clustering function that takes a vector of pointers to seeds
        tuple<vector<structures::UnionFind>, structures::UnionFind> cluster_seeds_internal ( 
                vector<vector<Seed>*>& all_seeds,
                size_t read_distance_limit, size_t fragment_distance_limit=0) const;

        const SnarlDistanceIndex& distance_index;
        const HandleGraph* graph;

        enum ChildNodeType {CHAIN, SNARL, NODE};

        
        static inline string typeToString(ChildNodeType t) {
            switch (t) {
            case CHAIN:
                return "CHAIN";
            case SNARL:
                return "SNARL";
            case NODE:
                return "NODE";
            default:
                return "OUT_OF_BOUNDS";
            }
        }


        struct NodeClusters {
            //All clusters of a snarl tree node
            //The node containing this struct may be an actual node,
            // snarl/chain that is a node the parent snarl's netgraph,
            // or a snarl in a chain

            //set of the indices of heads of clusters (group ids in the 
            //union find)
            //pair of <read index, seed index>
            hash_set<pair<size_t, size_t>> read_cluster_heads;

            //The shortest distance from any seed in any cluster to the 
            //left/right end of the snarl tree node that contains these
            //clusters
            pair<size_t, size_t> read_best_left = make_pair(std::numeric_limits<size_t>::max(), std::numeric_limits<size_t>::max());
            pair<size_t, size_t> read_best_right = make_pair(std::numeric_limits<size_t>::max(), std::numeric_limits<size_t>::max());
            size_t fragment_best_left = std::numeric_limits<size_t>::max();
            size_t fragment_best_right = std::numeric_limits<size_t>::max();

            //Distance from the start of the parent to the left of this node, etc
            size_t distance_start_left = std::numeric_limits<size_t>::max();
            size_t distance_start_right = std::numeric_limits<size_t>::max();
            size_t distance_end_left = std::numeric_limits<size_t>::max();
            size_t distance_end_right = std::numeric_limits<size_t>::max();

            //The snarl tree node that the clusters are on
            net_handle_t containing_net_handle; 
            net_handle_t parent_net_handle;
            net_handle_t grandparent_net_handle;
            net_handle_t end_in;

            nid_t node_id = 0;

            //Minimum length of a node or snarl
            //If it is a chain, then it is distance_index.chain_minimum_length(), which is
            //the expected length for a normal chain, and the length of the 
            //last component for a multicomponent chain 
            size_t node_length = std::numeric_limits<size_t>::max();             
            size_t prefix_sum_value = std::numeric_limits<size_t>::max(); //of node or first node in snarl
            size_t chain_component_start = 0; //of node or start of snarl
            size_t chain_component_end = 0; //of node or end of snarl

            size_t loop_left = std::numeric_limits<size_t>::max();
            size_t loop_right = std::numeric_limits<size_t>::max();

            //These are sometimes set if the value was in the cache
            //TODO: I should probably make this a static member of the class in case it changes
            bool has_parent_handle = false;;
            bool has_grandparent_handle = false;

            //Only set these for nodes or snarls in chains
            bool is_reversed_in_parent = false;




            //This one gets set for a (nontrivial) chain

            //Net handle of the chains last node pointing in
            bool is_trivial_chain = false;
            bool is_looping_chain = false;
            



            //Constructor
            //read_count is the number of reads in a fragment (2 for paired end)
            NodeClusters( net_handle_t net, size_t read_count, size_t seed_count, const SnarlDistanceIndex& distance_index) :
                containing_net_handle(std::move(net)),
                fragment_best_left(std::numeric_limits<size_t>::max()), fragment_best_right(std::numeric_limits<size_t>::max()){
                read_cluster_heads.reserve(seed_count);
            }
            //Constructor for a node or trivial chain
            NodeClusters( net_handle_t net, size_t read_count, size_t seed_count, bool is_reversed_in_parent, nid_t node_id, size_t node_length, size_t prefix_sum, size_t component) :
                containing_net_handle(net),
                is_reversed_in_parent(is_reversed_in_parent),
                node_length(node_length),
                prefix_sum_value(prefix_sum),
                chain_component_start(component),
                chain_component_end(component),
                node_id(node_id),
                fragment_best_left(std::numeric_limits<size_t>::max()), fragment_best_right(std::numeric_limits<size_t>::max()){
                    read_cluster_heads.reserve(seed_count);
            }
            void set_chain_values(const SnarlDistanceIndex& distance_index) {
                is_looping_chain = distance_index.is_looping_chain(containing_net_handle);
                node_length = distance_index.chain_minimum_length(containing_net_handle);
                end_in = distance_index.get_bound(containing_net_handle, true, true);
                chain_component_end = distance_index.get_chain_component(end_in, true);
            }
            void set_snarl_values(const SnarlDistanceIndex& distance_index) {
                node_length = distance_index.minimum_length(containing_net_handle);
                net_handle_t start_in = distance_index.get_node_from_sentinel(distance_index.get_bound(containing_net_handle, false, true));
                end_in =   distance_index.get_node_from_sentinel(distance_index.get_bound(containing_net_handle, true, true));
                chain_component_start = distance_index.get_chain_component(start_in);
                chain_component_end = distance_index.get_chain_component(end_in);
                prefix_sum_value = SnarlDistanceIndex::sum({
                                 distance_index.get_prefix_sum_value(start_in),
                                 distance_index.minimum_length(start_in)});
                loop_right = SnarlDistanceIndex::sum({distance_index.get_forward_loop_value(end_in),
                                                             2*distance_index.minimum_length(end_in)});
                //Distance to go backward in the chain and back
                loop_left = SnarlDistanceIndex::sum({distance_index.get_reverse_loop_value(start_in),
                                                            2*distance_index.minimum_length(start_in)});


            }

        };

        struct ParentToChildMap {
            //Struct for storing a map from a parent net_handle_t to a list of it's children
            //The children are represented as an index into all_clusters

            //The actual data that gets stored
            //hash map from the parent (as an index into all_clusters) to a vector of its children
            //Child list has a bool only_seeds
            //Each child is a tuple: 
            //
            // -The net_handle_t is the containing net handle
            // -The next two size_ts are depend on the net handle: if it's a snarl, then the first
            //   size_t is an index into all_node_clusters and the second is inf.
            //   If it's a node then they are the read_num, seed_num indices into all_seeds
            // -The third size_t is the chain component of the child
            // -The fourth size_t is the left offset of the seed or start node of the snarl snarl 
            //  (this is used for sorting children)
            hash_map<size_t, pair<bool, vector<tuple<net_handle_t, size_t, size_t, size_t, size_t>>>> parent_to_children;


            //Add a child (the handle, child_index) to its parent (parent_index)
            //Component is the component of the node or start component of the snarl
            //Offset is the prefix sum value of the seed, or the prefix sum of the start node of the snarl + 
            // node length of the start node
            // expected_size is the size to reserve for each parent chain
            void add_child(size_t& parent_index, net_handle_t& handle, size_t& child_index, size_t child_index2, 
                           size_t& component, size_t offset, bool is_seed, size_t expected_size) {
                if (parent_to_children.count(parent_index) == 0) {
                    parent_to_children[parent_index] = make_pair(is_seed, std::vector< tuple<net_handle_t, size_t, size_t, size_t, size_t>> (0));
                    parent_to_children[parent_index].second.reserve(expected_size);

                }
                parent_to_children[parent_index].first = parent_to_children[parent_index].first && is_seed;
                parent_to_children[parent_index].second.emplace_back(handle, child_index, child_index2, component, offset);
            }

            //For each pair of parent and child list, run iteratee
            void for_each_parent(const std::function<void(const size_t&, vector<tuple<net_handle_t, size_t, size_t, size_t, size_t>>&, const bool& only_seeds)>& iteratee) {
                for (auto& parent_and_child : parent_to_children) {
                    iteratee(parent_and_child.first, parent_and_child.second.second, parent_and_child.second.first);
                }
            }

            //Sort the child vector first by parent, and second by the order
            //of the children determined by comparator
            //returns true if it only contains seeds
            void sort_children(vector<tuple<net_handle_t, size_t, size_t, size_t, size_t>>& child_list,
                      const SnarlDistanceIndex& distance_index) {
                std::sort(child_list.begin(), child_list.end(),
                [&] (const tuple<net_handle_t, size_t, size_t, size_t, size_t>& a,
                     const tuple<net_handle_t, size_t, size_t, size_t, size_t>& b)->bool {
                    if (std::get<3>(a) == std::get<3>(b)) {
                        //If they are on the same component of the chain

                        if (std::get<4>(a) == std::get<4>(b)) {
                            //If they have the same prefix sum value, order using the distance index
                            return distance_index.is_ordered_in_chain(std::get<0>(a), std::get<0>(b));
                        } else {
                            //IF they have different prefix sum values, sort by prefix sum
                            return std::get<4>(a) < std::get<4>(b);
                        }

                    } else {
                        //If they are on different components, sort by component
                        return std::get<3>(a) < std::get<3>(b);
                    }
                });
            }
        };
        //These will be the cluster heads and distances for a cluster
        struct ClusterIndices {
            size_t read_num = std::numeric_limits<size_t>::max();
            size_t cluster_num = 0;
            size_t distance_left = 0;
            size_t distance_right = 0;
        };


        struct TreeState {
            //Hold all the tree relationships, seed locations, and cluster info

            //for the current level of the snarl tree and the parent level
            //As clustering occurs at the current level, the parent level
            //is updated to know about its children

            //Vector of all the seeds for each read
            vector<vector<Seed>*>* all_seeds; 

            //prefix sum vector of the number of seeds per read
            //To get the index of a seed for the fragment clusters
            //Also se this so that data structures that store information per seed can be single
            //vectors, instead of a vector of vectors following the structure of all_seeds 
            //since it uses less memory allocation to use a single vector
            vector<size_t> seed_count_prefix_sum;

            //The minimum distance between nodes for them to be put in the
            //same cluster
            size_t read_distance_limit;
            size_t fragment_distance_limit;


            //////////Data structures to hold clustering information

            //Structure to hold the clustering of the seeds
            vector<structures::UnionFind> read_union_find;
            structures::UnionFind fragment_union_find;



            //////////Data structures to hold snarl tree relationships
            //The snarls and chains get updated as we move up the snarl tree

            //Maps each node to a vector of the seeds that are contained in it
            //seeds are represented by indexes into the seeds vector (read_num, seed_num)
            hash_map<id_t, std::vector<std::pair<size_t, size_t>>> node_to_seeds;

            //This stores all the node clusters so we stop spending all our time allocating lots of vectors of NodeClusters
            vector<NodeClusters> all_node_clusters;

            //Map each net_handle to its index in all_node_clusters
            hash_map<net_handle_t, size_t> net_handle_to_index;

            
            //Map each chain to the snarls (only ones that contain seeds) that
            //comprise it. 
            //Snarls and chains represented as their indexes into 
            //distance_index.chain/snarl_indexes
            //Map maps the rank of the snarl to the snarl and snarl's clusters
            //  Since maps are ordered, it will be in the order of traversal
            //  of the snarls in the chain
            //  size_t is the index into all_node_clusters
            ParentToChildMap* chain_to_children;


            //Same structure as chain_to_children but for the level of the snarl
            //tree above the current one
            //This gets updated as the current level is processed
            //size_t is the index into all_node_clusters
            ParentToChildMap* parent_chain_to_children;

            //Map each snarl (as an index into all_node_clusters) to its children (also as an index into all_node_clusters)
            //for the current level of the snarl tree (inside the current chain) and its parent (parent of the current chain)
            std::multimap<size_t, size_t> snarl_to_children;

            //This holds all the child clusters of the root
            //each size_t is the index into all_node_clusters
            //Each pair is the parent and the child. This will be sorted by parent before
            //clustering so it
            vector<pair<size_t, size_t>> root_children;


            /////////////////////////////////////////////////////////

            //Constructor takes in a pointer to the seeds, the distance limits, and 
            //the total number of seeds in all_seeds
            TreeState (vector<vector<Seed>*>* all_seeds, size_t read_distance_limit, 
                       size_t fragment_distance_limit, size_t seed_count) :
                all_seeds(all_seeds),
                read_distance_limit(read_distance_limit),
                fragment_distance_limit(fragment_distance_limit),
                fragment_union_find (seed_count, false),
                seed_count_prefix_sum(1,0){

                for (size_t i = 0 ; i < all_seeds->size() ; i++) {
                    size_t size = all_seeds->at(i)->size();
                    size_t offset = seed_count_prefix_sum.back() + size;
                    seed_count_prefix_sum.push_back(offset);
                    read_union_find.emplace_back(size, false);

                }

                all_node_clusters.reserve(5*seed_count);
                net_handle_to_index.reserve(5*seed_count);
                root_children.reserve(seed_count);
            }
        };

        //Find which nodes contain seeds and assign those nodes to the 
        //snarls that contain them
        //Update the tree state's node_to_seed
        //and snarl_to_nodes_by_level, which assigns each node that contains
        //seeds to a snarl, organized by the level of the snarl in the snarl 
        //tree. snarl_to_nodes_by_level will be used to populate snarl_to_nodes
        //in the tree state as each level is processed
        //size_t is the index into all_node_clusters
        void get_nodes( TreeState& tree_state,
                        vector<ParentToChildMap>& chain_to_children_by_level) const;


        //Cluster all the snarls at the current level and update the tree_state
        //to add each of the snarls to the parent level
        void cluster_snarl_level(TreeState& tree_state) const;

        //Cluster all the chains at the current level
        void cluster_chain_level(TreeState& tree_state, size_t depth) const;

        //Cluster the seeds on the specified node
        void cluster_one_node(TreeState& tree_state, NodeClusters& node_clusters) const; 

        //Cluster the seeds in a snarl given by its net handle
        //Snarl_cluster_index is the index into tree_state.all_node_clusters
        void cluster_one_snarl(TreeState& tree_state, size_t snarl_clusters_index, 
                 std::multimap<size_t, size_t>::iterator child_range_start, std::multimap<size_t, size_t>::iterator child_range_end) const;

        //Cluster the seeds in a chain given by chain_index_i, an index into
        //distance_index.chain_indexes
        //If the depth is 0, also incorporate the top-level seeds from tree_state.top_level_seed_clusters
        //Chain children are tuples<net_handle, (child index, inf) or (seed read num, seed index), chain component, prefix sum>
        //If the children of the chain are only seeds on nodes, then cluster as if it is a node
        void cluster_one_chain(TreeState& tree_state, size_t chain_clusters_index, vector<tuple<net_handle_t, size_t, size_t, size_t, size_t>>& children_in_chain, bool only_seeds, bool is_top_level_chain) const;

        //Helper function for adding the next seed to the chain clusters
        void add_seed_to_chain_clusters(TreeState& tree_state, NodeClusters& chain_clusters,
                                        std::tuple<net_handle_t, size_t, size_t, size_t, size_t>& last_child, net_handle_t& last_child_handle, 
                                        size_t& last_prefix_sum, size_t& last_length, size_t& last_chain_component_end, 
                                        vector<ClusterIndices>& cluster_heads_to_add_again,
                                        bool& found_first_node, pair<bool, bool>& found_first_node_by_read,
                                        tuple<net_handle_t, size_t, size_t, size_t, size_t>& current_child_indices, bool is_first_child, bool is_last_child,
                                        bool skip_distances_to_ends) const;

        //Helper function for adding the next snarl to the chain clusters
        void add_snarl_to_chain_clusters(TreeState& tree_state, NodeClusters& chain_clusters,
                                        std::tuple<net_handle_t, size_t, size_t, size_t, size_t>& last_child, net_handle_t& last_child_handle, 
                                        size_t& last_prefix_sum, size_t& last_length, size_t& last_chain_component_end, 
                                        vector<ClusterIndices>& cluster_heads_to_add_again,
                                        bool& found_first_node, pair<bool, bool>& found_first_node_by_read,
                                        tuple<net_handle_t, size_t, size_t, size_t, size_t>& current_child_indices, bool is_first_child, bool is_last_child, 
                                        bool skip_distances_to_ends) const;

        //Cluster in the root 
        void cluster_root(TreeState& tree_state) const;

        //Cluster a list of seeds (SeedIndexes) that are on a single linear structure (node or chain)
        //Requires that the list of seeds are sorted relative to their position on the structure
        //The list of seeds is everything in the list between range_start and range_end
        //This can be called on a chain if there are no nested seeds on the chain
        //get_offset_from_seed_index returns a tuple of <read_num, seed_num, left offset> indices into all_seeds from whatever
        //SeedIndex is used to store the seeds
        //left offset is the distance from the left side of the structure
        template <typename SeedIndex>
        void cluster_seeds_on_linear_structure(TreeState& tree_state, NodeClusters& node_clusters, vector<SeedIndex>& seed_indices, 
                size_t structure_length, std::function<std::tuple<size_t, size_t, size_t>(const SeedIndex&)>& get_offset_from_seed_index, bool skip_distances_to_ends) const;

        //Compare two children of the parent and combine their clusters, to create clusters in the parent
        //This assumes that the first node hasn't been seen before but the second one has, so all of the
        //first node's clusters get added to the parent but assume that all of the second ones are already
        //part of the parent
        //old_distances contains the distances for cluster heads in the children, 
        //since the distances in tree_state.read_cluster_heads_to_distances will get updated
        //First child is true if this is the first time we see child_clusters1. If first_child is true and this is 
        //a snarl, then we need to update the snarl's distances to its parents
        void compare_and_combine_cluster_on_child_structures(TreeState& tree_state, NodeClusters& child_clusters1,
                NodeClusters& child_clusters2, NodeClusters& parent_clusters, 
                const vector<pair<size_t, size_t>>& child_distances, bool is_root, bool first_child) const;

        //The same as above, but compare clusters on a single child
        //This assumes that the child is the child of the root and not a root snarl
        //so we just look at external distances 
        void compare_and_combine_cluster_on_one_child(TreeState& tree_state, NodeClusters& child_clusters) const;

};
}

#endif
