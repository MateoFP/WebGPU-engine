const char* plane_shader =
R"(

struct MapData
{
    data : array<u32, 169>,
};

struct GUBO 
{
    projView    : mat4x4f,
    player_dest : vec2f,
    window      : vec2f,
    time        : f32,
    time_rb     : f32,
    time_a      : f32,
    debug       : u32,
};

struct InstanceData
{
    matrix      : mat4x4f,
    id          : u32,
    skinned     : u32,
    bone_offset : u32,
    color       : vec4f,
};

struct VertexInput 
{
    @location(0) position: vec3f,
    @location(1) uv: vec2f,
    @location(2) weights: vec4f,
    @location(3) joints: vec4u,
    @location(4) normals: vec3f,
};

struct VertexOutput 
{
	@builtin(position) position: vec4f,
	@location(0) uv: vec2f,
    @location(1) color: vec3f,
    @location(2) @interpolate(flat) texture_index: u32,
    @location(3) local_pos: vec2f,
    @location(4) player_pos: vec2f,
};

@group(0) @binding(0) var<uniform> gubo : GUBO;
@group(0) @binding(1) var<storage, read> instances : array<InstanceData>;
@group(0) @binding(2) var texture_array: texture_2d_array<f32>;
@group(0) @binding(3) var texture_sampler: sampler;
@group(0) @binding(4) var<storage, read> map : MapData;

const FLAG_BRICK:   u32 = 1u << 0u; 
const FLAG_FOG:     u32 = 1u << 1u;

fn GetFlag(x: u32, y: u32) -> u32 
{
    let cx = clamp(x, 0u, 12u);
    let cy = clamp(y, 0u, 12u);
    let index = (cy * 13u) + cx;
    let flags = map.data[index];

    let is_brick = (flags & FLAG_BRICK) != 0u;
    return u32(is_brick);
}

fn GetFogValue(x: u32, y: u32) -> f32 
{
    let cx = clamp(x, 0u, 12u);
    let cy = clamp(y, 0u, 12u);
    let index = (cy * 13u) + cx;
    let flags = map.data[index];

    let has_fog = (flags & FLAG_FOG) != 0u;
    if(has_fog)
    {
        return 1.0f;
    }
    else
    {
        return 0.0f;
    }
}

    @vertex
    fn vs_main(in: VertexInput, @builtin(instance_index) instance_idx: u32) -> VertexOutput
    {
        let instance = instances[instance_idx];
    
        let worldPosition = instance.matrix * vec4(in.position, 1.0);

        var out: VertexOutput; 
        out.position = gubo.projView * worldPosition;
        out.uv = in.uv;
        out.color = instance.color.xyz;
        out.texture_index = instance.id;
        out.local_pos = in.position.xy;

        return out;
    }

    @fragment
    fn fs_main(in: VertexOutput) -> @location(0) vec4f 
    {
        let map_uv = in.uv * 12;
        let tile_pos = vec2<i32>(floor(map_uv));
        let local_tile_uv = vec2<f32>(1.0) - fract(map_uv);
       
        let x = u32(tile_pos.x);
        let y = u32(tile_pos.y);

        let v_bl = GetFlag(x, y);
        let v_br = GetFlag(x + 1u, y);
        let v_tl = GetFlag(x, y + 1u);
        let v_tr = GetFlag(x + 1u, y + 1u);

        let tile_mask = (v_tl * 1u) | (v_tr * 2u) | (v_bl * 4u) | (v_br * 8u);

        let ddx = dpdx(map_uv);
        let ddy = dpdy(map_uv);

        let brick_color = textureSampleGrad(texture_array, texture_sampler, local_tile_uv, i32(2), ddx, ddy);
        let grass_color  = textureSampleGrad(texture_array, texture_sampler, local_tile_uv, i32(1), ddx, ddy); 
        var out_color: vec4<f32>;
 
        if(tile_mask == 0)
        {
            out_color =  grass_color;
        }
        else if(tile_mask == 10)
        {
            out_color =  brick_color;
        }
        else
        {
            let slot_offset = vec2<f32>(f32(tile_mask / 4u), f32(tile_mask % 4u));
            let alpha_uv = (slot_offset + local_tile_uv) * 0.25;
            let atlas_size = vec2<f32>(textureDimensions(texture_array, 0i)); 
            let pixel_coord = vec2<i32>(alpha_uv * atlas_size);
            let raw_mask = textureLoad(texture_array, pixel_coord, 0i, 0i).r;
            out_color = mix(grass_color, brick_color, raw_mask);
        }

        let fog_uv = in.uv * 13.0;
        let sample_pos = fog_uv - vec2<f32>(0.5);
        let fog_pos = vec2<i32>(floor(sample_pos));
        let local_fog_uv = fract(sample_pos);

        let fx = u32(fog_pos.x);
        let fy = u32(fog_pos.y);

        let f_bl = GetFogValue(fx, fy);
        let f_br = GetFogValue(fx + 1, fy);
        let f_tl = GetFogValue(fx, fy + 1);
        let f_tr = GetFogValue(fx + 1, fy + 1);

        let fog_top     = mix(f_tl, f_tr, local_fog_uv.x);
        let fog_bottom  = mix(f_bl, f_br, local_fog_uv.x);
        let fog_factor  = mix(fog_bottom, fog_top, local_fog_uv.y);
        let fog_color = vec3<f32>(0.0, 0.0, 0.0);

        out_color = vec4<f32>(mix(out_color.rgb, fog_color, fog_factor), out_color.a);
        
        if(gubo.debug == 1)
        {
            let line_thickness = 0.02;
            if (local_tile_uv.x < line_thickness || local_tile_uv.x > (1.0 - line_thickness) ||
                local_tile_uv.y < line_thickness || local_tile_uv.y > (1.0 - line_thickness)) 
            {
               return vec4f(1.0, 0.1, 0.0, 1.0);
            }
        }
        let corrected_rgb = pow(out_color.rgb, vec3f(1.0 / 2.2));   //MOVE TO SEPARATE SHADER
        return vec4f(corrected_rgb, out_color.a);                   //MOVE TO SEPARATE SHADER
    }
)";

const char* object_shader =
R"(

struct BoneBuffer 
{
    matrices: array<mat4x4<f32>>,
}

struct MapData
{
    data : array<u32, 169>,
};

struct GUBO 
{
    projView    : mat4x4f,
    player_dest : vec2f,
    window      : vec2f,
    time        : f32,
    time_rb     : f32,
    time_a      : f32,
    debug       : u32,
};

struct InstanceData
{
    matrix      : mat4x4f,
    id          : u32,
    skinned     : u32,
    bone_offset : u32,
    color       : vec4f,
};

struct VertexInput 
{
    @location(0) position: vec3f,
    @location(1) uv: vec2f,
    @location(2) weights: vec4f,
    @location(3) joints: vec4u,
    @location(4) normals: vec3f,
};

struct VertexOutput 
{
	@builtin(position) position: vec4f,
	@location(0) uv: vec2f,
    @location(1) color: vec3f,
    @location(2) @interpolate(flat) texture_index: u32,
    @location(3) w_pos : vec4f,
};

const FLAG_BRICK:   u32 = 1u << 0u; 
const FLAG_FOG:     u32 = 1u << 1u;

fn GetFogValue(x: u32, y: u32) -> f32 
{
    let cx = clamp(x, 0u, 12u);
    let cy = clamp(y, 0u, 12u);
    let index = (cy * 13u) + cx;
    let flags = map.data[index];

    let has_fog = (flags & FLAG_FOG) != 0u;
    if(has_fog)
    {
        return 1.0f;
    }
    else
    {
        return 0.0f;
    }
}

@group(0) @binding(0) var<uniform> gubo : GUBO;
@group(0) @binding(1) var<storage, read> instances : array<InstanceData>;
@group(0) @binding(2) var texture_array: texture_2d_array<f32>;
@group(0) @binding(3) var texture_sampler: sampler;
@group(0) @binding(4) var<storage, read> map : MapData;
@group(0) @binding(5) var<storage, read> bones : BoneBuffer;

    @vertex
    fn vs_main(in: VertexInput, @builtin(instance_index) instance_idx: u32) -> VertexOutput
    {
        var out: VertexOutput; 
        let instance = instances[instance_idx];
        
        var pos_local = vec4<f32>(in.position, 1.0);

        if(instance.skinned == 1)
        {
            let skinMatrix = 
                bones.matrices[in.joints.x + instance.bone_offset] * in.weights.x +
                bones.matrices[in.joints.y + instance.bone_offset] * in.weights.y +
                bones.matrices[in.joints.z + instance.bone_offset] * in.weights.z +
                bones.matrices[in.joints.w + instance.bone_offset] * in.weights.w;

            pos_local = skinMatrix * pos_local;
        }

        out.w_pos = instance.matrix * pos_local;
        out.position = gubo.projView * out.w_pos;
        out.uv = in.uv;
        out.color = instance.color.xyz;
        out.texture_index = instance.id;
        return out;
    }

    @fragment
    fn fs_main(in: VertexOutput) -> @location(0) vec4f 
    {
        var out_color: vec4<f32>;
        out_color = textureSample(texture_array, texture_sampler, in.uv, i32(in.texture_index));
        
        let world_min= vec2<f32>(0,0);
        let world_size = vec2<f32>(50,50);
        var map_uv = (in.w_pos.xy - world_min) / world_size;
        map_uv = clamp(map_uv, vec2<f32>(0.0), vec2<f32>(1.0));

        let fog_uv = map_uv * 13;
        let sample_pos = fog_uv - vec2<f32>(0.5);
        let fog_pos = vec2<i32>(floor(sample_pos));
        let local_fog_uv = fract(sample_pos);

        let fx = u32(fog_pos.x);
        let fy = u32(fog_pos.y);

        let f_bl = GetFogValue(fx, fy);
        let f_br = GetFogValue(fx + 1, fy);
        let f_tl = GetFogValue(fx, fy + 1);
        let f_tr = GetFogValue(fx + 1, fy + 1);

        let fog_top     = mix(f_tl, f_tr, local_fog_uv.x);
        let fog_bottom  = mix(f_bl, f_br, local_fog_uv.x);
        let fog_factor  = mix(fog_bottom, fog_top, local_fog_uv.y);
        let fog_color = vec3<f32>(0.0, 0.0, 0.0);

        out_color = vec4<f32>(mix(out_color.rgb, fog_color, fog_factor), out_color.a);

        if (out_color.a < 0.1) 
        {
            discard;
        }

        let corrected_rgb = pow(out_color.rgb, vec3f(1.0 / 2.2));   
        return vec4f(corrected_rgb, out_color.a);                   
    }
)";

const char* ui_shader =
R"(

struct InstanceDataUI
{
    uv    : vec2f,
    size   : vec2f,
    pos : vec2f,
};

struct GUBO 
{
    projView    : mat4x4f,
    player_dest : vec2f,
    window      : vec2f,
    time        : f32,
    time_rb     : f32,
    time_a      : f32,
    debug       : u32,
};

@group(0) @binding(0) var<uniform> gubo : GUBO;
@group(0) @binding(2) var texture_array: texture_2d_array<f32>;
@group(0) @binding(3) var texture_sampler: sampler;
@group(0) @binding(6) var<storage, read> instances_ui : array<InstanceDataUI>;

struct VertexOutput 
{
    @builtin(position) position: vec4f,
    @location(0) frag_pos: vec2f, 
};

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32, @builtin(instance_index) instance_idx: u32) -> VertexOutput 
{
    var output: VertexOutput;
    let instance = instances_ui[instance_idx];

    var unit_quad = array<vec2<f32>, 6>(
        vec2<f32>(0.0, 0.0), 
        vec2<f32>(1.0, 0.0), 
        vec2<f32>(0.0, 1.0), 

        vec2<f32>(0.0, 1.0),
        vec2<f32>(1.0, 0.0), 
        vec2<f32>(1.0, 1.0) 
    );

    let unit_pos = unit_quad[vertex_index];
    let local_size = unit_pos * instance.size;

    var pixel_pos = floor(instance.pos + vec2f(unit_pos.x * instance.size.x, unit_pos.y * instance.size.y) + 0.5);

    let atlas_size = vec2<f32>(512.0, 512.0);
    let uv_size = instance.size / atlas_size;
    let uv_start = instance.uv / atlas_size;
    
    let ndc_x = (pixel_pos.x / gubo.window.x) * 2.0 - 1.0;
    let ndc_y = 1.0 - (pixel_pos.y / gubo.window.y) * 2.0;
    output.position = vec4f(ndc_x, ndc_y, 0.0, 1.0);
    
    output.frag_pos = uv_start + (unit_pos * uv_size);
    return output;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f 
{
    var color = textureSample(texture_array, texture_sampler, in.frag_pos, i32(6));

    if (color.a < 0.1) 
    {
        discard;
    }

    return color;
}
)";

const char* wire_shader =
R"(

struct VertexOutput 
{
    @builtin(position) position: vec4f,
    @location(0) color: vec4f, 
    @location(1) normal : vec3f,
};

struct InstanceDataBB
{
    max     : vec4f,
    min     : vec4f,
    color   : vec4f,
    id      : u32,
    pad     : array<u32, 3>
};

struct GUBO 
{
    projView    : mat4x4f,
    player_dest : vec2f,
    window      : vec2f,
    time        : f32,
    time_rb     : f32,
    time_a      : f32,
    debug       : u32,
};

@group(0) @binding(0) var<uniform> gubo : GUBO;
@group(0) @binding(7) var<storage, read> instances : array<InstanceDataBB>;

fn GetQuadPos(instance: InstanceDataBB, v_idx: u32) -> vec3f 
{
    var quad_pos: vec3f;

    var unit_corners = array<vec3f, 8>(
        vec3f(-0.5, -0.5, -0.5), 
        vec3f( 0.5, -0.5, -0.5),
        vec3f( 0.5,  0.5, -0.5),
        vec3f(-0.5,  0.5, -0.5), 
        vec3f(-0.5, -0.5,  0.5), 
        vec3f( 0.5, -0.5,  0.5), 
        vec3f( 0.5,  0.5,  0.5),
        vec3f(-0.5,  0.5,  0.5)  
    );

    var triangle_indices = array<u32, 36>(
        0u, 3u, 2u,   0u, 2u, 1u,
        4u, 5u, 6u,   4u, 6u, 7u,
        0u, 1u, 5u,   0u, 5u, 4u,
        3u, 7u, 6u,   3u, 6u, 2u,
        0u, 4u, 7u,   0u, 7u, 3u,
        1u, 2u, 6u,   1u, 6u, 5u
    );

    let corner_idx = triangle_indices[v_idx];
    let unit_pos = unit_corners[corner_idx];

    let size = instance.max.xyz - instance.min.xyz;
    let center = (instance.min.xyz + instance.max.xyz) * 0.5;

    quad_pos = (unit_pos * size) + center;

    return quad_pos;
}

fn GetQuadNormal(v_idx: u32) -> vec3f 
{
    var normals = array<vec3f, 6>(
        vec3f( 0.0,  0.0, -1.0), 
        vec3f( 0.0,  0.0,  1.0), 
        vec3f( 0.0, -1.0,  0.0), 
        vec3f( 0.0,  1.0,  0.0), 
        vec3f(-1.0,  0.0,  0.0), 
        vec3f( 1.0,  0.0,  0.0)  
    );

    return normals[v_idx / 6u];
}

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32, @builtin(instance_index) instance_idx: u32) -> VertexOutput 
{
    var out: VertexOutput;
    var local_pos : vec3f;
    var local_normal : vec3f = vec3f(0.0, 0.0, 1.0);
    let instance = instances[instance_idx];

    switch(instance.id)
    {
        case 0: 
        { 
            local_pos = GetQuadPos(instance, vertex_index); 
            local_normal = GetQuadNormal(vertex_index);
        }
        default:{ local_pos = vec3f(0.0); local_normal = vec3f(0.0);  }
    }

    out.position = gubo.projView * vec4f(local_pos, 1.0);
    out.color = instance.color;
    out.normal = local_normal;

    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f 
{
    let light_dir = normalize(vec3f(0.5, 1.0, 0.8)); 
    let diff = max(dot(in.normal, light_dir), 0.2); 

    return vec4f(in.color.rgb * diff, in.color.a);
}
)";

const char* outline_shader =
R"(

struct VertexInput 
{
    @location(0) position: vec3f,
    @location(1) uv: vec2f,
    @location(2) weights: vec4f,
    @location(3) joints: vec4u,
    @location(4) normals: vec3f,
};

struct VertexOutput 
{
    @builtin(position) position: vec4f,
    @location(0) color: vec4f, 
};

struct BoneBuffer 
{
    matrices: array<mat4x4<f32>>,
}

struct InstanceData
{
    matrix      : mat4x4f,
    id          : u32,
    skinned     : u32,
    bone_offset : u32,
    color       : vec4f,
};

struct GUBO 
{
    projView    : mat4x4f,
    player_dest : vec2f,
    window      : vec2f,
    time        : f32,
    time_rb     : f32,
    time_a      : f32,
    debug       : u32,
};

@group(0) @binding(0) var<uniform> gubo : GUBO;
@group(0) @binding(1) var<storage, read> instances : array<InstanceData>;
@group(0) @binding(5) var<storage, read> bones : BoneBuffer;

@vertex
fn vs_main(in: VertexInput, @builtin(instance_index) instance_idx: u32) -> VertexOutput
{
    var out: VertexOutput;
    let instance = instances[instance_idx];

    var final_pos = in.position;
    var final_normal = in.normals;

    if(instance.skinned == 1)
    {
            let skinMatrix = 
                bones.matrices[in.joints.x + instance.bone_offset] * in.weights.x +
                bones.matrices[in.joints.y + instance.bone_offset] * in.weights.y +
                bones.matrices[in.joints.z + instance.bone_offset] * in.weights.z +
                bones.matrices[in.joints.w + instance.bone_offset] * in.weights.w;

            let skinned_pos_4 = skinMatrix * vec4f(in.position, 1.0);
            final_pos = skinned_pos_4.xyz;

            let skinMat3 = mat3x3f(
                skinMatrix[0].xyz,
                skinMatrix[1].xyz,
                skinMatrix[2].xyz);
            final_normal = normalize(skinMat3 * in.normals);
    }

    let thickness: f32 = 0.10;
    let fattened_pos = final_pos + (final_normal * thickness);

    let pos_world = instance.matrix * vec4f(fattened_pos, 1.0);

    out.position = gubo.projView * pos_world;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f 
{
    return vec4f(1.0, 0.0, 0.0, 0.5);
}
)";