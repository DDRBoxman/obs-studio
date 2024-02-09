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

string test_hlsl = R"(
uniform float4x4 ViewProj;
uniform float4 color = {1.0, 1.0, 1.0, 1.0};

struct SolidVertInOut {
    float4 pos : POSITION;
};

float4 PSSolid(SolidVertInOut vert_in) : TARGET
{
    return color;
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

class Shadertoy {
public:
    Shadertoy(obs_source_t *source) : mSource{source} {
        width = 1024;
        height = 768;
        
        obs_enter_graphics();

        auto effect_file = obs_module_file("test.effect");
        
       // auto glsl = variables_glsl + shadertoy_glsl + fragment_glsl;
        
        char* error;
        
        effect = gs_effect_create(simple_hlsl.c_str(), NULL, &error);
        
        
        blog(LOG_DEBUG,
             "error %s",
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
