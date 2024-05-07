/**
 * tree containers, concepts and algorithms
 * @author Tobias Weber <tweber@ill.fr>
 * @date November 2020 - June 2021
 * @note Forked on 19-apr-2021 and 3-jun-2021 from my privately developed "geo" project (https://github.com/t-weber/geo).
 * @license GPLv3, see 'LICENSE' file
 *
 * References:
 *   - (Klein 2005) R. Klein, "Algorithmische Geometrie" (2005),
 *                  ISBN: 978-3540209560 (http://dx.doi.org/10.1007/3-540-27619-X).
 *   - (FUH 2020) R. Klein, C. Icking, "Algorithmische Geometrie" (2020),
 *                Kurs 1840, Fernuni Hagen (https://vu.fernuni-hagen.de/lvuweb/lvu/app/Kurs/1840).
 *   - (FUH 2021) A. Schulz, J. Rollin, "Effiziente Algorithmen" (2021),
 *                Kurs 1684, Fernuni Hagen (https://vu.fernuni-hagen.de/lvuweb/lvu/app/Kurs/01684).
 *   - (Berg 2008) M. de Berg, O. Cheong, M. van Kreveld, M. Overmars, "Computational Geometry" (2008),
 *                 ISBN: 978-3-642-09681-5 (http://dx.doi.org/10.1007/978-3-540-77974-2).
 *   - https://www.boost.org/doc/libs/1_74_0/doc/html/intrusive/node_algorithms.html
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

#ifndef __GEO_CONTAINERS_H__
#define __GEO_CONTAINERS_H__

#include <type_traits>
#include <concepts>
#include <string>
#include <sstream>
#include <memory>
#include <optional>
#include <unordered_map>
#include <cstdint>
#include <iostream>

#include <boost/intrusive/bstree_algorithms.hpp>
#include <boost/intrusive/avltree_algorithms.hpp>
#include <boost/intrusive/rbtree_algorithms.hpp>
#include <boost/intrusive/splaytree_algorithms.hpp>
#include <boost/intrusive/treap_algorithms.hpp>

#include "tlibs2/libs/maths.h"


/**
 * range tree's underlying binary tree:
 * 	1: binary search tree
 * 	2: avl tree
 * 	3: red-black tree
 * 	4: splay tree
 */
#ifndef __RANGE_TREE_UNDERLYING_IMPL
	#define __RANGE_TREE_UNDERLYING_IMPL 2
#endif


namespace geo {

// ----------------------------------------------------------------------------
// concepts
// ----------------------------------------------------------------------------

/**
 * requirements for a basic vector container like std::vector
 */
template<class T>
concept is_tree_node = requires(const T& a)
{
	// tree hierarchy
	a.parent;
	a.left;
	a.right;
};
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// common classes / functions
// ----------------------------------------------------------------------------

/**
 * common node type
 */
template<class t_nodetype>
struct CommonTreeNode
{
	// tree hierarchy
	t_nodetype *parent = nullptr;
	t_nodetype *left = nullptr;
	t_nodetype *right = nullptr;


	CommonTreeNode() = default;
	virtual ~CommonTreeNode() = default;

	CommonTreeNode(const CommonTreeNode<t_nodetype>& other)
	{
		*this = operator=(other);
	}

	CommonTreeNode<t_nodetype>&
	operator=(const CommonTreeNode<t_nodetype>& other)
	{
		this->parent = other.parent;
		this->left = other.left;
		this->right = other.right;

		return *this;
	}

	virtual std::string description() const
	{
		return "";
	}
};


/**
 * common node traits
 * @see https://www.boost.org/doc/libs/1_74_0/doc/html/intrusive/node_algorithms.html
 */
template<class t_tree_node>
requires is_tree_node<t_tree_node>
struct BasicNodeTraits
{
	using node = t_tree_node;
	using node_ptr = node*;
	using const_node_ptr = const node*;

	static node* get_parent(const node* thenode)
	{
		if(!thenode) return nullptr;
		return thenode->parent;
	}

	static node* get_left(const node* thenode)
	{
		if(!thenode) return nullptr;
		return thenode->left;
	}

	static node* get_right(const node* thenode)
	{
		if(!thenode) return nullptr;
		return thenode->right;
	}

	static void set_parent(node* thenode, node* parent)
	{
		if(!thenode) return;
		thenode->parent = parent;
	}

	static void set_left(node* thenode, node* left)
	{
		if(!thenode) return;
		thenode->left = left;
	}

	static void set_right(node* thenode, node* right)
	{
		if(!thenode) return;
		thenode->right = right;
	}
};


/**
 * node traits
 * @see https://www.boost.org/doc/libs/1_74_0/doc/html/intrusive/node_algorithms.html
 */
template<class t_tree_node>
requires is_tree_node<t_tree_node>
struct BinTreeNodeTraits : public BasicNodeTraits<t_tree_node>
{
	using node = typename BasicNodeTraits<t_tree_node>::node;
	using node_ptr = typename BasicNodeTraits<t_tree_node>::node_ptr;
	using const_node_ptr = typename BasicNodeTraits<t_tree_node>::const_node_ptr;

	using balance = typename node::t_balance;

	static balance positive() { return 1; }
	static balance negative() { return -1; }
	static balance zero() { return 0; }

	static balance get_balance(const node* thenode)
	{
		if(!thenode) return zero();
		return thenode->balance;
	}

	static void set_balance(node* thenode, balance bal)
	{
		if(!thenode) return;
		thenode->balance = bal;
	}
};


/**
 * node traits
 * @see https://www.boost.org/doc/libs/1_74_0/doc/html/intrusive/node_algorithms.html
 */
template<class t_tree_node>
requires is_tree_node<t_tree_node>
struct RbTreeNodeTraits : public BasicNodeTraits<t_tree_node>
{
	using node = typename BasicNodeTraits<t_tree_node>::node;
	using node_ptr = typename BasicNodeTraits<t_tree_node>::node_ptr;
	using const_node_ptr = typename BasicNodeTraits<t_tree_node>::const_node_ptr;

	using color = typename node::t_colour;

	static color red() { return 1; }
	static color black() { return 0; }

	static color get_color(const node* thenode)
	{
		if(!thenode) return black();
		return thenode->colour;
	}

	static void set_color(node* thenode, color col)
	{
		if(!thenode) return;
		thenode->colour = col;
	}
};


/**
 * output the graph in the DOT format
 * @see https://en.wikipedia.org/wiki/DOT_(graph_description_language)
 */
template<class t_node>
void write_graph(std::ostream& ostrStates, std::ostream& ostrTransitions,
	const std::unordered_map<const t_node*, std::size_t>& nodeMap, const t_node* node)
requires is_tree_node<t_node>
{
	using namespace tl2_ops;
	if(!node) return;

	std::string descr = node->description();
	std::size_t num = nodeMap.find(node)->second;
	ostrStates << "\t" << num << " [label=\"";
	if(descr == "")
		ostrStates << num;
	else
		ostrStates << descr;
	ostrStates << "\"];\n";

	if(node->left)
	{
		std::size_t numleft = nodeMap.find(node->left)->second;
		ostrTransitions << "\t" << num << ":sw -> " << numleft << ":n [label=\"l\"];\n";

		write_graph<t_node>(ostrStates, ostrTransitions, nodeMap, node->left);
	}

	if(node->right)
	{
		std::size_t numright = nodeMap.find(node->right)->second;
		ostrTransitions << "\t" << num << ":se -> " << numright << ":n [label=\"r\"];\n";

		write_graph<t_node>(ostrStates, ostrTransitions, nodeMap, node->right);
	}
}


/**
 * number and save all unique graph nodes in a map
 */
template<class t_node>
void number_nodes(std::unordered_map<const t_node*, std::size_t>& map, const t_node* node, std::size_t &num)
requires is_tree_node<t_node>
{
	if(!node) return;

	if(auto iter = map.find(node); iter==map.end())
		map.emplace(std::make_pair(node, num++));

	number_nodes<t_node>(map, node->left, num);
	number_nodes<t_node>(map, node->right, num);
}


/**
 * output the graph in the DOT format
 * @see https://en.wikipedia.org/wiki/DOT_(graph_description_language)
 */
template<class t_node>
void write_graph(std::ostream& ostr, const t_node* node)
requires is_tree_node<t_node>
{
	std::ostringstream ostrStates;
	std::ostringstream ostrTransitions;

	ostrStates.precision(ostr.precision());
	ostrTransitions.precision(ostr.precision());

	std::unordered_map<const t_node*, std::size_t> nodeNumbers;
	std::size_t nodeNum = 0;
	number_nodes(nodeNumbers, node, nodeNum);

	write_graph<t_node>(ostrStates, ostrTransitions, nodeNumbers, node);

	ostr << "// directed graph\ndigraph tree\n{\n";
	ostr << "\tgraph [fontname = \"DejaVuSans\"]\n"
		<< "\tnode [fontname = \"DejaVuSans\"]\n"
		<< "\tedge [fontname = \"DejaVuSans\"]\n";

	ostr << "\n\t// states\n";
	ostr << ostrStates.str();
	ostr << "\n\t// transitions\n";
	ostr << ostrTransitions.str();
	ostr << "\n}\n";
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// range tree
// see (Klein 2005), ch. 3.3.2 pp. 135f.
// see (Berg 2008), pp. 105-110.
// ----------------------------------------------------------------------------

template<class t_vec> requires tl2::is_basic_vec<t_vec> class RangeTree;

/**
 * range tree node
 */
template<class t_vec>
requires tl2::is_basic_vec<t_vec>
struct RangeTreeNode : public CommonTreeNode<RangeTreeNode<t_vec>>
{
	using t_real = typename t_vec::value_type;

#if __RANGE_TREE_UNDERLYING_IMPL==1 || __RANGE_TREE_UNDERLYING_IMPL==2
	// bstree or avltree
	using t_balance = std::int64_t;
	t_balance balance = 0;
#elif __RANGE_TREE_UNDERLYING_IMPL==3
	// rbtree
	using t_colour = std::int8_t;
	t_colour colour = 0;
#endif

	// range tree for idx+1
	std::shared_ptr<RangeTree<t_vec>> nextidxtree{};

	// dimension of data and current index
	std::size_t dim = 0;
	std::size_t idx = 0;

	// range for current index
	t_real range[2] = { 0., 0. };

	// pointer to actual data
	std::shared_ptr<const t_vec> vec{};


	RangeTreeNode() = default;
	virtual ~RangeTreeNode() = default;

	RangeTreeNode(
		const std::shared_ptr<const t_vec>& vec,
		std::size_t dim, std::size_t idx=0)
			: CommonTreeNode<RangeTreeNode<t_vec>>{},
				dim{dim}, idx{idx}, vec{vec}
	{}


	/**
	 * get all node vectors in a linear fashion
	 */
	static void get_vecs(
		const RangeTreeNode<t_vec>* node,
		std::vector<std::shared_ptr<const t_vec>>& vecs,
		const t_vec* min=nullptr, const t_vec* max=nullptr)
	{
		auto is_in_range = [](
			const t_vec& vec,
			const t_vec& min, const t_vec& max,
			std::size_t dim) -> bool
		{
			for(std::size_t idx=0; idx<dim; ++idx)
			{
				if(vec[idx] < min[idx] || vec[idx] > max[idx])
					return false;
			}
			return true;
		};

		if(node->left)
			get_vecs(node->left, vecs, min, max);

		bool in_range = true;
		if(min && max)
			in_range = is_in_range(*node->vec, *min, *max, node->dim);
		if(in_range)
			vecs.push_back(node->vec);

		if(node->right)
			get_vecs(node->right, vecs, min, max);
	}


	void print(std::ostream& ostr, std::size_t indent=0) const
	{
		using namespace tl2_ops;
		const RangeTreeNode<t_vec> *node = this;

		ostr << "ptr: " << (void*)node;
		ostr << ", vec: ";
		if(node->vec)
			ostr << *node->vec;
		else
			ostr << "null";
		ostr << ", idx: " << node->idx;
		ostr << ", range: " << node->range[0] << ".." << node->range[1] << "\n";

		if(node->left || node->right)
		{
			for(std::size_t i=0; i<indent+1; ++i)
				ostr << "  ";
			ostr << "left: ";
			if(node->left)
				node->left->print(ostr, indent+1);
			else
				ostr << "nullptr\n";

			for(std::size_t i=0; i<indent+1; ++i)
				ostr << "  ";
			ostr << "right: ";
			if(node->right)
				node->right->print(ostr, indent+1);
			else
				ostr << "nullptr\n";
		}
	}


	friend std::ostream& operator<<(std::ostream& ostr, const RangeTreeNode<t_vec>& node)
	{
		node.print(ostr);
		return ostr;
	}


	friend bool operator<(const RangeTreeNode<t_vec>& e1, const RangeTreeNode<t_vec>& e2)
	{
		// compare by idx
		return (*e1.vec)[e1.idx] < (*e2.vec)[e2.idx];
	}
};


/**
 * k-dim range tree
 * @see (Klein 2005), ch. 3.3.2, pp. 135f
 * @see (Berg 2008), pp. 105-110
 */
template<class t_vec>
requires tl2::is_basic_vec<t_vec>
class RangeTree
{
public:
	using t_node = RangeTreeNode<t_vec>;

#if __RANGE_TREE_UNDERLYING_IMPL==1
	// bstree
	using t_nodetraits = BinTreeNodeTraits<t_node>;
	using t_treealgos = boost::intrusive::bstree_algorithms<t_nodetraits>;
#elif __RANGE_TREE_UNDERLYING_IMPL==2
	// avltree
	using t_nodetraits = BinTreeNodeTraits<t_node>;
	using t_treealgos = boost::intrusive::avltree_algorithms<t_nodetraits>;
#elif __RANGE_TREE_UNDERLYING_IMPL==3
	// rbtree
	using t_nodetraits = RbTreeNodeTraits<t_node>;
	using t_treealgos = boost::intrusive::rbtree_algorithms<t_nodetraits>;
#elif __RANGE_TREE_UNDERLYING_IMPL==4
	// sgtree
	using t_nodetraits = BasicNodeTraits<t_node>;
	using t_treealgos = boost::intrusive::splaytree_algorithms<t_nodetraits>;
#endif


public:
	RangeTree(std::size_t idx=0) : m_idx{idx}
	{
		t_treealgos::init_header(&m_root);
	}

	~RangeTree()
	{
		free_nodes(get_root());
	}


	/**
	 * query a rectangular range
	 */
	std::vector<std::shared_ptr<const t_vec>>
	query_range(const t_vec& _min, const t_vec& _max)
	{
		auto is_in_range =
		[](const t_node* node, const t_vec& min, const t_vec& max) -> bool
		{
			const std::size_t idx = node->idx;
			return node->range[0] <= min[idx] && node->range[1] >= max[idx];
		};

		const t_node* node = get_root();
		t_vec min = _min, max = _max;

		// iterate coordinate sub-trees
		while(true)
		{
			// fit query rectangle to range
			if(min[node->idx] < node->range[0])
				min[node->idx] = node->range[0];
			if(max[node->idx] > node->range[1])
				max[node->idx] = node->range[1];

			if(!is_in_range(node, min, max))
			{
				return {};
			}
			else
			{
				// descend tree to find the smallest fitting range
				while(1)
				{
					bool updated = false;
					if(node->left && is_in_range(node->left, min, max))
					{
						node = node->left;
						updated = true;
					}
					else if(node->right && is_in_range(node->right, min, max))
					{
						node = node->right;
						updated = true;
					}

					// no more updates
					if(!updated)
						break;
				}
			}

			if(!node->nextidxtree)
				break;

			node = node->nextidxtree->get_root();
			if(!node)
				break;
		}

		std::vector<std::shared_ptr<const t_vec>> vecs;
		t_node::get_vecs(node, vecs, &min, &max);
		return vecs;
	}


	/**
	 * insert a collection of vectors
	 */
	void insert(const std::vector<t_vec>& vecs)
	{
		for(const t_vec& vec : vecs)
			insert(vec);
		update();
	}

	/**
	 * insert a collection of vectors
	 */
	void insert(const std::vector<std::shared_ptr<const t_vec>>& vecs)
	{
		for(const std::shared_ptr<const t_vec>& vec : vecs)
			insert(vec);
		update();
	}

	/**
	 * insert a vector
	 */
	void insert(const t_vec& vec)
	{
		t_node* node = new t_node{std::make_shared<t_vec>(vec), vec.size(), m_idx};
		insert(node);
	}

	/**
	 * insert a vector
	 */
	void insert(const std::shared_ptr<const t_vec>& vec)
	{
		t_node* node = new t_node{vec, vec->size(), m_idx};
		insert(node);
	}


	const t_node* get_root() const
	{
		return t_treealgos::root_node(&m_root);
	}

	t_node* get_root()
	{
		return t_treealgos::root_node(&m_root);
	}


	/**
	 * update all ranges
	 */
	void update()
	{
		update(get_root());
	}


	/**
	 * update the node's ranges
	 */
	static void update(t_node* node)
	{
		if(!node) return;

		t_node* left = node->left;
		t_node* right = node->right;

		if(left) update(left);
		if(right) update(right);


		// --------------------------------------------------------------------
		// ranges
		// --------------------------------------------------------------------
		// leaf node
		if(!left && !right && node->vec)
			node->range[0] = node->range[1] = (*node->vec)[node->idx];

		if(left && !right)
		{
			node->range[0] = left->range[0];
			if(node->vec)
				node->range[1] = (*node->vec)[node->idx];
			else
				node->range[1] = left->range[1];
		}

		if(right && !left)
		{
			node->range[1] = right->range[1];
			if(node->vec)
				node->range[0] = (*node->vec)[node->idx];
			else
				node->range[0] = right->range[0];
		}

		if(left && right)
		{
			node->range[0] = left->range[0];
			node->range[1] = right->range[1];
		}
		// --------------------------------------------------------------------


		// --------------------------------------------------------------------
		// subtree for next index
		// --------------------------------------------------------------------
		if(node->idx+1 < node->dim)
		{
			node->nextidxtree = std::make_shared<RangeTree<t_vec>>(node->idx+1);

			std::vector<std::shared_ptr<const t_vec>> vecs;
			RangeTreeNode<t_vec>::get_vecs(node, vecs);

			node->nextidxtree->insert(vecs);
		}
		// --------------------------------------------------------------------
	}


	friend std::ostream& operator<<(std::ostream& ostr, const RangeTree<t_vec>& tree)
	{
		ostr << *tree.get_root();
		return ostr;
	}


protected:
	/**
	 * insert a node
	 */
	void insert(t_node* node)
	{
		t_treealgos::insert_equal(&m_root, get_root(), node,
			[](const t_node* node1, const t_node* node2) -> bool
			{
				return *node1 < *node2;
			});
	}


	static void free_nodes(t_node* node)
	{
		if(!node) return;

		if(node->left && node->left != node)
		{
			free_nodes(node->left);
			delete node->left;
			node->left = nullptr;
		}

		if(node->right && node->right != node)
		{
			free_nodes(node->right);
			delete node->right;
			node->right = nullptr;
		}
	}


private:
	t_node m_root{};
	std::size_t m_idx = 0;
};

// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// treap
// @see https://en.wikipedia.org/wiki/Treap
// @see (Berg 2008), pp. 226-230
// @see (FUH 2020), ch. 4.2.4, pp. 188-192
// ----------------------------------------------------------------------------

template<class t_vec>
requires tl2::is_basic_vec<t_vec>
struct TreapNode : public CommonTreeNode<TreapNode<t_vec>>
{
	// pointer to actual data
	std::shared_ptr<const t_vec> vec{};
};


template<class t_vec>
requires tl2::is_basic_vec<t_vec>
using TreapNodeTraits = BasicNodeTraits<TreapNode<t_vec>>;


/**
 * 2-dim treap: tree in first component, heap in second component
 * @see https://en.wikipedia.org/wiki/Treap
 * @see (Berg 2008), pp. 226-230
 * @see (FUH 2020), ch. 4.2.4, pp. 188-192
 */
template<class t_vec>
requires tl2::is_basic_vec<t_vec>
class Treap
{
public:
	using t_node = TreapNode<t_vec>;
	using t_nodetraits = TreapNodeTraits<t_vec>;
	using t_treealgos = boost::intrusive::treap_algorithms<t_nodetraits>;


public:
	Treap()
	{
		t_treealgos::init_header(&m_root);
	}

	~Treap()
	{
		free_nodes(get_root());
	}


	/**
	 * insert a collection of vectors
	 */
	void insert(const std::vector<t_vec>& vecs)
	{
		for(const t_vec& vec : vecs)
			insert(vec);
	}

	/**
	 * insert a vector
	 */
	void insert(const t_vec& vec)
	{
		t_node* node = new t_node;
		node->vec = std::make_shared<t_vec>(vec);
		insert(node);
	}


	const t_node* get_root() const
	{
		return t_treealgos::root_node(&m_root);
	}

	t_node* get_root()
	{
		return t_treealgos::root_node(&m_root);
	}


protected:
	/**
	 * insert a node
	 */
	void insert(t_node* node)
	{
		t_treealgos::insert_equal(&m_root, get_root(), node,
			[](const t_node* node1, const t_node* node2) -> bool
			{	// sorting for first component (tree)
				return (*node1->vec)[0] < (*node2->vec)[0];
			},
			[](const t_node* node1, const t_node* node2) -> bool
			{	// sorting for second component (heap)
				return (*node1->vec)[1] < (*node2->vec)[1];
			});
	}


	static void free_nodes(t_node* node)
	{
		if(!node) return;

		if(node->left && node->left != node)
		{
			free_nodes(node->left);
			delete node->left;
			node->left = nullptr;
		}

		if(node->right && node->right != node)
		{
			free_nodes(node->right);
			delete node->right;
			node->right = nullptr;
		}
	}


private:
	t_node m_root{};
	std::size_t m_idx = 0;
};

// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// k-d tree
// @see (Klein 2005), ch. 3.3.1, pp. 126f
// @see (Berg 2008), pp. 99-105
// @see https://en.wikipedia.org/wiki/K-d_tree
// ----------------------------------------------------------------------------

template<class t_vec> requires tl2::is_basic_vec<t_vec> class KdTree;

/**
 * k-d tree node
 */
template<class t_vec>
requires tl2::is_basic_vec<t_vec>
struct KdTreeNode : public CommonTreeNode<KdTreeNode<t_vec>>
{
	using t_scalar = typename t_vec::value_type;

	using t_balance = std::int64_t;
	t_balance balance = 0;

	// pointer to actual data for leaf nodes
	std::shared_ptr<const t_vec> vec{};

	// coordinate component index offset of splitting plane for inner nodes
	std::size_t split_idx{};
	t_scalar split_value{};


	KdTreeNode() = default;
	virtual ~KdTreeNode() = default;

	KdTreeNode(const std::shared_ptr<const t_vec>& vec)
		: CommonTreeNode<KdTreeNode<t_vec>>{}, vec{vec}
	{}


	void print(std::ostream& ostr, std::size_t indent = 0) const
	{
		using namespace tl2_ops;
		const KdTreeNode<t_vec> *node = this;

		ostr << "ptr: " << (void*)node << ", balance: " << node->balance;
		if(node->vec)
			ostr << ", vec: " << *node->vec;
		else
			ostr << ", split index: " << node->split_idx << ", split value: " << node->split_value;

		if(node->left || node->right)
		{
			ostr << "\n";
			for(std::size_t i=0; i<indent+1; ++i)
				ostr << "  ";
			ostr << "left: ";
			if(node->left)
				node->left->print(ostr, indent+1);
			else
				ostr << "nullptr\n";

			ostr << "\n";
			for(std::size_t i=0; i<indent+1; ++i)
				ostr << "  ";
			ostr << "right: ";
			if(node->right)
				node->right->print(ostr, indent+1);
			else
				ostr << "nullptr\n";
		}
	}


	virtual std::string description() const override
	{
		using namespace tl2_ops;

		std::ostringstream ostr;

		// left node or inner node?
		if(vec)
			ostr << "vertex: " << *vec;
		else
			ostr << "split index: " << split_idx
				<< "\nsplit value: " << split_value;

		return ostr.str();
	}


	friend std::ostream& operator<<(std::ostream& ostr, const KdTreeNode<t_vec>& node)
	{
		node.print(ostr);
		return ostr;
	}
};



/**
 * k-d tree
 * @see (Klein 2005), ch. 3.3.1, pp. 126f
 * @see (Berg 2008), pp. 99-105
 * @see https://en.wikipedia.org/wiki/K-d_tree
 */
template<class t_vec>
requires tl2::is_basic_vec<t_vec>
class KdTree
{
public:
	using t_node = KdTreeNode<t_vec>;
	using t_scalar = typename t_vec::value_type;

	using t_nodetraits = BinTreeNodeTraits<t_node>;
	using t_treealgos = boost::intrusive::bstree_algorithms<t_nodetraits>;


public:
	KdTree(std::size_t dim = 3) : m_dim{dim}
	{
		t_treealgos::init_header(&m_root);
	}

	~KdTree()
	{
		clear();
	}

	// TODO
	KdTree(const KdTree<t_vec>& other) = delete;
	KdTree<t_vec>& operator=(const KdTree<t_vec>& other) = delete;

	KdTree(KdTree<t_vec>&& other)
	{
		this->operator=(std::forward<KdTree<t_vec>&&>(other));
	}

	KdTree<t_vec>& operator=(KdTree<t_vec>&& other)
	{
		other.m_ownsroot = false;
		this->m_root = std::move(other.m_root);
		this->m_dim = other.m_dim;
		return *this;
	}


	void clear()
	{
		if(m_ownsroot)
			free_nodes(get_root());
	}


	const t_node* get_root() const
	{
		return t_treealgos::root_node(&m_root);
	}

	t_node* get_root()
	{
		return t_treealgos::root_node(&m_root);
	}


	/**
	 * create the tree from a collection of vectors
	 */
	void create(const std::vector<t_vec>& _vecs)
	{
		std::vector<std::shared_ptr<t_vec>> vecs;
		vecs.reserve(_vecs.size());

		for(const t_vec& vec : _vecs)
			vecs.emplace_back(std::make_shared<t_vec>(vec));

		create(get_root(), vecs, m_dim);
	}


	const t_node* get_closest(const t_vec& vec) const
	{
		const t_node* closest_node = nullptr;
		t_scalar closest_dist_sq = std::numeric_limits<t_scalar>::max();

		get_closest(get_root(), vec, &closest_node, &closest_dist_sq);
		return closest_node;
	}


	friend std::ostream& operator<<(std::ostream& ostr, const KdTree<t_vec>& tree)
	{
		ostr << *tree.get_root();
		return ostr;
	}


protected:
	/**
	 * create the tree from a collection of points
	 * @see (Berg 2008), pp. 100-101
	 */
	static void create(t_node* node,
		const std::vector<std::shared_ptr<t_vec>>& vecs,
		std::size_t dim = 3, std::size_t depth = 0)
	{
		if(!node || vecs.size() == 0)
			return;

		// create a leaf node
		if(vecs.size() == 1)
		{
			node->vec = vecs[0];
			return;
		}

		node->split_idx = (depth % dim);

		// use the mean value for the splitting plane offset
		t_scalar mean{};
		for(const auto& vec : vecs)
			mean += (*vec)[node->split_idx];
		mean /= t_scalar(vecs.size());
		node->split_value = mean;

		std::vector<std::shared_ptr<t_vec>> left{};
		std::vector<std::shared_ptr<t_vec>> right{};
		left.reserve(vecs.size());
		right.reserve(vecs.size());

		// find the half-spaces of the points and sort them
		// into the respective vectors
		for(const auto& vec : vecs)
		{
			if((*vec)[node->split_idx] <= node->split_value)
				left.push_back(vec);
			else
				right.push_back(vec);
		}

		// create the left and right child nodes
		if(left.size())
		{
			node->left = new t_node{};
			node->left->parent = node;
			create(node->left, left, dim, depth+1);
		}
		if(right.size())
		{
			node->right = new t_node{};
			node->right->parent = node;
			create(node->right, right, dim, depth+1);
		}

		// set balance factor
		decltype(node->balance) left_balance = 0;
		decltype(node->balance) right_balance = 0;
		if(node->left)
			left_balance = node->left->balance + 1;
		if(node->right)
			right_balance = node->right->balance + 1;
		node->balance = left_balance - right_balance;
	}


	/**
	 * delete node and its child nodes
	 */
	static void free_nodes(t_node* node)
	{
		if(!node) return;

		if(node->left && node->left != node)
		{
			free_nodes(node->left);
			delete node->left;
			node->left = nullptr;
		}

		if(node->right && node->right != node)
		{
			free_nodes(node->right);
			delete node->right;
			node->right = nullptr;
		}
	}


	/**
	 * look for the closest node
	 * @see https://en.wikipedia.org/wiki/K-d_tree#Nearest_neighbour_search
	 */
	static void get_closest(const t_node* node, const t_vec& vec,
		const t_node** closest_node, t_scalar* closest_dist_sq)
	{
		if(!node) return;

		// at leaf node?
		if(node->vec)
		{
			// get leaf node closest to query point so far
			t_scalar dist_sq = tl2::inner<t_vec>(vec-*node->vec, vec-*node->vec);
			if(dist_sq < *closest_dist_sq)
			{
				*closest_dist_sq = dist_sq;
				*closest_node = node;
			}
		}

		t_scalar dist_node_plane = vec[node->split_idx] - node->split_value;
		if(*closest_dist_sq >= dist_node_plane*dist_node_plane)
		{ // the distance between the query point and the node is greater
		  // than the distance between the node and the splitting plane
		  //   => need to continue looking on both sides
			get_closest(node->left, vec, closest_node, closest_dist_sq);
			if(dist_node_plane*dist_node_plane < *closest_dist_sq)
				get_closest(node->right, vec, closest_node, closest_dist_sq);
		}
		else
		{ // the distance between the query point and the node is less
		  // than the distance between the node and the splitting plane
		  //   => only need to continue on one side
			if(dist_node_plane <= 0.) // only continue on one side of the plane
				get_closest(node->left, vec, closest_node, closest_dist_sq);
			else
				get_closest(node->right, vec, closest_node, closest_dist_sq);
		}
	}


private:
	bool m_ownsroot{true};
	t_node m_root{};
	std::size_t m_dim{3};
};

// ----------------------------------------------------------------------------

}

#endif
