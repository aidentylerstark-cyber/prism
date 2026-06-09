/**
 * @file compositorblitV.glsl
 *
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2026, Linden Research, Inc.
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

// Attribute-less quad for the compositor's layer blit. gl_VertexID
// expands to the unit square; blit_rect places it in NDC.

uniform vec4 blit_rect;

out vec2 tc;

void main()
{
    vec2 p = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));
    tc = p;
    vec2 ndc = mix(blit_rect.xy, blit_rect.zw, p);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
