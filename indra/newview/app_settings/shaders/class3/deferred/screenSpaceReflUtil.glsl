/**
 * @file class3/deferred/screenSpaceReflUtil.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
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

// Based on https://imanolfotia.com/blog/1

uniform sampler2D sceneMap;
uniform sampler2D sceneDepth;

uniform vec2 screen_res;
uniform mat4 projection_matrix;
uniform mat4 inv_proj;
uniform mat4 modelview_delta;
uniform mat4 inv_modelview_delta;

// Declared to keep pipeline uniform setup happy
uniform vec3 iterationCount;
uniform vec3 rayStep;
uniform vec3 distanceBias;
uniform vec3 depthRejectBias;
uniform vec3 adaptiveStepMultiplier;
uniform vec3 splitParamsStart;
uniform vec3 splitParamsEnd;
uniform float glossySampleCount;
uniform float noiseSine;
uniform float maxZDepth;
uniform float maxRoughness;
uniform vec2 ssrJitterOffset;
uniform int hizMipCount;

#define MAX_THICKNESS   depthRejectBias.x
#define DEPTH_BIAS      depthRejectBias.y

vec4 getPositionWithDepth(vec2 pos_screen, float depth);

float random(vec2 uv)
{
    return fract(sin(dot(uv, vec2(12.9898, 78.233))) * 43758.5453123);
}

vec2 generateProjectedPosition(vec3 pos)
{
    vec4 samplePosition = projection_matrix * vec4(pos, 1.0);
    samplePosition.xy = (samplePosition.xy / samplePosition.w) * 0.5 + 0.5;
    // Compensate for SMAA T2x jitter difference between current and previous frame.
    // mSceneMap was rendered with the previous frame's jittered projection;
    // offset the UV so depth/color lookups hit the correct texels.
    samplePosition.xy += ssrJitterOffset;
    return samplePosition.xy;
}

float getLinearDepth(vec2 tc)
{
    // Force LOD 0 — sceneDepth now carries the Hi-Z mip chain,
    // and reflected UVs have chaotic derivatives that could cause
    // auto-LOD selection to sample coarse Hi-Z levels.
    float depth = textureLod(sceneDepth, tc, 0).r;
    vec4 pos = getPositionWithDepth(tc, depth);
    return -pos.z;
}

float projectDepth(vec3 viewPos)
{
    vec4 clip = projection_matrix * vec4(viewPos, 1.0);
    return (clip.z / clip.w) * 0.5 + 0.5;
}

// Hi-Z hierarchical screen-space ray trace (Godot parametric-T approach).
// origin/dir are in UV+depth space: (u, v, rawDepth) where depth is [0,1].
// Returns hit position (uv + raw depth) or vec3(-1) on miss.
//
// Uses parametric T along the ray for all comparisons, avoiding the AMD
// absolute-depth comparison that breaks for camera-facing rays (dir.z < 0).
// Z is normalized so dir.z = ±1, making depth_t = (cellDepth - origin.z) / dir.z
// trivially comparable with the edge_t from cell boundary intersections.
//
// Our Hi-Z stores MIN depth (standard GL: 0=near, 1=far), so min = closest.
// Godot uses reversed-Z with MAX depth, but the logic maps cleanly:
//   Godot: max(a,b,c,d) with reversed-Z → closest surface
//   Ours:  min(a,b,c,d) with standard-Z → closest surface
// The comparison flips accordingly.
//
// References:
//   Godot Engine SSR: screen_space_reflection.glsl (MIT license)
//   Sugulee/GPU Pro 5: Hi-Z Screen-Space Cone-Traced Reflections
vec3 hiZTrace(vec3 origin, vec3 dir, int maxIterations)
{
    int maxLevel = hizMipCount - 1;

    // Guard near-zero Z direction — treat as Z+ epsilon.
    if (abs(dir.z) < 1e-7)
        dir.z = 1e-7;

    // Normalize direction so |dir.z| = 1.
    // This makes depth_t = (cellDepth - origin.z) * zDir directly comparable
    // with edge_t values along the XY axes.
    // Reference: https://hacksoflife.blogspot.com/2020/10/a-tip-for-hiz-ssr-parametric-t-tracing.html
    vec3 rayDir = dir / abs(dir.z);

    // Standard GL: 0=near, 1=far. Camera-facing rays move toward 0 (dir.z < 0).
    // Godot (reversed-Z): facing_camera = rayDir.z >= 0 (toward near=1.0).
    // For standard GL: facing_camera = dir.z < 0 (toward near=0.0).
    bool facingCamera = dir.z < 0.0;
    float zDir = rayDir.z;  // ±1 after normalization

    // Cell step direction: +1 or -1 per axis (matches Godot cell_step).
    vec2 cellStep = vec2(rayDir.x < 0.0 ? -1.0 : 1.0,
                         rayDir.y < 0.0 ? -1.0 : 1.0);

    int curLevel = 0;
    float t = 0.0;

    // Compute t_max: parametric T to the screen edge where we stop tracing.
    vec2 t0 = (vec2(0.0) - origin.xy) / rayDir.xy;
    vec2 t1 = (vec2(1.0) - origin.xy) / rayDir.xy;
    vec2 t2 = max(t0, t1);
    float tMax = min(t2.x, t2.y);

    // Initial advance: push past origin cell to avoid self-intersection.
    // Matches Godot's initial advance exactly.
    {
        vec2 cellIndex = floor(origin.xy * screen_res);
        vec2 newCellIndex = cellIndex + clamp(cellStep, vec2(0.0), vec2(1.0));
        vec2 newCellPos = (newCellIndex / screen_res) + cellStep * 0.000001;
        vec2 posT = (newCellPos - origin.xy) / rayDir.xy;
        t = min(posT.x, posT.y);
    }

    for (int i = 0; i < maxIterations && curLevel >= 0 && t < tMax; i++)
    {
        vec3 pos = origin + rayDir * t;

        // Cell lookup at current mip level.
        vec2 cellCount = vec2(max(1, int(screen_res.x) >> curLevel),
                              max(1, int(screen_res.y) >> curLevel));
        ivec2 cellIndex = ivec2(floor(pos.xy * cellCount));
        cellIndex = clamp(cellIndex, ivec2(0), ivec2(cellCount) - 1);

        // Min depth in this cell (closest surface, standard GL).
        float cellDepth = texelFetch(sceneDepth, cellIndex, curLevel).r;

        // Parametric T to the depth surface.
        // Since rayDir.z = ±1, this is (cellDepth - origin.z) * (±1).
        float depthT = (cellDepth - origin.z) * zDir;

        // Parametric T to the nearest cell boundary in XY.
        vec2 newCellIndex = vec2(cellIndex) + clamp(cellStep, vec2(0.0), vec2(1.0));
        vec2 newCellPos = (newCellIndex / cellCount) + cellStep * 0.000001;
        vec2 posT = (newCellPos - origin.xy) / rayDir.xy;
        float edgeT = min(posT.x, posT.y);

        // Hit detection (matches Godot exactly):
        // Forward rays: hit if depth surface is reached before cell boundary.
        // Camera-facing rays: hit if ray hasn't traveled past the depth surface.
        bool hit = facingCamera ? (t <= depthT) : (depthT <= edgeT);

        int mipOffset = hit ? -1 : 1;

        // Thickness gate at mip 0 (matches Godot depth_tolerance check):
        // Linearize depths and reject if surface is too far behind ray.
        if (curLevel == 0 && hit)
        {
            float z0 = getPositionWithDepth(pos.xy, cellDepth).z;
            float z1 = getPositionWithDepth(pos.xy, pos.z).z;
            if ((z0 - z1) > MAX_THICKNESS)
            {
                hit = false;
                mipOffset = 0;  // Stay at mip 0, march cell-by-cell.
            }
        }

        // Advance parametric T (matches Godot exactly):
        // Only advance to depthT for non-facing-camera hits.
        // For facing-camera hits, descend without advancing.
        if (hit)
        {
            if (!facingCamera)
                t = max(t, depthT);
        }
        else
        {
            t = edgeT;
        }

        curLevel = min(curLevel + mipOffset, maxLevel);
    }

    vec3 hitPos = origin + rayDir * t;

    // Final bounds check.
    if (hitPos.x < 0.0 || hitPos.x > 1.0 ||
        hitPos.y < 0.0 || hitPos.y > 1.0 ||
        t >= tMax)
        return vec3(-1.0);

    return hitPos;
}

float calculateEdgeFade(vec2 screenPos)
{
    vec2 distFromCenter = abs(screenPos * 2.0 - 1.0);
    vec2 fade = smoothstep(0.85, 1.0, distFromCenter);
    return 1.0 - max(fade.x, fade.y);
}

float tapScreenSpaceReflection(
    int totalSamples,
    vec2 tc,
    vec3 viewPos,
    vec3 n,
    inout vec4 collectedColor,
    sampler2D source,
    float glossiness)
{
#ifdef TRANSPARENT_SURFACE
    collectedColor = vec4(1, 0, 1, 1);
    return 0;
#endif

    float roughness = 1.0 - glossiness;

    if (roughness >= maxRoughness)
        return 0.0;

    vec3 viewDir = normalize(viewPos);
    vec3 normal = normalize(n);

    float viewDotNormal = dot(-viewDir, normal);
    if (viewDotNormal <= 0.0)
    {
        collectedColor = vec4(0.0);
        return 0.0;
    }

    vec2 distFromCenter = abs(tc * 2.0 - 1.0);
    float baseEdgeFade = 1.0 - smoothstep(0.85, 1.0, max(distFromCenter.x, distFromCenter.y));
    if (baseEdgeFade <= 0.001)
    {
        collectedColor = vec4(0.0);
        return 0.0;
    }

    // Bias the ray origin along the normal, scaled by distance.
    // Prevents grazing-angle rays from scraping the originating surface
    // at distance where depth precision breaks down.
    float depthBias = max(0.01, -viewPos.z * DEPTH_BIAS);
    vec3 biasedPos = viewPos - normal * depthBias;

    vec3 transformedPos = (inv_modelview_delta * vec4(biasedPos, 1.0)).xyz;
    float startDepth = -transformedPos.z;

    if (startDepth > maxZDepth)
    {
        collectedColor = vec4(0.0);
        return 0.0;
    }

    vec3 perfectReflDir = normalize(reflect(viewDir, normal));

    int numSamples = max(1, int(glossySampleCount));
    vec3 accumColor = vec3(0.0);
    float accumFade = 0.0;
    int hits = 0;

    for (int s = 0; s < numSamples; s++)
    {
        vec3 reflectDir = perfectReflDir;

        // Jitter reflection direction based on roughness (importance-sampled GGX)
        if (roughness > 0.001)
        {
            float alpha = roughness * roughness;
            float u1 = random(tc * screen_res + noiseSine + float(s) * 0.123);
            float u2 = random(tc * screen_res * 1.7 + noiseSine + float(s) * 0.456 + 0.5);

            float theta = atan(alpha * sqrt(u1) / sqrt(1.0 - u1));
            float phi = 2.0 * 3.14159265 * u2;

            vec3 up = abs(reflectDir.y) < 0.999 ? vec3(0, 1, 0) : vec3(1, 0, 0);
            vec3 tangent = normalize(cross(up, reflectDir));
            vec3 bitangent = cross(reflectDir, tangent);

            vec3 h = normalize(
                sin(theta) * cos(phi) * tangent +
                sin(theta) * sin(phi) * bitangent +
                cos(theta) * reflectDir
            );

            reflectDir = normalize(reflect(-reflectDir, h));
        }

        vec3 reflTarget = viewPos + reflectDir;
        vec3 transformedTarget = (inv_modelview_delta * vec4(reflTarget, 1.0)).xyz;
        vec3 transformedReflDir = normalize(transformedTarget - transformedPos);

        if (transformedReflDir.z >= 0.5)
            continue;

        // Push ray origin along surface normal to prevent self-intersection.
        // Deterministic (not random) to avoid per-pixel noise.
        // Scales with distance to match depth-buffer precision degradation.
        float clearance = max(0.05, -viewPos.z * 0.002);
        vec3 clearedPos = biasedPos + normal * clearance;
        vec3 transformedClearedPos = (inv_modelview_delta * vec4(clearedPos, 1.0)).xyz;

        // Project ray origin and a nearby point to screen space (UV + raw depth).
        // Use a short step (fraction of distance-to-camera) to ensure the end
        // point stays in front of the camera — projecting at maxZDepth can wrap
        // behind the camera for reflections going toward the viewer.
        vec3 ssOrigin = vec3(generateProjectedPosition(transformedClearedPos),
                             projectDepth(transformedClearedPos));
        float stepDist = max(0.1, -transformedClearedPos.z * 0.1);
        vec3 transformedEnd = transformedClearedPos + transformedReflDir * stepDist;
        vec3 ssFar = vec3(generateProjectedPosition(transformedEnd),
                          projectDepth(transformedEnd));
        vec3 ssDir = normalize(ssFar - ssOrigin);

        vec3 result = hiZTrace(ssOrigin, ssDir, int(iterationCount.x));

        if (result.x < 0.0)
            continue;

        vec2 hitTC = result.xy;

        // Read actual surface depth at the hit pixel (mip 0, exact pixel).
        ivec2 hitPixel = ivec2(hitTC * screen_res);
        hitPixel = clamp(hitPixel, ivec2(0), ivec2(screen_res) - 1);
        float hitSurfaceDepth = texelFetch(sceneDepth, hitPixel, 0).r;

        // Reject sky / far-plane hits (forward-Z: 1.0 = far).
        float hitDepth = -getPositionWithDepth(hitTC, hitSurfaceDepth).z;
        if (hitDepth > maxZDepth)
            continue;

        // Confidence validation:
        // Compare the trace's final depth with the actual surface depth at that pixel.
        // Use a dead zone so small mismatches (from cell-boundary snapping) don't
        // reduce confidence — only penalize when the gap is significant.
        // No squaring: let the smoothstep gradient be gentle to avoid banding.
        vec3 viewSpaceSurface = getPositionWithDepth(hitTC, hitSurfaceDepth).xyz;
        vec3 viewSpaceHit = getPositionWithDepth(hitTC, result.z).xyz;
        float hitDistance = length(viewSpaceSurface - viewSpaceHit);
        float confidenceStart = MAX_THICKNESS * 0.25;
        float confidence = 1.0 - smoothstep(confidenceStart, MAX_THICKNESS, hitDistance);

        // Screen-space ray length in UV units (matches Godot ray_len).
        // Godot: ray_len = length(screen_ray_dir.xy * t)
        // Here we approximate with the UV displacement of the hit.
        float rayLen = length(result.xy - ssOrigin.xy);

        float edgeFade = calculateEdgeFade(hitTC);

        float zFadeStart = maxZDepth * 0.8;
        float zFade = 1.0 - smoothstep(zFadeStart, maxZDepth, hitDepth);

        float maxMipLevels = floor(log2(max(screen_res.x, screen_res.y)));
        float distanceFactor = clamp(rayLen, 0.0, 1.0);
        float effectiveRoughness = clamp(roughness + distanceFactor * roughness, 0.0, 1.0);
        float mipLevel = maxMipLevels * effectiveRoughness;
        vec4 sampledColor = textureLod(source, hitTC, mipLevel);

        float rayFade = 1.0 - smoothstep(0.6, 1.0, rayLen);
        float sampleFade = edgeFade * zFade * rayFade * confidence;

        accumColor += sampledColor.rgb;
        accumFade += sampleFade;
        hits++;
    }

    if (hits == 0)
    {
        collectedColor = vec4(0.0);
        return 0.0;
    }

    accumColor /= float(numSamples);
    accumFade /= float(numSamples);

    float remappedRoughness = clamp((roughness - (maxRoughness * 0.6)) / (maxRoughness - (maxRoughness * 0.6)), 0.0, 1.0);
    float roughnessFade = 1.0 - remappedRoughness;

    float combinedFade = accumFade * roughnessFade * baseEdgeFade;

    collectedColor = vec4(accumColor, combinedFade);
    return 1.0;
}
