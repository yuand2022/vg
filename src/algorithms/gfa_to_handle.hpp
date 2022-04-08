#ifndef VG_ALGORITHMS_GFA_TO_HANDLE_HPP_INCLUDED
#define VG_ALGORITHMS_GFA_TO_HANDLE_HPP_INCLUDED

/**
 * \file gfa_to_handle.hpp
 *
 * Defines algorithms for copying data from GFA files into handle graphs
 */

#include <iostream>
#include <cctype>
#include <vector>

#include "../handle.hpp"

namespace vg {
namespace algorithms {
using namespace std;

/// This exception will be thrown if the GFA data is not acceptable.
struct GFAFormatError : std::runtime_error {
    // Keep the constructor from a message
    using std::runtime_error::runtime_error;
};

/**
 * Stores ID information for a graph imported from a GFA.
 * Either all IDs are numerically equivalent to their GFA string IDs, or they
 * are stored in the name_to_id map.
 */
struct GFAIDMapInfo : public NamedNodeBackTranslation {
    /// If true, GFA string IDs are just graph numerical IDs.
    bool numeric_mode = true;
    /// This holds the max node ID yet used.
    nid_t max_id = 0;
    /// This maps from GFA string ID to graph numerical ID.
    /// This is behind a unique_ptr so it can be safely pointed into.
    unique_ptr<unordered_map<string, nid_t>> name_to_id = std::make_unique<unordered_map<string, nid_t>>();
    
    /// This inverts the name to ID map, and is populated when
    /// invert_translation is called, so it can be accessed thread-safely.
    unique_ptr<unordered_map<nid_t, const std::string*>> id_to_name;
    
    /**
     * Prepare the backing data structures for get_back_graph_node_name(). Call after name_to_id is complete.
     */
    void invert_translation();
    
    /**
     * Back-translation of node ranges. Is a no-op for imported GFA graphs that
     * haven't been modified, since the GFA graph is itself the backing graph.
     */
    std::vector<oriented_node_range_t> translate_back(const oriented_node_range_t& range) const;
    
    /**
     * Get the GFA sequence name of a node, given its ID.
     * Assumes this will never be called until after name_to_id is fully populated.
     */
    std::string get_back_graph_node_name(const nid_t& back_node_id) const;

};

/// Read a GFA file for a blunt-ended graph into a HandleGraph. Give "-" as a filename for stdin.
///
/// Throws GFAFormatError if the GFA file is not acceptable, and
/// std::ios_base::failure if an IO operation fails. Throws invalid_argument if
/// otherwise misused.
/// Does not give max ID hints, and so might be very slow when loading into an ODGI graph.
void gfa_to_handle_graph(const string& filename,
                         MutableHandleGraph* graph,
                         GFAIDMapInfo* translation = nullptr);

/// Overload which serializes its translation to a file internally.
void gfa_to_handle_graph(const string& filename,
                         MutableHandleGraph* graph,
                         const string& translation_filename);

/// Load a GFA from a stream (assumed not to be seekable or reopenable) into a HandleGraph.
void gfa_to_handle_graph(istream& in,
                         MutableHandleGraph* graph,
                         GFAIDMapInfo* translation = nullptr);

/// Same as gfa_to_handle_graph but also adds path elements from the GFA to the graph.
void gfa_to_path_handle_graph(const string& filename,
                              MutablePathMutableHandleGraph* graph,
                              GFAIDMapInfo* translation = nullptr,
                              int64_t max_rgfa_rank = numeric_limits<int64_t>::max());

/// Overload which serializes its translation to a file internally.
void gfa_to_path_handle_graph(const string& filename,
                              MutablePathMutableHandleGraph* graph,
                              int64_t max_rgfa_rank,
                              const string& translation_filename);
                              
/// Load a GFA from a stream (assumed not to be seekable or reopenable) into a PathHandleGraph.
void gfa_to_path_handle_graph(istream& in,
                              MutablePathMutableHandleGraph* graph,
                              GFAIDMapInfo* translation = nullptr,
                              int64_t max_rgfa_rank = numeric_limits<int64_t>::max());

/**
 * Lower-level tools for parsing GFA elements.
 *
 * Parsing functions return the fields as strings, and don't support overlaps.
 * Optional tags get read as strings in the vectors. 
 */
class GFAParser {
public:
    
    // We are going to split up existing line buffers.
    // So we need a cursor into one.
    using cursor_t = string::const_iterator;
    // And a range in one
    using range_t = pair<cursor_t, cursor_t>;
    // And a way to get the string value for one
    inline static string extract(const range_t& range) {
        return string(range.first, range.second);
    }
    // And a way to get the length of one
    inline static size_t length(const range_t& range) {
        return range.second - range.first;
    }
    // And a way to tell if one is empty
    inline static bool empty(const range_t& range) {
        return range.second == range.first;
    }
    // And a type for a collection of GFA tags.
    // This could become a range or list of ranges if we wanted to copy less.
    using tag_list_t = vector<string>;
    
    /**
     * Parse tags out from a possibly empty range to a vector of tag strings.
     */
    static tag_list_t parse_tags(const range_t& tag_range);
    
    /**
     * Parse an S line to name, sequence, and tags
     */
    static tuple<string, range_t, tag_list_t> parse_s(const string& s_line);
    
    /**
     * Parse an L line to name, is_reverse, name, is_reverse, overlap, and tags
     */
    static tuple<string, bool, string, bool, range_t, tag_list_t> parse_l(const string& l_line);
    
    /**
     * Parse a P line into name, visits, overlaps, and tags.
     */
    static tuple<string, range_t, range_t, tag_list_t> parse_p(const string& p_line);
    
    /**
     * Scan visits in a P line.
     * Calls a callback with all the steps.
     * visit_step takes {path-name, rank (-1 if path empty), step id, step reversed}
     * and returns true if it wants to keep iterating (false means stop).
     */
    static void scan_p(const string& p_line,
                       function<bool(const string& path_name, int64_t rank, const range_t& node_name, bool is_reverse)> visit_step);
                       
    /**
     * Scan visits extracted from a P line.
     * Calls a callback with all the steps.
     * visit_step takes {rank (-1 if path empty), step node name, step reversed}
     * and returns true if it wants to keep iterating (false means stop).
     */
    static void scan_p_visits(const range_t& visit_range,
                              function<bool(int64_t rank, const range_t& node_name, bool is_reverse)> visit_step);
   
    /**
     * Decode rGFA tags from the given list of tags from an S line.
     * Stores rGFA parameters at the given locations if set.
     * Returns true if a complete set of tags was found.
     */
    static bool decode_rgfa_tags(const tag_list_t& tags,
                                 string* out_name = nullptr,
                                 int64_t* out_offset = nullptr,
                                 int64_t* out_rank = nullptr);
    
    /**
     * Parse a GFA name into a numeric id.
     *
     * If all ids are numeric, they will be converted directly with stol.
     *
     * If all ids are non-numeric, they will get incrementing ids beginning
     * with 1, in order they are visited.
     *
     * If they are a mix of numeric and non-numeric, the numberic ones will be
     * converted with stol until the first non-numeric one is found, then it
     * will revert to using max-id.
     *
     * Since non-numeric ids are dependent on the order the nodes are scanned,
     * there is the unfortunate side effect that they will be different
     * sepending on whether the GFA is processed in lexicographic order or file
     * order.
     */
    static nid_t parse_sequence_id(const string& str, GFAIDMapInfo& id_map_info);
    
    /**
     * Find the existing sequence ID for the given node name, or 0 if it has not been seen yet.
     */
    static nid_t find_existing_sequence_id(const string& str, GFAIDMapInfo& id_map_info);
    
    // To actually parse GFA, we stick event listeners on here and then we go
    // through the GFA. It is the parser's job to make sure events aren't fired
    // before events they depend on (so a path is delayed until all the nodes
    // in it are parsed).
    
    // We can either use an internal ID map here
    unique_ptr<GFAIDMapInfo> internal_id_map;
    // Or have this pointed at an external one before we start parsing.
    GFAIDMapInfo* external_id_map;
    
    /// Get the ID map we should be using for parsing.
    inline GFAIDMapInfo& id_map();
    
    /// These listeners will be called with information for all nodes.
    vector<std::function<void(nid_t id, const range_t& sequence, const tag_list_t& tags)>> node_listeners;
    /// These listeners will be called with information for all edges, after
    /// the node listeners for the involved nodes.
    vector<std::function<void(nid_t from, bool from_is_reverse, nid_t to, bool to_is_reverse, const range_t& overlap, const tag_list_t& tags)>> edge_listeners;
    /// These listeners will be called with information for all P line paths,
    /// after the listeners for all involved nodes.
    vector<std::function<void(const string& name, const range_t& visits, const range_t& overlaps, const tag_list_t& tags)>> path_listeners;
    /// These listeners will be called with each visit of an rGFA path to a
    /// node, after the node listeners for the involved node. They will be
    /// called in order along each path. The listener is responsible for
    /// detecting any gaps in the offset space and producing multiple subpaths
    /// if necessary.
    vector<std::function<void(nid_t id, int64_t offset, const string& path_name, int64_t path_rank)>> rgfa_listeners;
    
    /// Include paths from rGFA tags at this rank or lower. Set to -1 to ignore rGFA tags.
    int64_t max_rgfa_rank = -1;
    
    /**
     * Parse GFA from the given stream.
     */
    void parse(istream& in);
    
};

}
}

#endif
