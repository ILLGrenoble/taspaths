/**
 * global type definitions
 * @author Tobias Weber <tweber@ill.fr>
 * @date feb-2021
 * @license GPLv3, see 'LICENSE' file
 */

#ifndef __PATHS_GLOBAL_TYPES_H__
#define __PATHS_GLOBAL_TYPES_H__


#include "tlibs2/libs/maths.h"


/**
 * fixed-size array wrapper
 */
template<class T, std::size_t N>
class t_arr : public std::array<T, N>
{
public:
	using allocator_type = void;

public:
	constexpr t_arr() = default;
	constexpr ~t_arr() = default;

	// dummy constructor to fulfill interface requirements
	constexpr t_arr(std::size_t) {};

	// copy constructor
	constexpr t_arr(const t_arr& other) : std::array<T, N>(other) {}

	constexpr t_arr& operator=(const t_arr& other)
	{
		std::array<T, N>::operator=(other);
		return *this;
	}

	// move constructor
	//t_arr(t_arr&& other) : std::array<T, N>(std::forward<t_arr&&>(other)) {}
};


template<class T> using t_arr2 = t_arr<T, 2>;
template<class T> using t_arr4 = t_arr<T, 4>;


using t_real = double;

// dynamic container types
using t_vec = tl2::vec<t_real, std::vector>;
using t_mat = tl2::mat<t_real, std::vector>;

// static container types
using t_vec2 = tl2::vec<t_real, t_arr2>;
using t_mat22 = tl2::mat<t_real, t_arr4>;


#endif
