/**
 * @file class1\deferred\pbralphaF.glsl
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
uniform sampler2D specularMap; // PBR: Packed: Occlusion, Metal, Roughness
#endif

#ifdef HAS_DIFFUSE_LOOKUP
uniform vec4 pbr_factors[4];
uniform vec4 emissive_colors[4];
#define PBR_METALLIC  pbr_factors[vary_texture_index].x
#define PBR_ROUGHNESS pbr_factors[vary_texture_index].y
#define PBR_MIN_ALPHA pbr_factors[vary_texture_index].z
#define PBR_EMISSIVE  emissive_colors[vary_texture_index].rgb
#else
uniform float metallicFactor;
uniform float roughnessFactor;
uniform vec3 emissiveColor;
#define PBR_METALLIC  metallicFactor
#define PBR_ROUGHNESS roughnessFactor
#define PBR_EMISSIVE  emissiveColor
#ifdef HAS_ALPHA_MASK
uniform float minimum_alpha;
#define PBR_MIN_ALPHA minimum_alpha
#endif
#endif

#if defined(HAS_SUN_SHADOW) || defined(HAS_SSAO)
uniform sampler2D lightMap;
#endif

uniform int sun_up_factor;
uniform vec3 sun_dir;
uniform vec3 moon_dir;
uniform int classic_mode;

out vec4 frag_color;

in vec3 vary_fragcoord;

#ifdef HAS_SUN_SHADOW
  uniform vec2 screen_res;
#endif

in vec3 vary_position;

in vec2 base_color_texcoord;
in vec2 normal_texcoord;
in vec2 metallic_roughness_texcoord;
in vec2 emissive_texcoord;

in vec4 vertex_color;

in vec3 vary_normal;
in vec3 vary_tangent;
flat in float vary_sign;


// Lights
// See: LLRender::syncLightState()
uniform vec4 light_position[8];
uniform vec3 light_direction[8]; // spot direction
uniform vec4 light_attenuation[8]; // linear, quadratic, is omni, unused, See: LLPipeline::setupHWLights() and syncLightState()
uniform vec3 light_diffuse[8];
uniform vec2 light_deferred_attenuation[8]; // light size and falloff

vec3 srgb_to_linear(vec3 c);
vec3 linear_to_srgb(vec3 c);

void calcAtmosphericVarsLinear(vec3 inPositionEye, vec3 norm, vec3 light_dir, out vec3 sunlit, out vec3 amblit, out vec3 atten, out vec3 additive);
vec4 applySkyAndWaterFog(vec3 pos, vec3 additive, vec3 atten, vec4 color);

void calcHalfVectors(vec3 lv, vec3 n, vec3 v, out vec3 h, out vec3 l, out float nh, out float nl, out float nv, out float vh, out float lightDist);
float calcLegacyDistanceAttenuation(float distance, float falloff);
float sampleDirectionalShadow(vec3 pos, vec3 norm, vec2 pos_screen);
void sampleReflectionProbes(inout vec3 ambenv, inout vec3 glossenv,
        vec2 tc, vec3 pos, vec3 norm, float glossiness, bool transparent, vec3 amblit_linear);

void mirrorClip(vec3 pos);
void waterClip(vec3 pos);

void calcDiffuseSpecular(vec3 baseColor, float metallic, inout vec3 diffuseColor, inout vec3 specularColor);

vec3 pbrBaseLight(vec3 diffuseColor,
                  vec3 specularColor,
                  float metallic,
                  vec3 pos,
                  vec3 norm,
                  float perceptualRoughness,
                  vec3 light_dir,
                  vec3 sunlit,
                  float scol,
                  vec3 radiance,
                  vec3 irradiance,
                  vec3 colorEmissive,
                  float ao,
                  vec3 additive,
                  vec3 atten);

vec3 pbrCalcPointLightOrSpotLight(vec3 diffuseColor, vec3 specularColor,
                    float perceptualRoughness,
                    float metallic,
                    vec3 n, // normal
                    vec3 p, // pixel position
                    vec3 v, // view vector (negative normalized pixel position)
                    vec3 lp, // light position
                    vec3 ld, // light direction (for spotlights)
                    vec3 lightColor,
                    float lightSize, float falloff, float is_pointlight, float ambiance);

void main()
{
    mirrorClip(vary_position);

    vec3 color = vec3(0,0,0);

    vec3  light_dir   = (sun_up_factor == 1) ? sun_dir : moon_dir;
    vec3  pos         = vary_position;

    waterClip(pos);

#ifdef HAS_DIFFUSE_LOOKUP
    vec4 basecolor = diffuseLookup(base_color_texcoord.xy);
#else
    vec4 basecolor = texture(diffuseMap, base_color_texcoord.xy);
#endif
    basecolor.rgb = srgb_to_linear(basecolor.rgb);
#ifdef HAS_ALPHA_MASK
    if (basecolor.a < PBR_MIN_ALPHA)
    {
        discard;
    }
#endif

    vec3 col = vertex_color.rgb * basecolor.rgb;

#ifdef HAS_NORMAL_LOOKUP
    vec3 vNt = normalLookup(normal_texcoord.xy).xyz*2.0-1.0;
#else
    vec3 vNt = texture(bumpMap, normal_texcoord.xy).xyz*2.0-1.0;
#endif
    float sign = vary_sign;
    vec3 vN = vary_normal;
    vec3 vT = vary_tangent.xyz;

    vec3 vB = sign * cross(vN, vT);
    vec3 norm = normalize( vNt.x * vT + vNt.y * vB + vNt.z * vN );

    norm *= gl_FrontFacing ? 1.0 : -1.0;

    float scol = 1.0;
    vec3 sunlit;
    vec3 amblit;
    vec3 additive;
    vec3 atten;
    calcAtmosphericVarsLinear(pos.xyz, norm, light_dir, sunlit, amblit, additive, atten);
    if (classic_mode > 0)
        sunlit *= 1.35;
    vec3 sunlit_linear = sunlit;

    vec2 frag = vary_fragcoord.xy/vary_fragcoord.z*0.5+0.5;

#ifdef HAS_SUN_SHADOW
    scol = sampleDirectionalShadow(pos.xyz, norm.xyz, frag);
#endif

#ifdef HAS_SPECULAR_LOOKUP
    vec3 orm = specularLookup(metallic_roughness_texcoord.xy).rgb;
#else
    vec3 orm = texture(specularMap, metallic_roughness_texcoord.xy).rgb; //orm is packed into "emissiveRect" to keep the data in linear color space
#endif

    float perceptualRoughness = orm.g * PBR_ROUGHNESS;
    float metallic = orm.b * PBR_METALLIC;
    float ao = orm.r;

    // emissiveColor is the emissive color factor from GLTF and is already in linear space
    vec3 colorEmissive = PBR_EMISSIVE;
#ifdef HAS_EMISSIVE_LOOKUP
    colorEmissive *= srgb_to_linear(emissiveLookup(emissive_texcoord.xy).rgb);
#else
    // emissiveMap here is a vanilla RGB texture encoded as sRGB, manually convert to linear
    colorEmissive *= srgb_to_linear(texture(emissiveMap, emissive_texcoord.xy).rgb);
#endif

    // PBR IBL
    float gloss      = 1.0 - perceptualRoughness;
    vec3  irradiance = amblit;
    vec3  radiance  = vec3(0);
    sampleReflectionProbes(irradiance, radiance, vary_position.xy*0.5+0.5, pos.xyz, norm.xyz, gloss, true, amblit);

    vec3 diffuseColor;
    vec3 specularColor;
    calcDiffuseSpecular(col.rgb, metallic, diffuseColor, specularColor);

    vec3 v = -normalize(pos.xyz);

    color = pbrBaseLight(diffuseColor, specularColor, metallic, v, norm.xyz, perceptualRoughness, light_dir, sunlit_linear, scol, radiance, irradiance, colorEmissive, ao, additive, atten);

    vec3 light = vec3(0);

    // Punctual lights
#define LIGHT_LOOP(i) light += pbrCalcPointLightOrSpotLight(diffuseColor, specularColor, perceptualRoughness, metallic, norm.xyz, pos.xyz, v, light_position[i].xyz, light_direction[i].xyz, light_diffuse[i].rgb, light_deferred_attenuation[i].x, light_deferred_attenuation[i].y, light_attenuation[i].z, light_attenuation[i].w);

    LIGHT_LOOP(1)
    LIGHT_LOOP(2)
    LIGHT_LOOP(3)
    LIGHT_LOOP(4)
    LIGHT_LOOP(5)
    LIGHT_LOOP(6)
    LIGHT_LOOP(7)

    color.rgb += light.rgb;

    color.rgb = applySkyAndWaterFog(pos.xyz, additive, atten, vec4(color, 1.0)).rgb;

    float a = basecolor.a*vertex_color.a;
    float final_scale = 1;
    if (classic_mode > 0)
        final_scale = 1.1;
    frag_color = max(vec4(color.rgb * final_scale,a), vec4(0));
}

#else

uniform sampler2D diffuseMap;  //always in sRGB space
uniform sampler2D emissiveMap;

uniform vec3 emissiveColor;

out vec4 frag_color;

in vec3 vary_position;

in vec2 base_color_texcoord;
in vec2 emissive_texcoord;

in vec4 vertex_color;

#ifdef HAS_ALPHA_MASK
uniform float minimum_alpha; // PBR alphaMode: MASK, See: mAlphaCutoff, setAlphaCutoff()
#endif

vec3 srgb_to_linear(vec3 c);
vec3 linear_to_srgb(vec3 c);


void main()
{
    vec3 color = vec3(0,0,0);

    vec3  pos         = vary_position;

    vec4 basecolor = texture(diffuseMap, base_color_texcoord.xy).rgba;
    basecolor.rgb = srgb_to_linear(basecolor.rgb);
#ifdef HAS_ALPHA_MASK
    if (basecolor.a < minimum_alpha)
    {
        discard;
    }
#endif

    color = vertex_color.rgb * basecolor.rgb;

    // emissiveColor is the emissive color factor from GLTF and is already in linear space
    vec3 colorEmissive = emissiveColor;
    // emissiveMap here is a vanilla RGB texture encoded as sRGB, manually convert to linear
    colorEmissive *= srgb_to_linear(texture(emissiveMap, emissive_texcoord.xy).rgb);


    float a = basecolor.a*vertex_color.a;
    color += colorEmissive;

    color = linear_to_srgb(color);
    frag_color = max(vec4(color.rgb,a), vec4(0));
}

#endif
