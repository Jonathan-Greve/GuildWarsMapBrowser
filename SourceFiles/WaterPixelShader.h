#pragma once

struct WaterPixelShader
{
    static constexpr char shader_ps[] = R"(
#define CHECK_TEXTURE_SET(TYPE) TYPE == texture_type

sampler ss : register(s0);
SamplerState ssClampLinear : register(s2);
SamplerState ssWrapLinear : register(s3);
Texture2D shaderTextures[8] : register(t0);

struct DirectionalLight
{
    float4 ambient;
    float4 diffuse;
    float4 specular;
    float3 direction;
    float pad;
};

cbuffer PerFrameCB : register(b0)
{
    DirectionalLight directionalLight;
    float time_elapsed;
    float3 fog_color_rgb;
    float fog_start;
    float fog_end;
    float fog_start_y; // The height at which fog starts.
    float fog_end_y; // The height at which fog ends.
    uint should_render_flags; // Shadows, Water reflection, fog (shadows at bit 0, water reflection at bit 1, fog at bit 2)
};

cbuffer PerObjectCB : register(b1)
{
    matrix World;
    uint4 uv_indices[2];
    uint4 texture_indices[2];
    uint4 blend_flags[2];
    uint4 texture_types[2];
    uint num_uv_texture_pairs;
    uint object_id;
    uint highlight_state; // 0 is not hightlight, 1 is dark green, 2 is lightgreen
    float pad1[1];
};

cbuffer PerCameraCB : register(b2)
{
    matrix View;
    matrix Projection;
    matrix directional_light_view;
    matrix directional_light_proj;
    matrix reflection_view;
    matrix reflection_proj;
    float3 cam_position;
    float2 shadowmap_texel_size;
    float2 reflection_texel_size;
};

cbuffer PerTerrainCB : register(b3)
{
    int grid_dim_x;
    int grid_dim_y;
    float min_x;
    float max_x;
    float min_y;
    float max_y;
    float min_z;
    float max_z;
    float water_level;
    float terrain_texture_pad_x;
    float terrain_texture_pad_y;
    float water_distortion_tex_scale; // GW: secondaryTexScale (file EnvWaterParams.secondaryTexScale)
    float water_distortion_scale;     // GW: secondaryTexScale (duplicated; used to match GW bump-env scale)
    float water_distortion_tex_speed; // GW: secondarySpeed    (file EnvWaterParams.secondarySpeed)
    float water_color_tex_scale;      // GW: primaryTexScale   (file EnvWaterParams.primaryTexScale)
    float water_color_tex_speed;      // GW: primarySpeed      (file EnvWaterParams.primarySpeed)
    float water_fresnel;             // GW: fresnel           (file EnvWaterParams.fresnel) (not used by the captured PS)
    float water_specular_scale;      // GW: specularScale     (file EnvWaterParams.specularScale) (not used by the captured PS)
    uint water_technique;
    float water_runtime_param_28;
    float4 color0; // GW: waterAbsorption (PS c1)  (float RGBA / 255)
    float4 color1; // GW: waterPattern    (PS c0)  (float RGBA / 255)
    float2 water_flow_dir;
    float2 water_flow_padding;
};

struct PixelInputType
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 lightingColor : COLOR0;
    float2 tex_coords0 : TEXCOORD0;
    float2 tex_coords1 : TEXCOORD1;
    float2 tex_coords2 : TEXCOORD2;
    float2 tex_coords3 : TEXCOORD3;
    float2 tex_coords4 : TEXCOORD4;
    float2 tex_coords5 : TEXCOORD5;
    float4 reflectionSpacePos : TEXCOORD6;
    float4 lightSpacePos : TEXCOORD7;
    float3 world_position : TEXCOORD8;
    float3x3 TBN : TEXCOORD9;
};

struct PSOutput
{
    float4 rt_0_output : SV_TARGET0; // Goes to first render target (usually the screen)
    float4 rt_1_output : SV_TARGET1; // Goes to second render target
};

// Constants from Gw.exe MapWater_render:
// - Primary scroll base is 0.01
// - Secondary scroll base is 0.02
// - MapWater_render: primary scale uses 0.00025, secondary scale uses 0.0005
// - MapWater_render: bump-env matrix scale uses 0.04
static const float GW_WATER_PRIMARY_ANIM_SPEED = 0.01;
static const float GW_WATER_SECONDARY_ANIM_SPEED = 0.02;
static const float GW_WATER_PRIMARY_TEX_SCALE = 0.00025;
static const float GW_WATER_SECONDARY_TEX_SCALE = 0.0005;
static const float GW_WATER_BUMPENV_SCALE = 0.04;

// MapWater_update_technique builds a 256x4 A8 fresnel LUT where each row is identical.
// We generate/bind the same LUT in the custom client and sample it as shaderTextures[3].a
// (see MapRenderer::InitializeWaterFresnelLUT).

float2 NormalizeOrDefault(float2 value, float2 fallback)
{
    float2 result = fallback;
    float len_sq = dot(value, value);
    if (len_sq > 1e-6)
    {
        result = value * rsqrt(len_sq);
    }

    return result;
}

float GW_FogFactor(float3 world_pos)
{
    // Use depth fog for water. Vertical fog attenuation can collapse to near-zero
    // on some maps and tint the full surface to fog color.
    float4 view_pos = mul(float4(world_pos, 1.0), View);
    float view_depth = abs(view_pos.z);

    float dist_denom = max(fog_end - fog_start, 1e-3);
    float dist_factor = saturate((fog_end - view_depth) / dist_denom);
    return dist_factor;
}

// Main Pixel Shader function (GW texbem water PS reimplementation)
float4 main(PixelInputType input) : SV_TARGET
{
    // Textures are bound as:
    //   shaderTextures[0] = stage3 pattern/base texture (often DXT1)
    //   shaderTextures[1] = stage0 bump DuDv texture (DDS bump, treated as RG storing du/dv)
    //   shaderTextures[2] = stage1 reflection render target (SRV)
    //   shaderTextures[3] = stage2 fresnel LUT (256x4 A8, stored in .a)
    float4 final = float4(0, 0, 0, 1);

    float4 c0_pattern = color1;
    float4 c1_absorption = color0;
    float3 c2_fog_rgb = fog_color_rgb;

    float2 flow_dir = NormalizeOrDefault(water_flow_dir, float2(0.70710677, 0.70710677));
    float2 world_uv = input.world_position.xz;

    // Captured GW water shader uses:
    // - Stage0 (t0): bump/du-dv (file: secondary* params; VS uses GW_WATER_SECONDARY_* constants)
    // - Stage3 (t3): pattern/base (file: primary* params; VS uses GW_WATER_PRIMARY_* constants)
    // Scroll is in UV units (after the GW constant multipliers); do not multiply by the scale again.
    float bump_scale = water_distortion_tex_scale * GW_WATER_SECONDARY_TEX_SCALE;
    float bump_scroll = time_elapsed * GW_WATER_SECONDARY_ANIM_SPEED * water_distortion_tex_speed;
    float2 uv_bump = world_uv * bump_scale - flow_dir * bump_scroll;

    float pattern_scroll = time_elapsed * GW_WATER_PRIMARY_ANIM_SPEED * water_color_tex_speed;
    float pattern_scale = water_color_tex_scale * GW_WATER_PRIMARY_TEX_SCALE;
    // Captured GW path uses opposite scroll sign for stage3 (pattern) vs stage0 (bump).
    float2 uv_pattern = world_uv * pattern_scale + flow_dir * pattern_scroll;

    // Sample stage0 bump (DuDv). DDS bump formats typically land in RG with neutral at 0.5 after conversion.
    float4 t0 = shaderTextures[1].Sample(ssWrapLinear, uv_bump);
    // D3D9 V8U8 is signed bytes; after conversion to UNORM in our loader, recover the signed range.
    float2 du_dv = ((t0.rg * 255.0) - 128.0) / 127.0;
    du_dv = clamp(du_dv, -1.0, 1.0);

    // D3D9 texbem uses BUMPENVMAT00..11; observed in captures:
    //   BUMPENVMAT00 == BUMPENVMAT11 == (secondaryTexScale * 0.04)
    float bump_mat_diag = water_distortion_scale * GW_WATER_BUMPENV_SCALE;
    float2 bump_offset = du_dv * bump_mat_diag;

    // Reflection coords (MapBrowser provides projected coords; GW computes equivalent UVs in its VS path)
    // Important: Do not clamp invalid projected UVs to the texture border/corners as that creates
    // large quadrant-like color blocks on water when projection becomes unstable near horizon/clip.
    bool has_reflection_sample = false;
    float2 uv_reflect = float2(0.5, 0.5);
    if (should_render_flags & 2)
    {
        float rw = input.reflectionSpacePos.w;
        if (rw > 1e-4)
        {
            float3 ndcPos = input.reflectionSpacePos.xyz / rw;
            uv_reflect = float2(ndcPos.x * 0.5 + 0.5, -ndcPos.y * 0.5 + 0.5);
            // GW technique 4 applies an extra projection remap; keep support but default runtime param to 0.
            if (water_technique == 4)
            {
                float s = rcp(max(2.0 * water_runtime_param_28, 1e-3));
                uv_reflect = (uv_reflect - 0.5) * s + 0.5;
            }

            uv_reflect += bump_offset;

            float2 texel_guard = max(reflection_texel_size, float2(0.0, 0.0));
            float2 uv_min = texel_guard;
            float2 uv_max = 1.0 - texel_guard;
            bool in_bounds = all(uv_reflect >= uv_min) && all(uv_reflect <= uv_max);
            has_reflection_sample = in_bounds;
        }
    }

    // Fresnel LUT coordinate:
    // Captured GW VS: oT2.xy = (-viewDirUp) * primarySpeed, where viewDirUp is the *normalized* camera->point
    // vector component along the world-up axis (GW uses Z-up; our viewer uses Y-up).
    float3 v_cam_to_pt = input.world_position.xyz - cam_position;
    float inv_len = rsqrt(max(dot(v_cam_to_pt, v_cam_to_pt), 1e-12));
    float view_up = v_cam_to_pt.y * inv_len;
    float fresnel_coord = (-view_up) * water_color_tex_speed;
    float t2_a = shaderTextures[3].Sample(ssClampLinear, float2(fresnel_coord, fresnel_coord)).a;

    // texbem stage1 (reflection) and stage3 (pattern)
    float4 t1 = has_reflection_sample ? shaderTextures[2].Sample(ssClampLinear, uv_reflect) : float4(0, 0, 0, 0);
    float4 t3 = shaderTextures[0].Sample(ssWrapLinear, uv_pattern + bump_offset);

    // Apply the exact constant usage from ps_1_1:
    //   t3 *= c0 (pattern)
    //   t1 *= c1 (absorption)
    float3 t3_rgb = t3.rgb * c0_pattern.rgb;
    float  t3_w   = t3.a   * c0_pattern.a;

    float3 t1_rgb = t1.rgb * c1_absorption.rgb;
    float  t1_w   = (t2_a * c1_absorption.a);
    t1_rgb *= t1_w;

    // lrp r0.xyz, t3.w, t3, t1  => r0 = t3.w*t3 + (1-t3.w)*t1
    float3 rgb = lerp(t1_rgb, t3_rgb, t3_w);

    // alpha pre-fog: (1-t3.w)*(1-t1.w)
    float out_a = (1.0 - t3_w) * (1.0 - t1_w);

    // Fog matches ps: rgb = lerp(c2, rgb, fogFactor)
    float fogFactor = (should_render_flags & 4) ? GW_FogFactor(input.world_position.xyz) : 1.0;
    rgb = lerp(c2_fog_rgb, rgb, fogFactor);

    // mad r0.w, -out_a, fogFactor, 1 => alpha = 1 - out_a * fogFactor
    out_a = 1.0 - out_a * fogFactor;

    final = float4(rgb, out_a);
    return final;
}
)";
};
