/**
 * paths rendering widget, vertex shader
 * @author Tobias Weber <tweber@ill.fr>
 * @date feb-2021
 * @license GPLv3, see 'LICENSE' file
 * @note Initially forked from my tlibs2 library (https://code.ill.fr/scientific-software/takin/tlibs2/-/blob/master/libs/qt/glplot.cpp).
 * @note Further code forked from my privately developed "misc" project (https://github.com/t-weber/misc).
 *
 * References:
 *   - http://doc.qt.io/qt-5/qopenglwidget.html#details
 *   - http://code.qt.io/cgit/qt/qtbase.git/tree/examples/opengl/threadedqopenglwidget
 *   - (Sellers 2014) G. Sellers et al., "OpenGL SuperBible", ISBN: 978-0-321-90294-8 (2014).
 *
 * ----------------------------------------------------------------------------
 * TAS-Paths (part of the Takin software suite)
 * Copyright (C) 2021  Tobias WEBER (Institut Laue-Langevin (ILL), 
 *                     Grenoble, France).
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

#version ${GLSL_VERSION}

#define t_real float

const t_real pi = ${PI};



// ----------------------------------------------------------------------------
// inputs to vertex shader
// ----------------------------------------------------------------------------
in vec4 vertex;
in vec4 normal;
in vec4 vertex_col;
in vec2 tex_coords;
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// outputs from vertex shader
// ----------------------------------------------------------------------------
out VertexOut
{
	vec4 col;
	vec2 coords;

	vec4 pos;
	vec4 norm;

	vec4 pos_shadow;
} vertex_out;
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// transformations
// ----------------------------------------------------------------------------
uniform mat4 trafos_proj = mat4(1.);
uniform mat4 trafos_cam = mat4(1.);
uniform mat4 trafos_cam_inv = mat4(1.);

uniform mat4 trafos_light_proj = mat4(1.);
uniform mat4 trafos_light = mat4(1.);
uniform mat4 trafos_light_inv = mat4(1.);

uniform mat4 trafos_obj = mat4(1.);
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// lighting
// ----------------------------------------------------------------------------
uniform bool shadow_enabled = false;
uniform bool shadow_renderpass = false;
// ----------------------------------------------------------------------------


/**
 * for shadow rendering, see (Sellers 2014), pp. 534-540.
 * for perspective transformation and divide, see
 *	https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluProject.xml
 */
void main()
{
	vec4 objPos = trafos_obj * vertex;
	vec4 objNorm = normalize(trafos_obj * normal);
	vec4 shadowPos = trafos_light_proj * trafos_light * objPos;

	if(shadow_renderpass)
		gl_Position = shadowPos;
	else
		gl_Position = trafos_proj * trafos_cam * objPos;

	vertex_out.pos = objPos;
	vertex_out.norm = objNorm;
	vertex_out.col = vertex_col;
	//vertex_out.col.a = 1;

	vertex_out.coords = tex_coords;

	shadowPos.xyz *= 0.5;
	shadowPos.xyz += 0.5 * shadowPos.w;
	vertex_out.pos_shadow = shadowPos;
}
