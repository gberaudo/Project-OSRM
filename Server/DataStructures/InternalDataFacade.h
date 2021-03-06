/*

Copyright (c) 2013, Project OSRM, Dennis Luxen, others
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef INTERNAL_DATA_FACADE
#define INTERNAL_DATA_FACADE

// implements all data storage when shared memory is _NOT_ used

#include "BaseDataFacade.h"

#include "../../DataStructures/OriginalEdgeData.h"
#include "../../DataStructures/QueryNode.h"
#include "../../DataStructures/QueryEdge.h"
#include "../../DataStructures/SharedMemoryVectorWrapper.h"
#include "../../DataStructures/StaticGraph.h"
#include "../../DataStructures/StaticRTree.h"
#include "../../Util/BoostFileSystemFix.h"
#include "../../Util/GraphLoader.h"
#include "../../Util/ProgramOptions.h"
#include "../../Util/SimpleLogger.h"

#include <osrm/Coordinate.h>

template <class EdgeDataT> class InternalDataFacade : public BaseDataFacade<EdgeDataT>
{

  private:
    typedef BaseDataFacade<EdgeDataT> super;
    typedef StaticGraph<typename super::EdgeData> QueryGraph;
    typedef typename QueryGraph::InputEdge InputEdge;
    typedef typename super::RTreeLeaf RTreeLeaf;

    InternalDataFacade() {}

    unsigned m_check_sum;
    unsigned m_number_of_nodes;
    QueryGraph *m_query_graph;
    std::string m_timestamp;

    std::shared_ptr<ShM<FixedPointCoordinate, false>::vector> m_coordinate_list;
    ShM<NodeID, false>::vector m_via_node_list;
    ShM<unsigned, false>::vector m_name_ID_list;
    ShM<TurnInstruction, false>::vector m_turn_instruction_list;
    ShM<char, false>::vector m_names_char_list;
    ShM<unsigned, false>::vector m_name_begin_indices;
    ShM<bool, false>::vector m_egde_is_compressed;
    ShM<unsigned, false>::vector m_geometry_indices;
    ShM<unsigned, false>::vector m_geometry_list;

    std::shared_ptr<StaticRTree<RTreeLeaf, ShM<FixedPointCoordinate, false>::vector, false>>
    m_static_rtree;

    void LoadTimestamp(const boost::filesystem::path &timestamp_path)
    {
        if (boost::filesystem::exists(timestamp_path))
        {
            SimpleLogger().Write() << "Loading Timestamp";
            boost::filesystem::ifstream timestamp_stream(timestamp_path);
            if (!timestamp_stream)
            {
                SimpleLogger().Write(logWARNING) << timestamp_path << " not found";
            }
            getline(timestamp_stream, m_timestamp);
            timestamp_stream.close();
        }
        if (m_timestamp.empty())
        {
            m_timestamp = "n/a";
        }
        if (25 < m_timestamp.length())
        {
            m_timestamp.resize(25);
        }
    }

    void LoadGraph(const boost::filesystem::path &hsgr_path)
    {
        typename ShM<typename QueryGraph::NodeArrayEntry, false>::vector node_list;
        typename ShM<typename QueryGraph::EdgeArrayEntry, false>::vector edge_list;

        SimpleLogger().Write() << "loading graph from " << hsgr_path.string();

        m_number_of_nodes = readHSGRFromStream(hsgr_path, node_list, edge_list, &m_check_sum);

        BOOST_ASSERT_MSG(0 != node_list.size(), "node list empty");
        // BOOST_ASSERT_MSG(0 != edge_list.size(), "edge list empty");
        SimpleLogger().Write() << "loaded " << node_list.size() << " nodes and " << edge_list.size()
                               << " edges";
        m_query_graph = new QueryGraph(node_list, edge_list);

        BOOST_ASSERT_MSG(0 == node_list.size(), "node list not flushed");
        BOOST_ASSERT_MSG(0 == edge_list.size(), "edge list not flushed");
        SimpleLogger().Write() << "Data checksum is " << m_check_sum;
    }

    void LoadNodeAndEdgeInformation(const boost::filesystem::path &nodes_file,
                                    const boost::filesystem::path &edges_file)
    {
        boost::filesystem::ifstream nodes_input_stream(nodes_file, std::ios::binary);

        NodeInfo current_node;
        unsigned number_of_coordinates = 0;
        nodes_input_stream.read((char *)&number_of_coordinates, sizeof(unsigned));
        m_coordinate_list =
            std::make_shared<std::vector<FixedPointCoordinate>>(number_of_coordinates);
        for (unsigned i = 0; i < number_of_coordinates; ++i)
        {
            nodes_input_stream.read((char *)&current_node, sizeof(NodeInfo));
            m_coordinate_list->at(i) = FixedPointCoordinate(current_node.lat, current_node.lon);
            BOOST_ASSERT((std::abs(m_coordinate_list->at(i).lat) >> 30) == 0);
            BOOST_ASSERT((std::abs(m_coordinate_list->at(i).lon) >> 30) == 0);
        }
        nodes_input_stream.close();

        boost::filesystem::ifstream edges_input_stream(edges_file, std::ios::binary);
        unsigned number_of_edges = 0;
        edges_input_stream.read((char *)&number_of_edges, sizeof(unsigned));
        m_via_node_list.resize(number_of_edges);
        m_name_ID_list.resize(number_of_edges);
        m_turn_instruction_list.resize(number_of_edges);
        m_egde_is_compressed.resize(number_of_edges);

        unsigned compressed = 0;

        OriginalEdgeData current_edge_data;
        for (unsigned i = 0; i < number_of_edges; ++i)
        {
            edges_input_stream.read((char *)&(current_edge_data), sizeof(OriginalEdgeData));
            m_via_node_list[i] = current_edge_data.via_node;
            m_name_ID_list[i] = current_edge_data.name_id;
            m_turn_instruction_list[i] = current_edge_data.turn_instruction;
            m_egde_is_compressed[i] = current_edge_data.compressed_geometry;
            if (m_egde_is_compressed[i])
            {
                ++compressed;
            }
        }

        edges_input_stream.close();
    }

    void LoadGeometries(const boost::filesystem::path &geometry_file)
    {
        std::ifstream geometry_stream(geometry_file.c_str(), std::ios::binary);
        unsigned number_of_indices = 0;
        unsigned number_of_compressed_geometries = 0;

        geometry_stream.read((char *)&number_of_indices, sizeof(unsigned));

        m_geometry_indices.resize(number_of_indices);
        geometry_stream.read((char *)&(m_geometry_indices[0]),
                             number_of_indices * sizeof(unsigned));

        geometry_stream.read((char *)&number_of_compressed_geometries, sizeof(unsigned));

        BOOST_ASSERT(m_geometry_indices.back() == number_of_compressed_geometries);
        m_geometry_list.resize(number_of_compressed_geometries);

        geometry_stream.read((char *)&(m_geometry_list[0]),
                             number_of_compressed_geometries * sizeof(unsigned));
        geometry_stream.close();
    }

    void LoadRTree(const boost::filesystem::path &ram_index_path,
                   const boost::filesystem::path &file_index_path)
    {
        BOOST_ASSERT_MSG(!m_coordinate_list->empty(), "coordinates must be loaded before r-tree");

        m_static_rtree = std::make_shared<StaticRTree<RTreeLeaf>>(
            ram_index_path, file_index_path, m_coordinate_list);
    }

    void LoadStreetNames(const boost::filesystem::path &names_file)
    {
        boost::filesystem::ifstream name_stream(names_file, std::ios::binary);
        unsigned number_of_names = 0;
        unsigned number_of_chars = 0;
        name_stream.read((char *)&number_of_names, sizeof(unsigned));
        name_stream.read((char *)&number_of_chars, sizeof(unsigned));
        BOOST_ASSERT_MSG(0 != number_of_names, "name file broken");
        BOOST_ASSERT_MSG(0 != number_of_chars, "name file broken");

        m_name_begin_indices.resize(number_of_names);
        name_stream.read((char *)&m_name_begin_indices[0], number_of_names * sizeof(unsigned));

        m_names_char_list.resize(number_of_chars + 1); //+1 gives sentinel element
        name_stream.read((char *)&m_names_char_list[0], number_of_chars * sizeof(char));
        BOOST_ASSERT_MSG(0 != m_names_char_list.size(), "could not load any names");
        name_stream.close();
    }

  public:
    ~InternalDataFacade()
    {
        delete m_query_graph;
        m_static_rtree.reset();
    }

    explicit InternalDataFacade(const ServerPaths &server_paths)
    {
        // generate paths of data files
        if (server_paths.find("hsgrdata") == server_paths.end())
        {
            throw OSRMException("no hsgr file given in ini file");
        }
        if (server_paths.find("ramindex") == server_paths.end())
        {
            throw OSRMException("no ram index file given in ini file");
        }
        if (server_paths.find("fileindex") == server_paths.end())
        {
            throw OSRMException("no leaf index file given in ini file");
        }
        if (server_paths.find("geometries") == server_paths.end())
        {
            throw OSRMException("no geometries file given in ini file");
        }
        if (server_paths.find("nodesdata") == server_paths.end())
        {
            throw OSRMException("no nodes file given in ini file");
        }
        if (server_paths.find("edgesdata") == server_paths.end())
        {
            throw OSRMException("no edges file given in ini file");
        }
        if (server_paths.find("namesdata") == server_paths.end())
        {
            throw OSRMException("no names file given in ini file");
        }

        ServerPaths::const_iterator paths_iterator = server_paths.find("hsgrdata");
        BOOST_ASSERT(server_paths.end() != paths_iterator);
        const boost::filesystem::path &hsgr_path = paths_iterator->second;
        paths_iterator = server_paths.find("timestamp");
        BOOST_ASSERT(server_paths.end() != paths_iterator);
        const boost::filesystem::path &timestamp_path = paths_iterator->second;
        paths_iterator = server_paths.find("ramindex");
        BOOST_ASSERT(server_paths.end() != paths_iterator);
        const boost::filesystem::path &ram_index_path = paths_iterator->second;
        paths_iterator = server_paths.find("fileindex");
        BOOST_ASSERT(server_paths.end() != paths_iterator);
        const boost::filesystem::path &file_index_path = paths_iterator->second;
        paths_iterator = server_paths.find("nodesdata");
        BOOST_ASSERT(server_paths.end() != paths_iterator);
        const boost::filesystem::path &nodes_data_path = paths_iterator->second;
        paths_iterator = server_paths.find("edgesdata");
        BOOST_ASSERT(server_paths.end() != paths_iterator);
        const boost::filesystem::path &edges_data_path = paths_iterator->second;
        paths_iterator = server_paths.find("namesdata");
        BOOST_ASSERT(server_paths.end() != paths_iterator);
        const boost::filesystem::path &names_data_path = paths_iterator->second;
        paths_iterator = server_paths.find("geometries");
        BOOST_ASSERT(server_paths.end() != paths_iterator);
        const boost::filesystem::path &geometries_path = paths_iterator->second;

        // load data
        SimpleLogger().Write() << "loading graph data";
        AssertPathExists(hsgr_path);
        LoadGraph(hsgr_path);
        SimpleLogger().Write() << "loading egde information";
        AssertPathExists(nodes_data_path);
        AssertPathExists(edges_data_path);
        LoadNodeAndEdgeInformation(nodes_data_path, edges_data_path);
        SimpleLogger().Write() << "loading geometries";
        AssertPathExists(geometries_path);
        LoadGeometries(geometries_path);
        SimpleLogger().Write() << "loading r-tree";
        AssertPathExists(ram_index_path);
        AssertPathExists(file_index_path);
        LoadRTree(ram_index_path, file_index_path);
        SimpleLogger().Write() << "loading timestamp";
        LoadTimestamp(timestamp_path);
        SimpleLogger().Write() << "loading street names";
        AssertPathExists(names_data_path);
        LoadStreetNames(names_data_path);
    }

    // search graph access
    unsigned GetNumberOfNodes() const { return m_query_graph->GetNumberOfNodes(); }

    unsigned GetNumberOfEdges() const { return m_query_graph->GetNumberOfEdges(); }

    unsigned GetOutDegree(const NodeID n) const { return m_query_graph->GetOutDegree(n); }

    NodeID GetTarget(const EdgeID e) const { return m_query_graph->GetTarget(e); }

    EdgeDataT &GetEdgeData(const EdgeID e) { return m_query_graph->GetEdgeData(e); }

    const EdgeDataT &GetEdgeData(const EdgeID e) const { return m_query_graph->GetEdgeData(e); }

    EdgeID BeginEdges(const NodeID n) const { return m_query_graph->BeginEdges(n); }

    EdgeID EndEdges(const NodeID n) const { return m_query_graph->EndEdges(n); }

    EdgeRange GetAdjacentEdgeRange(const NodeID node) const { return m_query_graph->GetAdjacentEdgeRange(node); };

    // searches for a specific edge
    EdgeID FindEdge(const NodeID from, const NodeID to) const
    {
        return m_query_graph->FindEdge(from, to);
    }

    EdgeID FindEdgeInEitherDirection(const NodeID from, const NodeID to) const
    {
        return m_query_graph->FindEdgeInEitherDirection(from, to);
    }

    EdgeID FindEdgeIndicateIfReverse(const NodeID from, const NodeID to, bool &result) const
    {
        return m_query_graph->FindEdgeIndicateIfReverse(from, to, result);
    }

    // node and edge information access
    FixedPointCoordinate GetCoordinateOfNode(const unsigned id) const
    {
        return m_coordinate_list->at(id);
    };

    bool EdgeIsCompressed(const unsigned id) const { return m_egde_is_compressed.at(id); }

    TurnInstruction GetTurnInstructionForEdgeID(const unsigned id) const
    {
        return m_turn_instruction_list.at(id);
    }

    bool LocateClosestEndPointForCoordinate(const FixedPointCoordinate &input_coordinate,
                                            FixedPointCoordinate &result,
                                            const unsigned zoom_level = 18) const
    {
        return m_static_rtree->LocateClosestEndPointForCoordinate(
            input_coordinate, result, zoom_level);
    }

    bool FindPhantomNodeForCoordinate(const FixedPointCoordinate &input_coordinate,
                                      PhantomNode &resulting_phantom_node,
                                      const unsigned zoom_level) const
    {
        const bool found = m_static_rtree->FindPhantomNodeForCoordinate(
            input_coordinate, resulting_phantom_node, zoom_level);
        return found;
    }

    unsigned GetCheckSum() const { return m_check_sum; }

    unsigned GetNameIndexFromEdgeID(const unsigned id) const
    {
        return m_name_ID_list.at(id);
    };

    void GetName(const unsigned name_id, std::string &result) const
    {
        if (UINT_MAX == name_id)
        {
            result = "";
            return;
        }
        BOOST_ASSERT_MSG(name_id < m_name_begin_indices.size(), "name id too high");
        const unsigned begin_index = m_name_begin_indices[name_id];
        const unsigned end_index = m_name_begin_indices[name_id + 1];
        BOOST_ASSERT_MSG(begin_index < m_names_char_list.size(), "begin index of name too high");
        BOOST_ASSERT_MSG(end_index < m_names_char_list.size(), "end index of name too high");

        BOOST_ASSERT_MSG(begin_index <= end_index, "string ends before begin");
        result.clear();
        result.resize(end_index - begin_index);
        std::copy(m_names_char_list.begin() + begin_index,
                  m_names_char_list.begin() + end_index,
                  result.begin());
    }

    virtual unsigned GetGeometryIndexForEdgeID(const unsigned id) const
    {
        return m_via_node_list.at(id);
    }

    virtual void GetUncompressedGeometry(const unsigned id, std::vector<unsigned> &result_nodes)
        const
    {
        const unsigned begin = m_geometry_indices.at(id);
        const unsigned end = m_geometry_indices.at(id + 1);

        result_nodes.clear();
        result_nodes.insert(
            result_nodes.begin(), m_geometry_list.begin() + begin, m_geometry_list.begin() + end);
    }

    std::string GetTimestamp() const { return m_timestamp; }
};

#endif // INTERNAL_DATA_FACADE
