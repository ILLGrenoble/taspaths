/**
 * image processing concepts, containers and algorithms
 * @author Tobias Weber <tweber@ill.fr>
 * @date May 2021
 * @note Forked on 26-may-2021 from my privately developed "misc" project (https://github.com/t-weber/misc).
 * @license GPLv3, see 'LICENSE' file
 *
 * References for the GIL image library:
 *  - https://www.boost.org/doc/libs/1_69_0/libs/gil/doc/html/tutorial.html
 *
 * References for the spatial index tree:
 *  - https://www.boost.org/doc/libs/1_76_0/libs/geometry/doc/html/index.html
 *  - https://www.boost.org/doc/libs/1_76_0/libs/geometry/doc/html/geometry/spatial_indexes/rtree_examples.html
 *  - https://github.com/boostorg/geometry/tree/develop/example
 *
 * ----------------------------------------------------------------------------
 * TAS-Paths (part of the Takin software suite)
 * Copyright (C) 2021       Tobias WEBER (Institut Laue-Langevin (ILL),
 *                          Grenoble, France).
 * "misc" project
 * Copyright (C) 2017-2021  Tobias WEBER (privately developed).
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
#ifndef __GEO_ALGOS_IMG_H__
#define __GEO_ALGOS_IMG_H__

/**
 * which index tree to use for finding the closest obstacles
 * 1: r* tree
 * 2: kd tree
 */
#define GEO_OBSTACLES_INDEX_TREE 1


#include <concepts>
#include <vector>
#include <cstdlib>

#ifdef USE_GIL
	#include <boost/gil/image.hpp>
#endif

#if GEO_OBSTACLES_INDEX_TREE == 1
	#include <boost/geometry.hpp>
	#include <boost/geometry/index/rtree.hpp>
	#include <boost/function_output_iterator.hpp>
#elif GEO_OBSTACLES_INDEX_TREE == 2
	#include "trees.h"
#endif

#ifdef USE_OCV
	#include <opencv2/core.hpp>
	#include <opencv2/imgcodecs.hpp>
	#include <opencv2/imgproc.hpp>
#endif

#include "tlibs2/libs/maths.h"


namespace geo {

// ----------------------------------------------------------------------------
// concepts
// ----------------------------------------------------------------------------
/**
 * requirements for the Image class
 */
template<class t_image>
concept is_image = requires(t_image& img, std::size_t y, std::size_t x)
{
	typename t_image::value_type;

	x = img.GetWidth();
	y = img.GetHeight();

	img.SetPixel(x, y, 0);
	img.GetPixel(x, y);
};


/**
 * requirements for a gil image view
 */
template<class t_imageview>
concept is_imageview = requires(const t_imageview& img, std::size_t y, std::size_t x)
{
	x = img.width();
	y = img.height();

	img.row_begin(y)[x];
};
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// classes
// ----------------------------------------------------------------------------
/**
 * simple image class
 */
template<class t_pixel = bool>
class Image
{
public:
	using value_type = t_pixel;

public:
	Image() = default;
	~Image() = default;

	Image(std::size_t w, std::size_t h)
	{
		Init(w, h);
	}


	Image(const Image<t_pixel>& img)
		: Image(img.GetWidth(), img.GetHeight())
	{
		SetImage(img.m_img.get());
	}


	Image<t_pixel>& operator=(const Image<t_pixel>& img)
	{
		Init(img.GetWidth(), img.GetHeight());
		SetImage(img.m_img.get());

		return *this;
	}


	void Init(std::size_t w, std::size_t h)
	{
		m_width = w;
		m_height = h;
		m_img = std::make_unique<t_pixel[]>(w*h);
	}


	void Clear()
	{
		m_width = 0;
		m_height = 0;
		m_img.reset();
	}


	std::size_t GetWidth() const
	{
		return m_width;
	}


	std::size_t GetHeight() const
	{
		return m_height;
	}


	t_pixel GetPixel(std::size_t x, std::size_t y) const
	{
		if(x < GetWidth() && y < GetHeight())
			return m_img[y*m_width + x];
		return t_pixel{};
	}


	void SetPixel(std::size_t x, std::size_t y, t_pixel pix)
	{
		if(x < GetWidth() && y < GetHeight())
			m_img[y*m_width + x] = pix;
	}


	void SetImage(const t_pixel* img)
	{
		for(std::size_t y = 0; y < GetHeight(); ++y)
			for(std::size_t x = 0; x < GetWidth(); ++x)
				SetPixel(x, y, img[y*GetWidth() + x]);
	}


private:
	std::size_t m_width{}, m_height{};
	std::unique_ptr<t_pixel[]> m_img{};
};
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// interface wrapper functions
// ----------------------------------------------------------------------------
/**
 * get a pixel from an image
 */
template<class t_image> requires is_image<t_image>
typename t_image::value_type
get_pixel(const t_image& img, int x, int y)
{
	using t_pixel = typename t_image::value_type;

	if(x >= (int)img.GetWidth() || y >= (int)img.GetHeight() || x<0 || y<0)
		return t_pixel{};

	return img.GetPixel(x, y);
}


/**
 * set a pixel in an image
 */
template<class t_image> requires is_image<t_image>
void set_pixel(t_image& img, int x, int y,
	typename t_image::value_type pixel)
{
	if(x >= (int)img.GetWidth() || y >= (int)img.GetHeight() || x<0 || y<0)
		return;

	img.SetPixel(x, y, pixel);
}


/**
 * get the width and height from an image
 */
template<class t_image> requires is_image<t_image>
std::pair<std::size_t, std::size_t> get_image_dims(const t_image& img)
{
	return std::make_pair(img.GetWidth(), img.GetHeight());
}


#ifdef USE_GIL
/**
 * get a pixel from an image view
 */
template<class t_imageview> requires is_imageview<t_imageview>
typename boost::gil::channel_type<t_imageview>::type
get_pixel(const t_imageview& img, int x, int y)
{
	using t_pixel = typename boost::gil::channel_type<t_imageview>::type;

	if(x >= img.width() || y >= img.height() || x<0 || y<0)
		return t_pixel{};

	return img.row_begin(y)[x];
}


/**
 * set a pixel in an image view
 */
template<class t_imageview> requires is_imageview<t_imageview>
void set_pixel(t_imageview& img, int x, int y,
	typename boost::gil::channel_type<t_imageview>::type pixel)
{
	if(x >= img.width() || y >= img.height() || x<0 || y<0)
		return;

	img.row_begin(y)[x] = pixel;
}
#endif


/**
 * get the width and height from an image view
 */
template<class t_imageview> requires is_imageview<t_imageview>
std::pair<std::size_t, std::size_t> get_image_dims(const t_imageview& img)
{
	return std::make_pair(img.width(), img.height());
}
// ----------------------------------------------------------------------------



// ----------------------------------------------------------------------------
// algorithms
// ----------------------------------------------------------------------------
/**
 * contour tracing
 * @see http://www.imageprocessingplace.com/downloads_V3/root_downloads/tutorials/contour_tracing_Abeer_George_Ghuneim/ray.html
 */
template<class t_vec,
	class t_imageview, class t_boundaryview = t_imageview>
requires tl2::is_vec<t_vec>
std::vector<std::vector<t_vec>> trace_contour(
	const t_imageview& img, t_boundaryview* boundary = nullptr)
{
	// contour polygons
	std::vector<std::vector<t_vec>> contours;


	// contour already seen?
	auto already_seen = [&contours](const t_vec& vec) -> bool
	{
		for(const auto& contour : contours)
		{
			if(std::find(contour.begin(), contour.end(), vec) != contour.end())
				return true;
		}

		return false;
	};


	// next possible position depending on direction
	auto get_next_dir = [](const t_vec& dir, t_vec& next_dir, int iter=0) -> bool
	{
		static const t_vec next_dirs[] =
		{
			tl2::create<t_vec>({ -1, -1 }), // 0
			tl2::create<t_vec>({  0, -1 }), // 1
			tl2::create<t_vec>({  1, -1 }), // 2
			tl2::create<t_vec>({  1,  0 }), // 3
			tl2::create<t_vec>({  1,  1 }), // 4
			tl2::create<t_vec>({  0,  1 }), // 5
			tl2::create<t_vec>({ -1,  1 }), // 6
			tl2::create<t_vec>({ -1,  0 }), // 7
		};

		const t_vec back_dir = -dir;
		std::size_t idx = 0;
		bool has_next_dir = false;

		for(int i=0; i<8; ++i)
		{
			if(back_dir == next_dirs[i])
			{
				idx = (iter + i+1) % 8;
				has_next_dir = true;
				break;
			}
		}

		if(!has_next_dir)
			return false;

		next_dir = next_dirs[idx];
		return true;
	};


	// find multiple contours
	t_vec start = tl2::create<t_vec>({0, 0});

	while(true)
	{
		std::vector<t_vec> contour;

		// find start pixel
		bool start_found = false;
		auto [width, height] = get_image_dims(img);

		for(int y=start[1]; y<(int)height; ++y)
		{
			for(int x=0; x<(int)width; ++x)
			{
				if(get_pixel(img, x, y))
				{
					// for multiple contours: inside a contour
					if(get_pixel(img, x-1, y))
						continue;

					t_vec vec = tl2::create<t_vec>({x, y});
					if(already_seen(vec))
						continue;

					start = vec;
					start_found = true;
					break;
				}
			}

			if(start_found)
				break;
		}

		if(!start_found)
			return contours;

		//contour.push_back(start);
		if(boundary)
			set_pixel<t_boundaryview>(*boundary, start[0], start[1], 0xff);


		// trace boundary
		t_vec pos = start;
		t_vec dir = tl2::create<t_vec>({1, 0});
		t_vec next_dir = tl2::create<t_vec>({0, 0});

		while(true)
		{
			bool has_next_dir = false;

			for(int i=0; i<8; ++i)
			{
				if(get_next_dir(dir, next_dir, i)
					&& get_pixel(img, pos[0]+next_dir[0], pos[1]+next_dir[1]))
				{
					has_next_dir = true;
					break;
				}
			}

			if(has_next_dir)
			{
				dir[0] = next_dir[0];
				dir[1] = next_dir[1];

				// don't insert the same point multiple times
				if(!tl2::equals_0<t_vec>(dir))
				{
					pos[0] += dir[0];
					pos[1] += dir[1];

					contour.push_back(pos);
					if(boundary)
						set_pixel<t_boundaryview>(*boundary, pos[0], pos[1], 0xff);
				}
			}
			else
			{
				break;
			}

			// back at start
			if(pos[0] == start[0] && pos[1] == start[1])
				break;
		}

		if(contour.size())
			contours.emplace_back(std::move(contour));
		else
			break;
	}

	return contours;
}


#ifdef USE_OCV
/**
 * contour tracing using opencv
 * @see https://docs.opencv.org/4.x/index.html
 * @see https://github.com/opencv/opencv
 */
template<class t_vec,
	class t_imageview, class t_boundaryview = t_imageview>
requires tl2::is_vec<t_vec>
std::vector<std::vector<t_vec>> trace_contour_ocv(const t_imageview& img)
{
	// convert from internal image format
	auto [width, height] = get_image_dims(img);
	cv::Mat mat(height, width, CV_8U);

	for(std::size_t y = 0; y < height; ++y)
		for(std::size_t x = 0; x < width; ++x)
			mat.at<std::uint8_t>(y, x) = get_pixel(img, x, y);

	// find contours
	std::vector<std::vector<cv::Point>> contours;
	std::vector<cv::Vec4i> hierarchy;
	cv::findContours(mat, contours, hierarchy,
		cv::RETR_LIST, cv::CHAIN_APPROX_SIMPLE);

	// debug output
	/*cv::Mat cont_mat(height, width, CV_8UC3, cv::Scalar{0, 0, 0});
	cv::drawContours(cont_mat, contours, -1, cv::Scalar{0xff, 0xff, 0xff});
	cv::imwrite("/users/tw/tmp/taspaths_cont_0.png", mat);
	cv::imwrite("/users/tw/tmp/taspaths_cont_1.png", cont_mat);*/

	// convert to internal vector format
	std::vector<std::vector<t_vec>> contour_polys;
	for(const std::vector<cv::Point>& contour : contours)
	{
		std::vector<t_vec> contour_poly;
		contour_poly.reserve(contour.size());

		for(const cv::Point& pt : contour)
			contour_poly.emplace_back(tl2::create<t_vec>({pt.x, pt.y}));

		contour_polys.emplace_back(std::move(contour_poly));
	}

	return contour_polys;
}
#endif


/**
 * results structure of the build_closest_pixel_tree function
 */
template<class t_vec>
requires tl2::is_vec<t_vec>
struct ClosestPixelTreeResults
{
public:
	using t_scalar = typename t_vec::value_type;

#if GEO_OBSTACLES_INDEX_TREE == 1
	// vertex type used in index tree
	template<class T = t_scalar>
	using t_idxvertex = boost::geometry::model::point<
		T, 2, boost::geometry::cs::cartesian>;

	// spatial index tree type
	using t_idxtree = boost::geometry::index::rtree<
		t_idxvertex<t_scalar>,
		boost::geometry::index::dynamic_rstar>;
#elif GEO_OBSTACLES_INDEX_TREE == 2
	using t_idxtree = KdTree<t_vec>;
#endif


private:
	// spatial index tree for the pixels
#if GEO_OBSTACLES_INDEX_TREE == 1
	t_idxtree idxtree{typename t_idxtree::parameters_type(8)};
#elif GEO_OBSTACLES_INDEX_TREE == 2
	t_idxtree idxtree{2};
#endif


public:
	/**
	 * get the index tree
	 */
	t_idxtree& GetIndexTree() { return idxtree; }
	const t_idxtree& GetIndexTree() const { return idxtree; }


	/**
	 * query the postions closest to pos
	 */
	std::vector<t_vec> Query(const t_vec& pos, std::size_t num) const
	{
		std::vector<t_vec> nearest_vertices;
		nearest_vertices.reserve(num);

#if GEO_OBSTACLES_INDEX_TREE == 1
		idxtree.query(boost::geometry::index::nearest(
			t_idxvertex<t_scalar>(pos[0], pos[1]), num),
			boost::make_function_output_iterator([&nearest_vertices](const auto& point)
			{
				t_vec vec = tl2::create<t_vec>({
					point.template get<0>(), point.template get<1>()});
				nearest_vertices.emplace_back(std::move(vec));
			}));
#elif GEO_OBSTACLES_INDEX_TREE == 2
		if(const auto* node = idxtree.get_closest(pos); node)
			nearest_vertices.emplace_back(*node->vec);

#endif

		return nearest_vertices;
	}


	/**
	 * clear the index tree
	 */
	void Clear()
	{
		idxtree.clear();
	}
};


/**
 * build an index tree to find the pixel of a certain value
 * which is closest to a given coordinate
 */
template<class t_vec, class t_imageview>
requires tl2::is_vec<t_vec>
ClosestPixelTreeResults<t_vec>
build_closest_pixel_tree(const t_imageview& img)
{
#if GEO_OBSTACLES_INDEX_TREE == 1
	using t_results = ClosestPixelTreeResults<t_vec>;
	using t_scalar = typename t_results::t_scalar;
	using t_idxvertex = typename t_results::template t_idxvertex<t_scalar>;

	t_results results;
	auto& tree = results.GetIndexTree();

	auto [width, height] = get_image_dims(img);

	// iterate pixels
	for(int y=0; y<(int)height; ++y)
	{
		for(int x=0; x<(int)width; ++x)
		{
			auto pix_val = get_pixel(img, x-1, y);
			if(pix_val)
			{
				// convert pixel coordinate to index vertex and insert it into the tree
				tree.insert(t_idxvertex{x, y});
			}
		}
	}

	return results;

#elif GEO_OBSTACLES_INDEX_TREE == 2
	using t_results = ClosestPixelTreeResults<t_vec>;

	t_results results;
	auto& tree = results.GetIndexTree();
	auto [width, height] = get_image_dims(img);

	std::vector<t_vec> verts_to_insert;

	// iterate pixels
	for(int y=0; y<(int)height; ++y)
	{
		for(int x=0; x<(int)width; ++x)
		{
			auto pix_val = get_pixel(img, x-1, y);
			if(pix_val)
				verts_to_insert.emplace_back(tl2::create<t_vec>({x, y}));
		}
	}

	tree.create(verts_to_insert);
	return results;
#endif
}
// ----------------------------------------------------------------------------

} // geo


#endif
