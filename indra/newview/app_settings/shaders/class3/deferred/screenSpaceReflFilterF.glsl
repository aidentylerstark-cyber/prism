/**
 * @file screenSpaceReflFilterF.glsl
 *
 * $LicenseInfo:firstyear=2024&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2024, Linden Research, Inc.
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

out vec4 frag_color;

uniform sampler2D diffuseMap;
uniform vec2 screen_res;
uniform vec2 delta;
uniform vec3 kern[4];
uniform float kern_scale;

in vec2 vary_fragcoord;

GBufferInfo getGBuffer(vec2 screenpos);
vec4 getPosition(vec2 pos_screen);

void main()
{
    vec2 tc = vary_fragcoord.xy;
    vec3 pos = getPosition(tc).xyz;
    GBufferInfo gb = getGBuffer(tc);

    vec4 ccol = texture(diffuseMap, tc);

    // Skip pixels with no SSR data (alpha == 0)
    if (ccol.a <= 0.0)
    {
        frag_color = ccol;
        return;
    }

    // Per-pixel roughness drives blur width: smooth = no blur, rough = full blur
    float roughness;
    if (GET_GBUFFER_FLAG(gb.gbufferFlag, GBUFFER_FLAG_HAS_PBR))
        roughness = gb.specular.g;       // ORM: g = perceptualRoughness
    else
        roughness = 1.0 - gb.specular.a; // Legacy: a = glossiness

    // Edge-aware contact heuristic: sample depth and normals at 4 axis-aligned
    // neighbors. High depth variance or normal divergence = geometric edge =
    // reduce blur to preserve contact reflections and sharp creases.
    vec2 texel = 1.0 / screen_res;
    float z0 = pos.z;
    float zL = getPosition(tc - vec2(texel.x, 0.0)).z;
    float zR = getPosition(tc + vec2(texel.x, 0.0)).z;
    float zU = getPosition(tc - vec2(0.0, texel.y)).z;
    float zD = getPosition(tc + vec2(0.0, texel.y)).z;

    float maxDeltaZ = max(max(abs(zL - z0), abs(zR - z0)),
                          max(abs(zU - z0), abs(zD - z0)));
    float depthEdge = maxDeltaZ / max(-z0 * 0.01, 0.1);

    vec3 nL = getGBuffer(tc - vec2(texel.x, 0.0)).normal;
    vec3 nR = getGBuffer(tc + vec2(texel.x, 0.0)).normal;
    vec3 nU = getGBuffer(tc - vec2(0.0, texel.y)).normal;
    vec3 nD = getGBuffer(tc + vec2(0.0, texel.y)).normal;

    float minNdot = min(min(dot(gb.normal, nL), dot(gb.normal, nR)),
                        min(dot(gb.normal, nU), dot(gb.normal, nD)));
    float normalEdge = 1.0 - clamp(minNdot, 0.0, 1.0);

    float edge = max(depthEdge, normalEdge);
    float contactFactor = 1.0 - smoothstep(0.0, 1.0, edge);

    vec2 dlt = kern_scale * delta * roughness * contactFactor;
    // Scale kernel by distance to reduce over-blur at far range
    dlt /= max(-pos.z * 0.01, 1.0);

    float defined_weight = kern[0].x;
    vec4 col = ccol * kern[0].x;

    // Plane-distance threshold, relaxed with distance
    float tolerance = pos.z * pos.z * 0.00005;

    // Perturb sample origin to hide edge ghosting (same pattern as blurLightF)
    vec2 stc = tc * screen_res;
    float tc_mod = 0.5 * (stc.x + stc.y);
    tc_mod -= floor(tc_mod);
    tc_mod *= 2.0;
    stc += (tc_mod - 0.5) * kern[1].z * dlt * 0.5;

    // Build 7-tap kernel from 4 control points
    vec3 k[7];
    k[0] = kern[0];
    k[2] = kern[1];
    k[4] = kern[2];
    k[6] = kern[3];
    k[1] = (k[0] + k[2]) * 0.5;
    k[3] = (k[2] + k[4]) * 0.5;
    k[5] = (k[4] + k[6]) * 0.5;

    for (int i = 1; i < 7; i++)
    {
        vec2 samptc = (stc + k[i].z * dlt * 2.0) / screen_res;
        vec3 samppos = getPosition(samptc).xyz;
        float d = dot(gb.normal, samppos - pos);
        if (d * d <= tolerance)
        {
            vec4 s = texture(diffuseMap, samptc);
            col += s * k[i].x;
            defined_weight += k[i].x;
        }
    }

    for (int i = 1; i < 7; i++)
    {
        vec2 samptc = (stc - k[i].z * dlt * 2.0) / screen_res;
        vec3 samppos = getPosition(samptc).xyz;
        float d = dot(gb.normal, samppos - pos);
        if (d * d <= tolerance)
        {
            vec4 s = texture(diffuseMap, samptc);
            col += s * k[i].x;
            defined_weight += k[i].x;
        }
    }

    col /= defined_weight;
    frag_color = max(col, vec4(0));
}
