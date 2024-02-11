#include <obs-module.h>
#include <string>
using namespace std;

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("shadertoy", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "OBS Shadertoy module";
}

string variables_glsl = R"(
#version 450 core

vec4 i_mouse;
vec4 i_date;
vec3 i_resolution;
float i_time;
vec3 i_channel_resolution[4];
float i_time_delta;
int i_frame;
float i_framerate;

layout(binding = 1) uniform texture2D i_channel0;
layout(binding = 2) uniform sampler sampler0;
layout(binding = 3) uniform texture2D i_channel1;
layout(binding = 4) uniform sampler sampler1;
layout(binding = 5) uniform texture2D i_channel2;
layout(binding = 6) uniform sampler sampler2;
layout(binding = 7) uniform texture2D i_channel3;
layout(binding = 8) uniform sampler sampler3;

// Shadertoy compatibility, see we can use the same code copied from shadertoy website

#define iChannel0 sampler2D(i_channel0, sampler0)
#define iChannel1 sampler2D(i_channel1, sampler1)
#define iChannel2 sampler2D(i_channel2, sampler2)
#define iChannel3 sampler2D(i_channel3, sampler3)

#define iMouse i_mouse
#define iDate i_date
#define iResolution i_resolution
#define iTime i_time
#define iChannelResolution i_channel_resolution
#define iTimeDelta i_time_delta
#define iFrame i_frame
#define iFrameRate i_framerate

#define mainImage shader_main
)";

string shadertoy_glsl = R"(
/* This animation is the material of my first youtube tutorial about creative
   coding, which is a video in which I try to introduce programmers to GLSL
   and to the wonderful world of shaders, while also trying to share my recent
   passion for this community.
                                       Video URL: https://youtu.be/f4s1h2YETNY
*/

//https://iquilezles.org/articles/palettes/
vec3 palette( float t ) {
    vec3 a = vec3(0.5, 0.5, 0.5);
    vec3 b = vec3(0.5, 0.5, 0.5);
    vec3 c = vec3(1.0, 1.0, 1.0);
    vec3 d = vec3(0.263,0.416,0.557);

    return a + b*cos( 6.28318*(c*t+d) );
}

//https://www.shadertoy.com/view/mtyGWy
void mainImage( out vec4 fragColor, in vec2 fragCoord ) {
    vec2 uv = (fragCoord * 2.0 - iResolution.xy) / iResolution.y;
    vec2 uv0 = uv;
    vec3 finalColor = vec3(0.0);
    
    for (float i = 0.0; i < 4.0; i++) {
        uv = fract(uv * 1.5) - 0.5;

        float d = length(uv) * exp(-length(uv0));

        vec3 col = palette(length(uv0) + i*.4 + iTime*.4);

        d = sin(d*8. + iTime)/8.;
        d = abs(d);

        d = pow(0.01 / d, 1.2);

        finalColor += col * d;
    }
        
    fragColor = vec4(finalColor, 1.0);
}
)";

string fragment_glsl = R"(
layout(location = 0) in vec2 uv;

struct ShadertoyInput {
    vec4 mouse;
    vec4 date;
    vec3 resolution;
    float time;
    vec3 channel_res[4];
    float time_delta;
    int frame;
    float framerate;
};

layout(binding = 0) uniform ShadertoyInput input;
out vec4 FragColor;
void main(){

    i_mouse = input.mouse;
    i_date = input.date;
    i_resolution = input.resolution;
    i_time = input.time;
    i_channel_resolution = input.channel_res;
    i_time_delta = input.time_delta;
    i_frame = input.frame;
    i_framerate = input.framerate;
    vec2 uv = vec2(uv.x, 1.0 - uv.y);
    vec2 frag_coord = uv * i_resolution.xy;

    shader_main(FragColor, frag_coord);

}
)";

string _test_hlsl = R"(
uniform float4x4 ViewProj;
uniform float4 color = {1.0, 1.0, 1.0, 1.0};

struct SolidVertInOut {
    float4 pos : POSITION;
};

float4 PSSolid(SolidVertInOut vert_in) : TARGET
{
    float4 fragColor;
    mainImage(fragColor, pos.xy);
    return fragColor;
}

SolidVertInOut VSSolid(SolidVertInOut vert_in)
{
    SolidVertInOut vert_out;
    vert_out.pos = mul(float4(vert_in.pos.xyz, 1.0), ViewProj);
    return vert_out;
}

technique Solid
{
    pass
    {
        vertex_shader = VSSolid(vert_in);
        pixel_shader  = PSSolid(vert_in);
    }
}
)";

string simple_hlsl = R"(
uniform float4x4 ViewProj;
uniform float4 color = {1.0, 1.0, 1.0, 1.0};

void mainImage(inout float4 fragColor, float2 fragCoord)
{
    fragColor = color;
    return;
}

struct SolidVertInOut {
    float4 pos : POSITION;
};

float4 PSSolid(SolidVertInOut vert_in) : TARGET
{
    float4 local;
    mainImage(local, vert_in.pos.xy);
    return local;
}

SolidVertInOut VSSolid(SolidVertInOut vert_in)
{
    SolidVertInOut vert_out;
    vert_out.pos = mul(float4(vert_in.pos.xyz, 1.0), ViewProj);
    return vert_out;
}

technique Solid
{
    pass
    {
        vertex_shader = VSSolid(vert_in);
        pixel_shader  = PSSolid(vert_in);
    }
}

)";

string test_hlsl = R"(
struct type_10 {
    float4 FragColor : SV_Target0;
};

uniform float4 iMouse;
uniform float4 iDate;
uniform float3 iResolution;
uniform float iTime;
uniform float3 iChannelResolution[4];
uniform float iTimeDelta;
uniform int iFrame;
uniform float iFramerate;
uniform float2 pos_1;
uniform float4 FragColor;

struct FragmentInput_main {
    float2 pos_2 : LOC0;
};

float3 palette(float t)
{
    float t_1 = float(0);
    float3 a = float3(0.5, 0.5, 0.5);
    float3 b = float3(0.5, 0.5, 0.5);
    float3 c = float3(1.0, 1.0, 1.0);
    float3 d = float3(0.263, 0.416, 0.557);

    t_1 = t;
    float3 _expr30 = a;
    float3 _expr31 = b;
    float3 _expr33 = c;
    float _expr34 = t_1;
    float3 _expr36 = d;
    float3 _expr40 = c;
    float _expr41 = t_1;
    float3 _expr43 = d;
    return (_expr30 + (_expr31 * cos((6.28318 * ((_expr40 * _expr41) + _expr43)))));
}

void mainImage(inout float4 fragColor, float2 fragCoord)
{
    float2 fragCoord_1 = float2(0);
    float2 uv = float2(0);
    float2 uv0_ = float2(0);
    float3 finalColor = float3(0);
    float i = float(0.0);
    float d_1 = float(0);
    float3 col = float3(0);

    fragCoord_1 = fragCoord;
    float2 _expr11 = fragCoord_1;
    float3 _expr14 = iResolution;
    float3 _expr17 = iResolution;
    uv = (((_expr11 * 2.0) - _expr14.xy) / (_expr17.y).xx);
    float2 _expr22 = uv;
    uv0_ = _expr22;
    bool loop_init = true;
    while(true) {
        if (!loop_init) {
            float _expr33 = i;
            i = (_expr33 + 1.0);
        }
        loop_init = false;
        float _expr29 = i;
        if (!((_expr29 < 4.0))) {
            break;
        }
        {
            float2 _expr36 = uv;
            float2 _expr39 = uv;
            uv = (frac((_expr39 * 1.5)) - (0.5).xx);
            float2 _expr47 = uv;
            float2 _expr50 = uv0_;
            float2 _expr54 = uv0_;
            d_1 = (length(_expr47) * exp(-(length(_expr54))));
            float2 _expr61 = uv0_;
            float _expr63 = i;
            float _expr67 = iTime;
            float2 _expr72 = uv0_;
            float _expr74 = i;
            float _expr78 = iTime;
            const float3 _e82 = float3(0);
            _e82 = palette(((length(_expr72) + (_expr74 * 0.4)) + (_expr78 * 0.4)));
            col = _e82;
            float _expr84 = d_1;
            float _expr87 = iTime;
            float _expr89 = d_1;
            float _expr92 = iTime;
            d_1 = (sin(((_expr89 * 8.0) + _expr92)) / 8.0);
            float _expr98 = d_1;
            d_1 = abs(_expr98);
            float _expr101 = d_1;
            float _expr105 = d_1;
            d_1 = pow((0.01 / _expr105), 1.2);
            //float3 _expr109 = finalColor;
            //float3 _expr110 = col;
            //float _expr111 = d_1;
            finalColor = (finalColor + (col * d_1));
        }
    }
    float3 _expr114 = finalColor;
    fragColor = float4(_expr114.x, _expr114.y, _expr114.z, 1.0);
    return;
}

void main_1()
{
    float4 local = float4(0);

    float2 _expr14 = float2(pos_1);
    mainImage(local, _expr14);
    float4 _expr15 = local;
    FragColor = _expr15;
    return;
}

type_10 Constructtype_10(float4 arg0) {
    type_10 ret = 0;
    ret.FragColor = arg0;
    return ret;
}

type_10 main(FragmentInput_main fragmentinput_main)
{
    float2 pos = fragmentinput_main.pos_2;
    pos_1 = pos;
    main_1();
    float4 _expr23 = FragColor;
    const type_10 type_10_ = Constructtype_10(_expr23);
    return type_10_;
}



)";

string entry_hlsl = R"(
uniform float4x4 ViewProj;

struct SolidVertInOut {
    float4 pos : POSITION;
};

float4 PSSolid(SolidVertInOut vert_in) : TARGET
{
    float4 fragColor;
    mainImage(fragColor, vert_in.pos.xy);
    return fragColor;
}

SolidVertInOut VSSolid(SolidVertInOut vert_in)
{
    SolidVertInOut vert_out;
    vert_out.pos = mul(float4(vert_in.pos.xyz, 1.0), ViewProj);
    return vert_out;
}

technique Solid
{
    pass
    {
        vertex_shader = VSSolid(vert_in);
        pixel_shader  = PSSolid(vert_in);
    }
}
)";

class Shadertoy {
public:
    Shadertoy(obs_source_t *source) : mSource{source} {
        width = 1024;
        height = 768;
        
        obs_enter_graphics();

        auto effect_file = obs_module_file("test.effect");
        
       // auto glsl = variables_glsl + shadertoy_glsl + fragment_glsl;
        
        char* error;
        
        effect = gs_effect_create((test_hlsl + entry_hlsl).c_str(), NULL, &error);
        
        
        blog(LOG_DEBUG,
             "effect error %s",
             error);
        
        bfree(error);
        
        //effect = gs_effect_create_from_file(effect_file, NULL);
        bfree(effect_file);

        obs_leave_graphics();
    }
    
    uint32_t getWidth() {
        return width;
    }
    
    uint32_t getHeight() {
        return height;
    }
    
    void render() {
        struct vec4 colorVal;
        vec4_from_rgba(&colorVal, -16711936);
        
        struct vec2 resolution;
        resolution.x = width;
        resolution.y = height;
        
        //gs_eparam_t *i_resolution = gs_effect_get_param_by_name(effect, "i_resolution");
        //gs_eparam_t *color = gs_effect_get_param_by_name(effect, "color");
        gs_technique_t *tech = gs_effect_get_technique(effect, "Solid");
        
       // gs_effect_set_vec2(i_resolution, &resolution);
        //gs_effect_set_vec4(color, &colorVal);

        gs_technique_begin(tech);
        gs_technique_begin_pass(tech, 0);

        gs_draw_sprite(0, 0, width, height);

        gs_technique_end_pass(tech);
        gs_technique_end(tech);
    }
    
private:
    obs_source_t *mSource;
    gs_effect_t *effect;
    
    uint32_t width;
    uint32_t height;
};

bool obs_module_load()
{
    struct obs_source_info info = {};
    
    info.id = "shadertoy_source";
    info.type = OBS_SOURCE_TYPE_INPUT;
    info.output_flags = OBS_SOURCE_VIDEO;
    info.get_name = [](void *) -> const char * {
        return obs_module_text("Source.Name");
    };
    info.create = [](obs_data_t *, obs_source_t *source) -> void * {
        return new Shadertoy(source);
    };
    info.destroy = [](void *data) {
        delete static_cast<Shadertoy *>(data);
    };
    info.get_width = [](void *data) -> uint32_t {
        return static_cast<Shadertoy *>(data)->getWidth();
    };
    info.get_height = [](void *data) -> uint32_t {
        return static_cast<Shadertoy *>(data)->getHeight();
    };
    info.video_render = [](void *data, gs_effect_t *) {
        static_cast<Shadertoy *>(data)->render();
    };

    obs_register_source(&info);

	return true;
}
