/**
 * graph containers, concepts and algorithms
 * @author Tobias Weber <tweber@ill.fr>
 * @date May 2021
 * @note Forked on 19-apr-2021 and 3-jun-2021 from my privately developed "geo" project (https://github.com/t-weber/geo).
 * @license GPLv3, see 'LICENSE' file
 *
 * References for the algorithms:
 *   - (Klein 2005) R. Klein, "Algorithmische Geometrie" (2005),
 *                  ISBN: 978-3540209560 (http://dx.doi.org/10.1007/3-540-27619-X).
 *   - (FUH 2020) R. Klein, C. Icking, "Algorithmische Geometrie" (2020),
 *                Kurs 1840, Fernuni Hagen (https://vu.fernuni-hagen.de/lvuweb/lvu/app/Kurs/1840).
 *   - (FUH 2021) A. Schulz, J. Rollin, "Effiziente Algorithmen" (2021),
 *                Kurs 1684, Fernuni Hagen (https://vu.fernuni-hagen.de/lvuweb/lvu/app/Kurs/01684).
 *   - (Berg 2008) M. de Berg, O. Cheong, M. van Kreveld, M. Overmars, "Computational Geometry" (2008),
 *                 ISBN: 978-3-642-09681-5 (http://dx.doi.org/10.1007/978-3-540-77974-2).
 *   - (Erickson 2019) J. Erickson, "Algorithms" (2019),
 *                     ISBN: 978-1-792-64483-2 (http://jeffe.cs.illinois.edu/teaching/algorithms/).
 *
 * ----------------------------------------------------------------------------
 * TAS-Paths (part of the Takin software suite)
 * Copyright (C) 2021       Tobias WEBER (Institut Laue-Langevin (ILL),
 *                          Grenoble, France).
 * "geo" project
 * Copyright (C) 2020-2021  Tobias WEBER (privately developed).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ----------------------------------------------------------------------------
 */

#ifndef __GRAPH_ALGOS_H__
#define __GRAPH_ALGOS_H__

#include <type_traits>
#include <concepts>
#include <vector>
#include <limits>
#include <stack>
#include <set>
#include <unordered_set>
#include <algorithm>
#include <optional>
#include <iostream>

#ifdef USE_BOOST_GRAPH
	#include <boost/graph/adjacency_list.hpp>
	#include <boost/graph/kruskal_min_spanning_tree.hpp>
#endif

#include "tlibs2/libs/maths.h"


namespace geo {

// ----------------------------------------------------------------------------
// concepts
// ----------------------------------------------------------------------------

// hack for standard libraries where this concept is not yet available
#define __GEO_NO_CONVERTIBLE__

#ifdef __GEO_NO_CONVERTIBLE__
	template<class t_1, class t_2>
	concept convertible_to = std::is_convertible<t_1, t_2>::value;
#else
	template<class t_1, class t_2>
	concept convertible_to = requires { std::convertible_to<t_1, t_2>; };
#endif


/**
 * requirements for the graph container interface
 */
template<class t_graph>
concept is_graph = requires(t_graph& graph, std::size_t vertidx)
{
	// function to get vertex count
	{ graph.GetNumVertices() }
		-> convertible_to<std::size_t>;

	// function to get vertex index from identifier
	{ graph.GetVertexIndex("v123") }
		-> convertible_to<std::optional<std::size_t>>;

	// function to get vertex identifier from index
	{ graph.GetVertexIdent(vertidx) }
		-> convertible_to<std::string>;

	// function to get edge weight
	{ graph.GetWeight(vertidx, vertidx) }
		-> convertible_to<std::optional<typename t_graph::t_weight>>;

	// get neighbours of a vertex
	graph.GetNeighbours(vertidx);

	// support insertion of vertices by identifier
	graph.AddVertex("v123");

	// support insertion of edges by index
	graph.AddEdge(0, 1);

	// support insertion of edges by vertex identifiers
	graph.AddEdge("v123", "v321");
};
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// containers
// ----------------------------------------------------------------------------

/**
 * adjacency matrix
 * @see (FUH 2021), Kurseinheit 4, pp. 3-5
 * @see https://en.wikipedia.org/wiki/Adjacency_matrix
 */
template<class _t_weight = unsigned int>
class AdjacencyMatrix
{
public:
	using t_weight = _t_weight;
	using t_mat = tl2::mat<std::optional<t_weight>, std::vector>;


public:
	AdjacencyMatrix() = default;
	~AdjacencyMatrix() = default;


	void Clear()
	{
		m_vertexidents.clear();
		m_mat = t_mat{};
	}


	std::size_t GetNumVertices() const
	{
		return m_mat.size1();
	}


	const std::string& GetVertexIdent(std::size_t i) const
	{
		return m_vertexidents[i];
	}


	std::optional<std::size_t> GetVertexIndex(const std::string& vert) const
	{
		auto iter = std::find(m_vertexidents.begin(), m_vertexidents.end(), vert);
		if(iter == m_vertexidents.end())
			return std::nullopt;

		return iter - m_vertexidents.begin();
	}


	void AddVertex(const std::string& id)
	{
		const std::size_t N = GetNumVertices();
		t_mat matNew = tl2::create<t_mat>(N+1, N+1);

		for(std::size_t i=0; i<N+1; ++i)
			for(std::size_t j=0; j<N+1; ++j)
				matNew(i,j) = (i<N && j<N) ? m_mat(i,j) : std::nullopt;

		m_mat = std::move(matNew);
		m_vertexidents.push_back(id);
	}


	void RemoveVertex(std::size_t idx)
	{
		m_mat = tl2::submat<t_mat>(m_mat, idx, idx);
		m_vertexidents.erase(m_vertexidents.begin() + idx);
	}


	void RemoveVertex(const std::string& id)
	{
		if(auto idx = GetVertexIndex(id); idx)
			RemoveVertex(*idx);
	}


	void SetWeight(std::size_t idx1, std::size_t idx2, t_weight w)
	{
		m_mat(idx1, idx2) = w;
	}


	void SetWeight(const std::string& vert1, const std::string& vert2, t_weight w)
	{
		auto idx1 = GetVertexIndex(vert1);
		auto idx2 = GetVertexIndex(vert2);

		if(idx1 && idx2)
			SetWeight(*idx1, *idx2, w);
	}


	std::optional<t_weight> GetWeight(std::size_t idx1, std::size_t idx2) const
	{
		return m_mat(idx1, idx2);
	}


	std::optional<t_weight> GetWeight(const std::string& vert1, const std::string& vert2) const
	{
		auto idx1 = GetVertexIndex(vert1);
		auto idx2 = GetVertexIndex(vert2);

		if(idx1 && idx2)
			return GetWeight(*idx1, *idx2);

		return std::nullopt;
	}


	void AddEdge(std::size_t idx1, std::size_t idx2, t_weight w=0)
	{
		SetWeight(idx1, idx2, w);
	}


	void AddEdge(const std::string& vert1, const std::string& vert2, t_weight w=0)
	{
		SetWeight(vert1, vert2, w);
	}


	std::vector<std::tuple<std::size_t, std::size_t, t_weight>> GetEdges() const
	{
		const std::size_t N = GetNumVertices();

		std::vector<std::tuple<std::size_t, std::size_t, t_weight>> edges;
		edges.reserve(N * N);

		for(std::size_t i=0; i<N; ++i)
		{
			for(std::size_t j=0; j<N; ++j)
			{
				if(auto w = m_mat(i,j); w)
					edges.emplace_back(std::make_tuple(i, j, *w));
			}
		}

		return edges;
	}


	std::vector<std::tuple<std::string, std::string, t_weight>> GetEdgesIdent() const
	{
		auto edges = GetEdges();
		std::vector<std::tuple<std::string, std::string, t_weight>> edgesident;
		edgesident.reserve(edges.size());

		for(const auto& edge : edges)
		{
			const std::string& ident1 = GetVertexIdent(std::get<0>(edge));
			const std::string& ident2 = GetVertexIdent(std::get<1>(edge));

			edgesident.emplace_back(std::make_tuple(ident1, ident2, std::get<2>(edge)));
		}

		return edgesident;
	}


	void RemoveEdge(std::size_t idx1, std::size_t idx2)
	{
		m_mat(idx1, idx2).reset();
	}


	void RemoveEdge(const std::string& vert1, const std::string& vert2)
	{
		auto idx1 = GetVertexIndex(vert1);
		auto idx2 = GetVertexIndex(vert2);

		if(!idx1 || !idx2)
			return;

		RemoveEdge(idx1, idx2);
	}


	bool IsAdjacent(std::size_t idx1, std::size_t idx2) const
	{
		return GetWeight(idx1, idx2).has_value();
	}


	bool IsAdjacent(const std::string& vert1, const std::string& vert2) const
	{
		return GetWeight(vert1, vert2).has_value();;
	}


	std::vector<std::size_t> GetNeighbours(std::size_t idx, bool outgoing_edges=true) const
	{
		std::vector<std::size_t> neighbours;
		const std::size_t N = GetNumVertices();
		neighbours.reserve(2*N);

		// neighbour vertices on outgoing edges
		if(outgoing_edges)
		{
			for(std::size_t idxOther=0; idxOther<N; ++idxOther)
			{
				if(GetWeight(idx, idxOther))
					neighbours.push_back(idxOther);
			}
		}

		// neighbour vertices on incoming edges
		else
		{
			for(std::size_t idxOther=0; idxOther<N; ++idxOther)
			{
				if(GetWeight(idxOther, idx))
					neighbours.push_back(idxOther);
			}
		}

		return neighbours;
	}


	std::vector<std::string> GetNeighbours(const std::string& vert, bool outgoing_edges=true) const
	{
		auto iter = std::find(m_vertexidents.begin(), m_vertexidents.end(), vert);
		if(iter == m_vertexidents.end())
			return {};
		std::size_t idx = iter - m_vertexidents.begin();

		std::vector<std::string> neighbours;
		const std::size_t N = GetNumVertices();
		neighbours.reserve(2*N);

		// neighbour vertices on outgoing edges
		if(outgoing_edges)
		{
			for(std::size_t idxOther=0; idxOther<N; ++idxOther)
			{
				if(GetWeight(idx, idxOther))
					neighbours.push_back(m_vertexidents[idxOther]);
			}
		}

		// neighbour vertices on incoming edges
		else
		{
			for(std::size_t idxOther=0; idxOther<N; ++idxOther)
			{
				if(GetWeight(idxOther, idx))
					neighbours.push_back(m_vertexidents[idxOther]);
			}
		}

		return neighbours;
	}


private:
	std::vector<std::string> m_vertexidents{};
	t_mat m_mat{};
};



/**
 * adjacency list
 * @see (FUH 2021), Kurseinheit 4, pp. 3-5
 * @see https://en.wikipedia.org/wiki/Adjacency_list
 */
template<class _t_weight = unsigned int>
class AdjacencyList
{
public:
	using t_weight = _t_weight;


public:
	AdjacencyList() = default;
	~AdjacencyList() = default;


	void Clear()
	{
		m_vertexidents.clear();
		m_nodes.clear();
	}


	std::size_t GetNumVertices() const
	{
		return m_vertexidents.size();
	}


	const std::string& GetVertexIdent(std::size_t i) const
	{
		return m_vertexidents[i];
	}


	std::optional<std::size_t> GetVertexIndex(const std::string& vert) const
	{
		auto iter = std::find(m_vertexidents.begin(), m_vertexidents.end(), vert);
		if(iter == m_vertexidents.end())
			return std::nullopt;

		return iter - m_vertexidents.begin();
	}


	void AddVertex(const std::string& id)
	{
		m_vertexidents.push_back(id);
		m_nodes.push_back(nullptr);
	}


	void RemoveVertex(std::size_t idx)
	{
		// remove nodes
		m_vertexidents.erase(m_vertexidents.begin() + idx);
		m_nodes.erase(m_nodes.begin() + idx);

		for(std::size_t idx1=0; idx1<m_nodes.size(); ++idx1)
		{
			std::shared_ptr<AdjNode> node = m_nodes[idx1];
			std::shared_ptr<AdjNode> node_prev;

			while(node)
			{
				if(node->idx == idx)
				{
					if(node_prev)
						node_prev->next = node->next;
					else
						m_nodes[idx1] = node->next;
					break;
				}

				node_prev = node;
				node = node->next;
			}
		}

		// fix indices
		std::unordered_set<std::shared_ptr<AdjNode>> seen_nodes;

		for(std::size_t idx1=0; idx1<m_nodes.size(); ++idx1)
		{
			std::shared_ptr<AdjNode> node = m_nodes[idx1];

			while(node)
			{
				if(seen_nodes.find(node) == seen_nodes.end())
				{
					seen_nodes.insert(node);

					if(node->idx > idx)
						--node->idx;
				}

				node = node->next;
			}
		}
	}


	void RemoveVertex(const std::string& id)
	{
		if(auto idx = GetVertexIndex(id); idx)
			RemoveVertex(*idx);
	}


	void SetWeight(std::size_t idx1, std::size_t idx2, t_weight w)
	{
		std::shared_ptr<AdjNode> node = m_nodes[idx1];

		while(node)
		{
			if(node->idx == idx2)
			{
				node->weight = w;
				break;
			}

			node = node->next;
		}
	}


	void SetWeight(const std::string& vert1, const std::string& vert2, t_weight w)
	{
		auto idx1 = GetVertexIndex(vert1);
		auto idx2 = GetVertexIndex(vert2);

		if(idx1 && idx2)
			SetWeight(*idx1, *idx2, w);
	}


	std::optional<t_weight> GetWeight(std::size_t idx1, std::size_t idx2) const
	{
		std::shared_ptr<AdjNode> node = m_nodes[idx1];

		while(node)
		{
			if(node->idx == idx2)
				return node->weight;

			node = node->next;
		}

		return std::nullopt;
	}


	std::optional<t_weight> GetWeight(const std::string& vert1, const std::string& vert2) const
	{
		auto idx1 = GetVertexIndex(vert1);
		auto idx2 = GetVertexIndex(vert2);

		if(idx1 && idx2)
			return GetWeight(*idx1, *idx2);

		return std::nullopt;
	}


	void AddEdge(std::size_t idx1, std::size_t idx2, t_weight w=0)
	{
		if(idx1 >= m_nodes.size() || idx2 >= m_nodes.size())
			return;

		// get previous head of list
		std::shared_ptr<AdjNode> node = m_nodes[idx1];
		// make new head of list
		m_nodes[idx1] = std::make_shared<AdjNode>();
		// previous head is next to new head
		m_nodes[idx1]->next = node;
		m_nodes[idx1]->idx = idx2;
		m_nodes[idx1]->weight = w;
	}


	void AddEdge(const std::string& vert1, const std::string& vert2, t_weight w=0)
	{
		auto idx1 = GetVertexIndex(vert1);
		auto idx2 = GetVertexIndex(vert2);

		if(!idx1 || !idx2)
			return;

		AddEdge(*idx1, *idx2, w);
	}


	void RemoveEdge(std::size_t idx1, std::size_t idx2)
	{
		std::shared_ptr<AdjNode> node = m_nodes[idx1];
		std::shared_ptr<AdjNode> node_prev;

		while(node)
		{
			if(node->idx == idx2)
			{
				if(node_prev)
					node_prev->next = node->next;
				else
					m_nodes[idx1] = node->next;
				break;
			}

			node_prev = node;
			node = node->next;
		}
	}


	void RemoveEdge(const std::string& vert1, const std::string& vert2)
	{
		auto idx1 = GetVertexIndex(vert1);
		auto idx2 = GetVertexIndex(vert2);

		if(!idx1 || !idx2)
			return;

		RemoveEdge(*idx1, *idx2);
	}


	bool IsAdjacent(std::size_t idx1, std::size_t idx2) const
	{
		auto neighbours = GetNeighbours(idx1);
		return std::find(neighbours.begin(), neighbours.end(), idx2) != neighbours.end();
	}


	bool IsAdjacent(const std::string& vert1, const std::string& vert2) const
	{
		auto idx1 = GetVertexIndex(vert1);
		auto idx2 = GetVertexIndex(vert2);

		if(!idx1 || !idx2)
			return false;

		return IsAdjacent(*idx1, *idx2);
	}


	std::vector<std::size_t> GetNeighbours(std::size_t idx, bool outgoing_edges=true) const
	{
		std::vector<std::size_t> neighbours;
		neighbours.reserve(GetNumVertices());

		// neighbour vertices on outgoing edges
		if(outgoing_edges)
		{
			std::shared_ptr<AdjNode> node = m_nodes[idx];

			while(node)
			{
				neighbours.push_back(node->idx);
				node = node->next;
			}
		}

		// neighbour vertices on incoming edges
		else
		{
			for(std::size_t i=0; i<m_nodes.size(); ++i)
			{
				std::shared_ptr<AdjNode> node = m_nodes[i];

				while(node)
				{
					if(node->idx == idx)
					{
						neighbours.push_back(i);
						break;
					}
					node = node->next;
				}
			}
		}

		return neighbours;
	}


	std::vector<std::string> GetNeighbours(const std::string& vert, bool outgoing_edges=true) const
	{
		auto idx = GetVertexIndex(vert);
		if(!idx)
			return {};

		std::vector<std::size_t> neighbour_indices = GetNeighbours(*idx, outgoing_edges);

		std::vector<std::string> neighbours;
		neighbours.reserve(neighbour_indices.size());

		for(std::size_t neighbour_index : neighbour_indices)
		{
			const std::string& id = GetVertexIdent(neighbour_index);
			neighbours.push_back(id);
		}

		return neighbours;
	}


private:
	struct AdjNode
	{
		std::size_t idx{};
		t_weight weight{};

		std::shared_ptr<AdjNode> next{};
	};

	std::vector<std::string> m_vertexidents{};
	std::vector<std::shared_ptr<AdjNode>> m_nodes{};
};
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// algorithms
// ----------------------------------------------------------------------------

/**
 * export graph to the dot format
 * @see https://graphviz.org/doc/info/lang.html
 */
template<class t_graph> requires is_graph<t_graph>
bool print_graph(const t_graph& graph, std::ostream& ostr = std::cout)
{
	const std::size_t N = graph.GetNumVertices();

	ostr << "digraph my_graph\n{\n";

	ostr << "\t// vertices\n";
	for(std::size_t i=0; i<N; ++i)
		ostr << "\t" << i << " [label=\"" << graph.GetVertexIdent(i) << "\"];\n";

	ostr << "\n";
	ostr << "\t// edges and weights\n";

	for(std::size_t i=0; i<N; ++i)
	{
		for(std::size_t j=0; j<N; ++j)
		{
			auto w = graph.GetWeight(i, j);
			if(!w)
				continue;

			ostr << "\t" << i << " -> " << j << " [label=\"" << *w << "\"];\n";
		}
	}

	ostr << "}\n";
	return true;
}


/**
 * dijkstra algorithm
 * @see (FUH 2021), Kurseinheit 4, p. 17
 * @see (Erickson 2019), p. 288
 */
template<class t_graph,
	class t_weight_func =
		std::optional<typename t_graph::t_weight>(std::size_t, std::size_t)>
requires is_graph<t_graph>
std::vector<std::optional<std::size_t>>
dijk(const t_graph& graph, const std::string& startvert,
	t_weight_func *weight_func = nullptr)
{
	// start index
	auto _startidx = graph.GetVertexIndex(startvert);
	if(!_startidx)
		return {};
	const std::size_t startidx = *_startidx;


	// distances
	const std::size_t N = graph.GetNumVertices();
	using t_weight = typename t_graph::t_weight;

	std::vector<t_weight> dists;
	std::vector<std::optional<std::size_t>> predecessors;
	dists.resize(N);
	predecessors.resize(N);

	// don't use the full maximum to prevent overflows when we're adding the weight afterwards
	const t_weight infinity = std::numeric_limits<t_weight>::max() / 2;
	for(std::size_t vertidx=0; vertidx<N; ++vertidx)
		dists[vertidx] = (vertidx==startidx ? 0 : infinity);


	// comparator for distances heap
	auto vert_cmp = [&dists](std::size_t idx1, std::size_t idx2) -> bool
	{
		return dists[idx1] >= dists[idx2];
	};

	// vertex distances heap
	std::vector<std::size_t> distheap;
	distheap.reserve(N);

	for(std::size_t vertidx=0; vertidx<N; ++vertidx)
		distheap.push_back(vertidx);

	std::make_heap(distheap.begin(), distheap.end(), vert_cmp);


	while(!distheap.empty())
	{
#ifdef DIJK_DEBUG
		std::cout << "\nNew iteration.\n";
		std::cout << "Vertex indices in distances heap:\n";
		for(std::size_t idx : distheap)
			std::cout << idx << " (dist: " << dists[idx] << "), ";
		std::cout << "." << std::endl;

		std::cout << "Predecessor indices:\n";
		for(const auto& pred : predecessors)
		{
			if(pred)
				std::cout << *pred;
			else
				std::cout << "null";
			std::cout << ", ";
		}
		std::cout << std::endl;
#endif
		std::size_t vertidx = distheap.front();
		std::pop_heap(distheap.begin(), distheap.end(), vert_cmp);
		distheap.pop_back();

		std::vector<std::size_t> neighbours = graph.GetNeighbours(vertidx);
		for(std::size_t neighbouridx : neighbours)
		{
			// edge weight
			std::optional<typename t_graph::t_weight> w;

			// directly get edge weight, or use user-supplied weight function
			if(!weight_func)
				w = graph.GetWeight(vertidx, neighbouridx);
			else
				w = (*weight_func)(vertidx, neighbouridx);

			if(!w)
				continue;

			// is the path from startidx to neighbouridx over vertidx shorter than from startidx to neighbouridx?
			if(dists[vertidx] + *w < dists[neighbouridx])
			{
#ifdef DIJK_DEBUG
				std::cout << "Path from " << startidx << " to "
					<< neighbouridx << " over " << vertidx
					<< " is shorter than from " << startidx
					<< " to " << neighbouridx << ": "
					<< "old distance: " << dists[neighbouridx] << ", "
					<< "new distance: " << dists[vertidx] + *w << "." << std::endl;
				std::cout << "Vertex " << vertidx <<
					" is new predecessor of " << neighbouridx << "." << std::endl;
#endif
				// update distance
				dists[neighbouridx] = dists[vertidx] + *w;
				predecessors[neighbouridx] = vertidx;

				// resort the heap after the distance changes
				std::make_heap(distheap.begin(), distheap.end(), vert_cmp);
			}
		}
	}

#ifdef DIJK_DEBUG
		std::cout << "\nFinal result.\n";
		std::cout << "Predecessor indices:\n";
		for(const auto& pred : predecessors)
		{
			if(pred)
				std::cout << *pred;
			else
				std::cout << "null";
			std::cout << ", ";
		}
		std::cout << std::endl;

		std::cout << "Distances:\n";
		for(t_weight w : dists)
			std::cout << w << ", ";
		std::cout << std::endl;
#endif

	return predecessors;
}


/**
 * dijkstra algorithm (version which also works for negative weights)
 * @see (Erickson 2019), p. 285
 */
template<class t_graph,
	class t_weight_func =
		std::optional<typename t_graph::t_weight>(std::size_t, std::size_t)>
requires is_graph<t_graph>
std::vector<std::optional<std::size_t>>
dijk_mod(const t_graph& graph, const std::string& startvert,
	t_weight_func *weight_func = nullptr)
{
	// start index
	auto _startidx = graph.GetVertexIndex(startvert);
	if(!_startidx)
		return {};
	const std::size_t startidx = *_startidx;

	// distances
	const std::size_t N = graph.GetNumVertices();
	using t_weight = typename t_graph::t_weight;

	std::vector<t_weight> dists;
	std::vector<std::optional<std::size_t>> predecessors;
	dists.resize(N);
	predecessors.resize(N);

	// don't use the full maximum to prevent overflows when we're adding the weight afterwards
	const t_weight infinity = std::numeric_limits<t_weight>::max() / 2;
	for(std::size_t vertidx=0; vertidx<N; ++vertidx)
		dists[vertidx] = (vertidx==startidx ? 0 : infinity);

	// distance priority queue and comparator
	auto vert_cmp = [&dists](std::size_t idx1, std::size_t idx2) -> bool
	{
		// sort by ascending value: !operator<
		return dists[idx1] >= dists[idx2];
	};

	std::vector<std::size_t> distheap;
	distheap.reserve(N);

	// push only start index, not all indices
	distheap.push_back(startidx);

	while(distheap.size())
	{
#ifdef DIJK_DEBUG
		std::cout << "\nNew iteration.\n";
		std::cout << "Vertex indices in distances heap:\n";
		for(std::size_t idx : distheap)
			std::cout << idx << " (dist: " << dists[idx] << "), ";
		std::cout << "." << std::endl;

		std::cout << "Predecessor indices:\n";
		for(const auto& pred : predecessors)
		{
			if(pred)
				std::cout << *pred;
			else
				std::cout << "null";
			std::cout << ", ";
		}
		std::cout << std::endl;
#endif

		std::size_t vertidx = *distheap.begin();
		std::pop_heap(distheap.begin(), distheap.end(), vert_cmp);
		distheap.pop_back();

		std::vector<std::size_t> neighbours = graph.GetNeighbours(vertidx);
		for(std::size_t neighbouridx : neighbours)
		{
			// edge weight
			std::optional<typename t_graph::t_weight> w;

			// directly get edge weight, or use user-supplied weight function
			if(!weight_func)
				w = graph.GetWeight(vertidx, neighbouridx);
			else
				w = (*weight_func)(vertidx, neighbouridx);

			if(!w)
				continue;

			// is the path from startidx to neighbouridx over vertidx shorter than from startidx to neighbouridx?
			if(dists[vertidx] + *w < dists[neighbouridx])
			{
#ifdef DIJK_DEBUG
				std::cout << "Path from " << startidx << " to "
					<< neighbouridx << " over " << vertidx
					<< " is shorter than from " << startidx
					<< " to " << neighbouridx << ": "
					<< "old distance: " << dists[neighbouridx] << ", "
					<< "new distance: " << dists[vertidx] + *w << "." << std::endl;
				std::cout << "Vertex " << vertidx <<
					" is new predecessor of " << neighbouridx << "." << std::endl;
#endif

				dists[neighbouridx] = dists[vertidx] + *w;
				predecessors[neighbouridx] = vertidx;

				// insert the new node index if it's not in the queue yet
				if(std::find(distheap.begin(), distheap.end(), neighbouridx) == distheap.end())
					distheap.push_back(neighbouridx);

				// resort the priority queue heap after neighbouridx distance changes
				std::make_heap(distheap.begin(), distheap.end(), vert_cmp);
			}
		}
	}

#ifdef DIJK_DEBUG
		std::cout << "\nFinal result.\n";
		std::cout << "Predecessor indices:\n";
		for(const auto& pred : predecessors)
		{
			if(pred)
				std::cout << *pred;
			else
				std::cout << "null";
			std::cout << ", ";
		}
		std::cout << std::endl;

		std::cout << "Distances:\n";
		for(t_weight w : dists)
			std::cout << w << ", ";
		std::cout << std::endl;
#endif

	return predecessors;
}


/**
 * bellman-ford algorithm
 * @see (FUH 2021), Kurseinheit 4, p. 13
 */
template<class t_graph,
	class t_mat=tl2::mat<typename t_graph::t_weight, std::vector>,
	class t_weight_func =
		std::optional<typename t_graph::t_weight>(std::size_t, std::size_t)>
requires is_graph<t_graph> && tl2::is_mat<t_mat>
std::tuple<t_mat, std::vector<std::optional<std::size_t>>>
bellman(const t_graph& graph, const std::string& startvert,
	t_weight_func *weight_func = nullptr)
{
	// start index
	auto _startidx = graph.GetVertexIndex(startvert);
	if(!_startidx)
		return std::make_tuple(t_mat{}, std::vector<std::optional<std::size_t>>{});
	const std::size_t startidx = *_startidx;


	// distances
	const std::size_t N = graph.GetNumVertices();
	using t_weight = typename t_graph::t_weight;
	t_mat dists = tl2::zero<t_mat>(N, N);

	// don't use the full maximum to prevent overflows when we're adding the weight afterwards
	const t_weight infinity = std::numeric_limits<t_weight>::max() / 2;

	for(std::size_t vertidx=0; vertidx<N; ++vertidx)
		dists(0, vertidx) = (vertidx==startidx ? 0 : infinity);


	// predecessor nodes
	std::vector<std::optional<std::size_t>> predecessors;
	predecessors.resize(N);


	// iterate vertices
	for(std::size_t i=1; i<N; ++i)
	{
		for(std::size_t vertidx=0; vertidx<N; ++vertidx)
		{
			dists(i, vertidx) = dists(i-1, vertidx);

			std::vector<std::size_t> neighbours = graph.GetNeighbours(vertidx, false);
			for(std::size_t neighbouridx : neighbours)
			{
				// edge weight
				std::optional<typename t_graph::t_weight> w;

				// directly get edge weight, or use user-supplied weight function
				if(!weight_func)
					w = graph.GetWeight(vertidx, neighbouridx);
				else
					w = (*weight_func)(vertidx, neighbouridx);

				if(!w)
					continue;

				if(dists(i-1, neighbouridx) + *w < dists(i, vertidx))
				{
					dists(i, vertidx) = dists(i-1, neighbouridx) + *w;
					predecessors[vertidx] = neighbouridx;
				}
			}
		}
	}

	return std::make_tuple(dists, predecessors);
}


/**
 * floyd-warshall algorithm
 * @see (FUH 2021), Kurseinheit 4, p. 23
 */
template<class t_graph, class t_mat=tl2::mat<typename t_graph::t_weight, std::vector>>
requires is_graph<t_graph> && tl2::is_mat<t_mat>
t_mat floyd(const t_graph& graph)
{
	// distances
	const std::size_t N = graph.GetNumVertices();
	using t_weight = typename t_graph::t_weight;
	t_mat dists = tl2::zero<t_mat>(N, N);
	t_mat next_dists = tl2::zero<t_mat>(N, N);


	// don't use the full maximum to prevent overflows when we're adding the weight afterwards
	const t_weight infinity = std::numeric_limits<t_weight>::max() / 2;

	// initial weights
	for(std::size_t vertidx1=0; vertidx1<N; ++vertidx1)
	{
		std::vector<std::size_t> neighbours = graph.GetNeighbours(vertidx1);

		for(std::size_t vertidx2=0; vertidx2<N; ++vertidx2)
		{
			if(vertidx2 == vertidx1)
				continue;

			// is vertidx2 a direct neighbour of vertidx1?
			if(std::find(neighbours.begin(), neighbours.end(), vertidx2) != neighbours.end())
				dists(vertidx1, vertidx2) = *graph.GetWeight(vertidx1, vertidx2);
			else
				dists(vertidx1, vertidx2) = infinity;
		}
	}


	// iterate vertices
	for(std::size_t i=1; i<N; ++i)
	{
		for(std::size_t vertidx1=0; vertidx1<N; ++vertidx1)
		{
			for(std::size_t vertidx2=0; vertidx2<N; ++vertidx2)
			{
				t_weight dist1 = dists(vertidx1, vertidx2);
				t_weight dist2 = dists(vertidx1, i) + dists(i, vertidx2);

				next_dists(vertidx1, vertidx2) = std::min(dist1, dist2);
			}
		}

		std::swap(dists, next_dists);
	}

	return dists;
}


/**
 * removes the first N elements of a edge tuple
 */
template<std::size_t N, class... t_args, std::size_t... indices,
	template<class ...> class t_tup = std::tuple>
auto _tuple_tail(const t_tup<t_args...>& tup, std::index_sequence<indices...>)
{
	return std::make_tuple( std::get<indices + N>(tup) ... );
}


/**
 * removes the first N elements of a edge tuple
 */
template<std::size_t N, class... t_args,
	template<class ...> class t_tup = std::tuple>
auto tuple_tail(const t_tup<t_args...>& tup)
{
	return _tuple_tail<N, t_args...>(
		tup, std::make_index_sequence<sizeof...(t_args) - N>());
}


/**
 * gets the tuple elements specified in the index sequence
 */
template<class... t_args, std::size_t... indices,
	template<class ...> class t_tup = std::tuple>
auto _tuple_elems(const t_tup<t_args...>& tup, std::index_sequence<indices...>)
{
	return std::make_tuple( std::get<indices>(tup) ... );
}


/**
 * keeps the first N elements of a edge tuple
 */
template<std::size_t N, class... t_args,
	template<class ...> class t_tup = std::tuple>
auto tuple_head(const t_tup<t_args...>& tup)
{
	return _tuple_elems<t_args...>(tup, std::make_index_sequence<N>());
}


/**
 * converts the first two elements of a tuple to a pair
 */
template<class t_1, class t_2,
	template<class ...> class t_tup = std::tuple>
std::pair<t_1, t_2> to_pair(const t_tup<t_1, t_2>& tup)
{
	return std::make_pair(std::get<0>(tup), std::get<1>(tup));
}


/**
 * finds loops in an undirected graph
 */
template<class t_edge = std::tuple<std::size_t, std::size_t, unsigned int>>
bool has_loops(
	const std::vector<t_edge>& edges,
	std::size_t start_from, std::size_t start_to)
{
	// [from, to]
	std::stack<t_edge> tovisit;
	tovisit.push(std::make_tuple(start_from, start_to, 0));

	std::set<std::size_t> visitedverts;
	visitedverts.insert(start_from);

	using t_simpleedge = std::pair<
		typename std::tuple_element<0, t_edge>::type,
		typename std::tuple_element<1, t_edge>::type>;
	std::set<t_simpleedge> visitededges;

	// visit connected vertices
	while(!tovisit.empty())
	{
		auto [vertfrom, vertto, weight] = tovisit.top();
		tovisit.pop();

		auto topedge = std::make_pair(vertfrom, vertto);

		if(visitededges.find(topedge) != visitededges.end())
			continue;

		visitededges.emplace(std::move(topedge));
		visitededges.insert(std::make_pair(vertto, vertfrom));

		// has this vertex already been visited? => loop in graph
		if(visitedverts.find(vertto) != visitedverts.end())
			return true;

		visitedverts.insert(vertto);

		// get all edges from current vertex
		for(auto iter=edges.begin(); iter!=edges.end(); ++iter)
		{
			auto resttup = tuple_tail<2>(*iter);

			// forward direction
			if(std::get<0>(*iter) == vertto)
			{
				auto tup = std::tuple_cat(std::make_tuple(
					std::get<0>(*iter), std::get<1>(*iter)),
					resttup);
				tovisit.emplace(std::move(tup));
			}

			// backward direction
			if(std::get<1>(*iter) == vertto)
			{
				auto tup = std::tuple_cat(std::make_tuple(
					std::get<1>(*iter), std::get<0>(*iter)),
					resttup);
				tovisit.emplace(std::move(tup));
			}
		}
	}

	return false;
}


/**
 * minimal spanning tree
 * @see (FUH 2020), ch. 5.2.3, pp. 221-224
 * @see https://de.wikipedia.org/wiki/Algorithmus_von_Kruskal
 */
template<class t_edge = std::tuple<std::size_t, std::size_t, unsigned int>>
std::vector<t_edge> calc_min_spantree(const std::vector<t_edge>& _edges)
{
	// sort edges by weight
	std::vector<t_edge> edges = _edges;
	std::stable_sort(edges.begin(), edges.end(),
		[](const t_edge& edge1, const t_edge& edge2) -> bool
		{
			return std::get<2>(edge1) >= std::get<2>(edge2);
		});

	std::vector<t_edge> span;
	span.reserve(edges.size());

	while(edges.size())
	{
		t_edge edge = std::move(edges.back());
		edges.pop_back();

		span.push_back(edge);
		if(has_loops<t_edge>(span, std::get<0>(edge), std::get<1>(edge)))
			span.pop_back();
	}

	return span;
}


/**
 * minimal spanning tree for vectors
 * @see (FUH 2020), ch. 5.2.3, pp. 221-224
 * @see https://de.wikipedia.org/wiki/Algorithmus_von_Kruskal
 */
template<class t_vec, class t_edge = std::pair<std::size_t, std::size_t>>
std::vector<t_edge> calc_min_spantree(
	const std::vector<t_vec>& verts,
	const std::vector<t_edge>& _edges)
requires tl2::is_vec<t_vec>
{
	using t_real = typename t_vec::value_type;
	using t_weighted_edge = std::tuple<
		typename t_edge::first_type,
		typename t_edge::second_type,
		t_real>;

	// get weights from edge lengths
	std::vector<t_weighted_edge> edges;
	edges.reserve(_edges.size());
	for(std::size_t i=0; i<_edges.size(); ++i)
	{
		t_vec dir = verts[_edges[i].first] - verts[_edges[i].second];
		t_real lensq = tl2::inner(dir, dir);

		t_weighted_edge edge = std::make_tuple(_edges[i].first, _edges[i].second, lensq);
		edges.emplace_back(std::move(edge));
	}

	// convert to t_edge vector
	auto to_simple_edges = [](const std::vector<t_weighted_edge>& edges) -> std::vector<t_edge>
	{
		std::vector<t_edge> simpleedges;
		simpleedges.reserve(edges.size());

		for(const auto& edge : edges)
			simpleedges.emplace_back(to_pair(tuple_head<2>(edge)));

		return simpleedges;
	};

	return to_simple_edges(calc_min_spantree<t_weighted_edge>(edges));
}


#ifdef USE_BOOST_GRAPH
/**
 * minimum spanning tree using boost.graph for comparison
 * @see http://www.boost.org/doc/libs/1_65_1/libs/graph/doc/table_of_contents.html
 * @see https://github.com/boostorg/graph/tree/develop/example
 */
template<class t_vec, class t_edge = std::pair<std::size_t, std::size_t>>
std::vector<t_edge> calc_min_spantree_boost(const std::vector<t_vec>& verts)
requires tl2::is_vec<t_vec>
{
	using t_real = typename t_vec::value_type;
	struct t_edge_weight
	{
		t_edge_weight() = default;
		t_edge_weight(t_real weight) : weight(weight) {}

		t_real weight{1};
	};

	using t_graph = boost::adjacency_list<
		boost::vecS, boost::vecS, boost::undirectedS, t_vec, t_edge_weight>;
	using t_edge_descr = typename boost::graph_traits<t_graph>::edge_descriptor;

	t_graph graph;
	auto weight = boost::get(&t_edge_weight::weight, graph);

	for(const t_vec& vert : verts)
		boost::add_vertex(vert, graph);

	for(std::size_t i=0; i<verts.size(); ++i)
	{
		const t_vec& vert1 = verts[i];
		for(std::size_t j=i+1; j<verts.size(); ++j)
		{
			const t_vec& vert2 = verts[j];
			t_real dist = tl2::norm(vert2 - vert1);

			boost::add_edge(
				boost::vertex(i, graph),
				boost::vertex(j, graph),
				t_edge_weight{dist},
				graph);
		}
	}

	std::vector<t_edge_descr> spanning_edges;
	boost::kruskal_minimum_spanning_tree(graph,
		std::back_inserter(spanning_edges),
		boost::weight_map(weight));

	std::vector<t_edge> span;
	span.reserve(spanning_edges.end() - spanning_edges.begin());
	for(auto iter=spanning_edges.begin(); iter!=spanning_edges.end(); ++iter)
	{
		std::size_t idx1 = boost::source(*iter, graph);
		std::size_t idx2 = boost::target(*iter, graph);
		span.emplace_back(std::make_pair(idx1, idx2));
	}

	return span;
}
#endif
// ----------------------------------------------------------------------------


}
#endif
