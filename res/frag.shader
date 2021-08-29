/**
 * paths rendering widget, fragment shader
 * @author Tobias Weber <tweber@ill.fr>
 * @date feb-2021
 * @license GPLv3, see 'LICENSE' file
 *
 * References:
 *   - http://doc.qt.io/qt-5/qopenglwidget.html#details
 *   - http://code.qt.io/cgit/qt/qtbase.git/tree/examples/opengl/threadedqopenglwidget
 *   - (Sellers 2014) G. Sellers et al., ISBN: 978-0-321-90294-8 (2014).
 */

#version ${GLSL_VERSION}

#define t_real float
#define MAX_LIGHTS ${MAX_LIGHTS}

const t_real pi = ${PI};


// ----------------------------------------------------------------------------
// inputs to fragment shader (output from vertex shader)
// ----------------------------------------------------------------------------
in VertexOut
{
	vec4 col;
	vec2 coords;

	vec4 pos;
	vec4 norm;

	vec4 pos_shadow;
} frag_in;
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// outputs from fragment shader
// ----------------------------------------------------------------------------
out vec4 frag_out_col;
// ----------------------------------------------------------------------------


// ----------------------------------------------------------------------------
// current cursor position
// ----------------------------------------------------------------------------
uniform bool cursor_active = true;
uniform vec2 cursor_coords = vec2(0.5, 0.5);
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
uniform vec4 lights_const_col = vec4(1, 1, 1, 1);
uniform vec3 lights_pos[MAX_LIGHTS];
uniform int lights_numactive = 1;	// how many lights to use?

uniform sampler2DShadow shadow_map;
uniform bool shadow_enabled = false;
uniform bool shadow_renderpass = false;

const t_real g_diffuse = 1.;
const t_real g_specular = 0.25;
const t_real g_shininess = 1.;
const t_real g_ambient = 0.2;
const t_real g_atten = 0.005;
const t_real g_shadow_atten = 0.75;
// ----------------------------------------------------------------------------


/**
 * reflect a vector on a surface with normal n 
 *	=> subtract the projection vector twice: 1 - 2*|n><n|
 */
mat3 reflect(vec3 n)
{
	mat3 refl = mat3(1.) - 2.*outerProduct(n, n);

	// have both vectors point away from the surface
	return -refl;
}


/**
 * position of the camera
 */
vec3 get_campos()
{
	vec4 trans = -vec4(trafos_cam[3].xyz, 0);
	return (trafos_cam_inv * trans).xyz;
}


/**
 * phong lighting model
 * @see https://en.wikipedia.org/wiki/Phong_reflection_model
 */
t_real lighting(vec4 objVert, vec4 objNorm)
{
	t_real I_total = 0.;

	vec3 dirToCam;
	// only used for specular lighting
	if(g_specular > 0.)
		dirToCam = normalize(get_campos() - objVert.xyz);

	// iterate (active) light sources
	for(int lightidx=0;
		lightidx<min(lights_pos.length(), lights_numactive);
		++lightidx)
	{
		t_real atten = 1.;
		t_real I_diff = 0.;
		t_real I_spec = 0.;

		// diffuse lighting
		vec3 vertToLight = lights_pos[lightidx] - objVert.xyz;
		t_real distVertLight = length(vertToLight);
		vec3 dirLight = vertToLight / distVertLight;

		if(g_diffuse > 0.)
		{
			t_real I_diff_inc = g_diffuse * dot(objNorm.xyz, dirLight);
			if(I_diff_inc < 0.)
				I_diff_inc = 0.;
			I_diff += I_diff_inc;
		}


		// specular lighting
		if(g_specular > 0.)
		{
			if(dot(dirToCam, objNorm.xyz) > 0.)
			{
				vec3 dirLightRefl = reflect(objNorm.xyz) * dirLight;

				t_real val = dot(dirToCam, dirLightRefl);
				if(val > 0.)
				{
					t_real I_spec_inc = g_specular * pow(val, g_shininess);
					if(I_spec_inc < 0.)
						I_spec_inc = 0.;
					I_spec += I_spec_inc;
				}
			}
		}


		// attenuation
		atten /= distVertLight*distVertLight * g_atten;

		if(atten > 1.)
			atten = 1.;
		if(atten < 0.)
			atten = 0.;

		I_total += (I_diff + I_spec) * atten;
	}

	// ambient lighting
	t_real I_amb = g_ambient;

	// total intensity
	return I_total + I_amb;
}


void main()
{
	frag_out_col = vec4(1, 1, 1, 1);

	// shadow rendering pass
	if(shadow_renderpass)
	{
		// use colours for z value
		frag_out_col = vec4(frag_in.pos.z/frag_in.pos.w);
	}

	// normal rendering pass
	else
	{
		const t_real z_eps = 0.05;
		t_real I = lighting(frag_in.pos, frag_in.norm);

		frag_out_col.rgb *= frag_in.col.rgb * I;
		frag_out_col *= lights_const_col;

		if(shadow_enabled)
		{
			// get the shadow z value
			t_real shadow_val = textureProj(shadow_map, frag_in.pos_shadow);

			// is the shadow z value larger than the current object z value?
			if(shadow_val > frag_in.pos.z + z_eps)
				frag_out_col.rgb *= g_shadow_atten;
		}

		if(cursor_active && length(frag_in.coords - cursor_coords) < 0.01)
		{
			frag_out_col = vec4(1, 0, 0, 1);
		}
	}
}
