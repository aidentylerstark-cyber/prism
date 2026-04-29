/**
 * @file pbropaqueF.glsl
 *
 * $LicenseInfo:firstyear=2022&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2022, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

/*[EXTRA_CODE_HERE]*/


#ifndef IS_HUD

// deferred opaque implementation

#ifndef HAS_DIFFUSE_LOOKUP
uniform sampler2D diffuseMap;  //always in sRGB space
#endif

#ifndef HAS_NORMAL_LOOKUP
uniform sampler2D bumpMap;
#endif
#ifndef HAS_EMISSIVE_LOOKUP
uniform sampler2D emissiveMap;
#endif
#ifndef HAS_SPECULAR_LOOKUP
uniform sampler2D specularMap; // Packed: Occlusion, Metal, Roughness
#endif

// Per-slot PBR material params.
//  pbr_factors:      x metallic, y roughness, z min_alpha, w _
//  emissive_colors: .rgb emissive, .a _
#ifdef HAS_DIFFUSE_LOOKUP
uniform vec4 pbr_factors[4];
uniform vec4 emissive_colors[4];
#define PBR_METALLIC       pbr_factors[vary_texture_index].x
#define PBR_ROUGHNESS      pbr_factors[vary_texture_index].y
#define PBR_MIN_ALPHA      pbr_factors[vary_texture_index].z
#define PBR_EMISSIVE       emissive_colors[vary_texture_index].rgb
#else
uniform float metallicFactor;
uniform float roughnessFactor;
uniform vec3  emissiveColor;
uniform float minimum_alpha;
#define PBR_METALLIC       metallicFactor
#define PBR_ROUGHNESS      roughnessFactor
#define PBR_MIN_ALPHA      minimum_alpha
#define PBR_EMISSIVE       emissiveColor
#endif

out vec4 frag_data[4];

in vec3 vary_position;
in vec4 vertex_color;
in vec3 vary_normal;
in vec3 vary_tangent;
flat in float vary_sign;

in vec2 base_color_texcoord;
in vec2 normal_texcoord;
in vec2 metallic_roughness_texcoord;
in vec2 emissive_texcoord;

vec3 linear_to_srgb(vec3 c);
vec3 srgb_to_linear(vec3 c);

uniform vec4 clipPlane;
uniform float clipSign;

void mirrorClip(vec3 pos);
vec4 encodeNormal(vec3 n, float env, float gbuffer_flag);

uniform mat3 normal_matrix;

void main()
{
    mirrorClip(vary_position);

#ifdef HAS_DIFFUSE_LOOKUP
    vec4 basecolor = diffuseLookup(base_color_texcoord.xy);
#else
    vec4 basecolor = texture(diffuseMap, base_color_texcoord.xy);
#endif
    basecolor.rgb = srgb_to_linear(basecolor.rgb);

    basecolor *= vertex_color;

    if (basecolor.a < PBR_MIN_ALPHA)
    {
        discard;
    }

    vec3 col = basecolor.rgb;

    // from mikktspace.com
#ifdef HAS_NORMAL_LOOKUP
    vec3 vNt = normalLookup(normal_texcoord.xy).xyz*2.0-1.0;
#else
    vec3 vNt = texture(bumpMap, normal_texcoord.xy).xyz*2.0-1.0;
#endif
    float sign = vary_sign;
    vec3 vN = vary_normal;
    vec3 vT = vary_tangent.xyz;

    vec3 vB = sign * cross(vN, vT);
    vec3 tnorm = normalize( vNt.x * vT + vNt.y * vB + vNt.z * vN );

    // RGB = Occlusion, Roughness, Metal
#ifdef HAS_SPECULAR_LOOKUP
    vec3 spec = specularLookup(metallic_roughness_texcoord.xy).rgb;
#else
    vec3 spec = texture(specularMap, metallic_roughness_texcoord.xy).rgb;
#endif

    spec.g *= PBR_ROUGHNESS;
    spec.b *= PBR_METALLIC;

    vec3 emissive = PBR_EMISSIVE;
#ifdef HAS_EMISSIVE_LOOKUP
    emissive *= srgb_to_linear(emissiveLookup(emissive_texcoord.xy).rgb);
#else
    emissive *= srgb_to_linear(texture(emissiveMap, emissive_texcoord.xy).rgb);
#endif

    tnorm *= gl_FrontFacing ? 1.0 : -1.0;

    //spec.rgb = vec3(1,1,0);
    //col = vec3(0,0,0);
    //emissive = vary_tangent.xyz*0.5+0.5;
    //emissive = vec3(sign*0.5+0.5);
    //emissive = vNt * 0.5 + 0.5;
    //emissive = tnorm*0.5+0.5;
    // See: C++: addDeferredAttachments(), GLSL: softenLightF
    frag_data[0] = max(vec4(col, 0.0), vec4(0));                                                   // Diffuse
    frag_data[1] = max(vec4(spec.rgb,0.0), vec4(0));                                    // PBR linear packed Occlusion, Roughness, Metal.
    frag_data[2] = encodeNormal(tnorm, 0, GBUFFER_FLAG_HAS_PBR); // normal, environment intensity, flags

#if defined(HAS_EMISSIVE)
    frag_data[3] = max(vec4(emissive,0), vec4(0));                                                // PBR sRGB Emissive
#endif
}

#else

// forward fullbright implementation for HUDs

uniform sampler2D diffuseMap;  //always in sRGB space

uniform vec3 emissiveColor;
uniform sampler2D emissiveMap;

out vec4 frag_color;

in vec3 vary_position;
in vec4 vertex_color;

in vec2 base_color_texcoord;
in vec2 emissive_texcoord;

uniform float minimum_alpha; // PBR alphaMode: MASK, See: mAlphaCutoff, setAlphaCutoff()

vec3 linear_to_srgb(vec3 c);
vec3 srgb_to_linear(vec3 c);

void main()
{
    vec4 basecolor = texture(diffuseMap, base_color_texcoord.xy).rgba;

    basecolor.a *= vertex_color.a;

    if (basecolor.a < minimum_alpha)
    {
        discard;
    }

    vec3 col = vertex_color.rgb * srgb_to_linear(basecolor.rgb);

    vec3 emissive = emissiveColor;
    emissive *= srgb_to_linear(texture(emissiveMap, emissive_texcoord.xy).rgb);

    col += emissive;

    // HUDs are rendered after gamma correction, output in sRGB space
    frag_color.rgb = linear_to_srgb(col);
    frag_color.a = 0.0;
}

#endif

