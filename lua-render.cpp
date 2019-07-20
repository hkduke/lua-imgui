extern "C" {
    #include <lua.h>
    #include <lauxlib.h> 
}
#include <GL/glew.h> 
#include <windows.h>
#include "imgui.h"

static GLuint _program;
static GLuint _vbo;
static GLuint _ebo;

static GLchar* _vs =
    "#version 300 es\n"
    "precision mediump float;\n"
    "layout (location = 0) in vec2 Position;\n"
    "layout (location = 1) in vec2 UV;\n"
    "layout (location = 2) in vec4 Color;\n"
    "uniform mat4 ProjMtx;\n"
    "out vec2 Frag_UV;\n"
    "out vec4 Frag_Color;\n"
    "void main()\n"
    "{\n"
    "    Frag_UV = UV;\n"
    "    Frag_Color = Color;\n"
    "    gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
    "}\n";

static GLchar* _fs =
    "#version 300 es\n"
    "precision mediump float;\n"
    "uniform sampler2D Texture;\n"
    "in vec2 Frag_UV;\n"
    "in vec4 Frag_Color;\n"
    "layout (location = 0) out vec4 Out_Color;\n"
    "void main()\n"
    "{\n"
    "    Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
    "}\n";

static GLuint
_load_shader(const char *glsl, int type)
{
    GLuint shader = glCreateShader(type);
    const GLchar *source[] = {glsl};
    glShaderSource(shader, 1, source, NULL);
    glCompileShader(shader);
    GLint ret = GL_TRUE;
    glGetShaderiv(shader,GL_COMPILE_STATUS,&ret);
    char buf[1024];
    GLint len;
    if(ret == GL_FALSE){
        glGetShaderInfoLog(shader,1024,(GLint*)&len,buf);
        printf("Compile Shader fail error log: %s \nshader code:\n%s\n",buf,ret);
        exit(0);
    }
    return shader;
}

GLuint 
_compile_shader(const char *vs, const char *fs){
    GLuint vertex_shader = _load_shader(vs, GL_VERTEX_SHADER);
    GLuint fragment_shader = _load_shader(fs, GL_FRAGMENT_SHADER);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program;
}

static void
_set_pixel_format_to_hdc(HDC hDC){
    int color_deep;
    PIXELFORMATDESCRIPTOR pfd;

    color_deep = GetDeviceCaps(hDC, BITSPIXEL);

    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = color_deep;
    pfd.cDepthBits = 0;
    pfd.cStencilBits = 0;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pixelFormat = ChoosePixelFormat(hDC, &pfd);

    SetPixelFormat(hDC, pixelFormat, &pfd);
}

static void 
_initgl(HDC dc,int w,int h){
    _set_pixel_format_to_hdc(dc);
    HGLRC glrc = wglCreateContext(dc);
    wglMakeCurrent(dc, glrc);
    if (glewInit() != GLEW_OK)
    {
        printf("el init fail\n");
        exit(0);
    };
    glViewport(0, 0, w,h);
}

static void
_create_font_texture(){
    // Build texture atlas
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width, height;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bits (75% of the memory is wasted, but default font is so small) because it is more likely to be compatible with user's existing shaders. If your ImTextureId represent a higher-level concept than just a GL texture id, consider calling GetTexDataAsAlpha8() instead to save on GPU memory.
    // Upload texture to graphics system
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    io.Fonts->TexID = (ImTextureID)(intptr_t)texture;
}


static void
_setup_render_state(ImDrawData* draw_data,
                    int fb_width,
                    int fb_height,
                    GLuint vertex_array_object)
 {
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);

    // Setup viewport,  projection matrix
    // Our visible imgui space lies from draw_data->DisplayPos (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
    glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);
    float L = draw_data->DisplayPos.x;
    float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
    float T = draw_data->DisplayPos.y;
    float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
    const float ortho_projection[4][4] =
    {
        { 2.0f/(R-L),   0.0f,         0.0f,   0.0f },
        { 0.0f,         2.0f/(T-B),   0.0f,   0.0f },
        { 0.0f,         0.0f,        -1.0f,   0.0f },
        { (R+L)/(L-R),  (T+B)/(B-T),  0.0f,   1.0f },
    };
    glUniform1i(0, 0);
    glUniformMatrix4fv(0, 1, GL_FALSE, &ortho_projection[0][0]);
    glBindVertexArray(vertex_array_object);
    // Bind vertex/index buffers and setup attributes for ImDrawVert
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0,   2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, pos));
    glVertexAttribPointer(1,    2, GL_FLOAT,         GL_FALSE, sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, uv));
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE,  sizeof(ImDrawVert), (GLvoid*)IM_OFFSETOF(ImDrawVert, col));
}

static void
_render(ImDrawData *draw_data){
     // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(draw_data->DisplaySize.x * draw_data->FramebufferScale.x);
    int fb_height = (int)(draw_data->DisplaySize.y * draw_data->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;
    bool clip_origin_lower_left = true; 
    GLuint vertex_array_object = 0;
    glGenVertexArrays(1, &vertex_array_object);
    _setup_render_state(draw_data, fb_width, fb_height, vertex_array_object);
    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = draw_data->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = draw_data->FramebufferScale; // (1,1) unless using retina display which are often (2,2)
    // Render command lists
    for (int n = 0; n < draw_data->CmdListsCount; n++)
    {
        const ImDrawList* cmd_list = draw_data->CmdLists[n];
        // Upload vertex/index buffers
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), (const GLvoid*)cmd_list->VtxBuffer.Data, GL_STREAM_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), (const GLvoid*)cmd_list->IdxBuffer.Data, GL_STREAM_DRAW);

        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != NULL)
            {
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec4 clip_rect;
                clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
                clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
                clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
                clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

                if (clip_rect.x < fb_width && clip_rect.y < fb_height && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f)
                {
                    glScissor((int)clip_rect.x, (int)(fb_height - clip_rect.w), (int)(clip_rect.z - clip_rect.x), (int)(clip_rect.w - clip_rect.y));
                    // Bind texture, Draw
                    glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
                    glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, (void*)(intptr_t)(pcmd->IdxOffset * sizeof(ImDrawIdx)));
                }
            }
        }
    }
    glDeleteVertexArrays(1, &vertex_array_object);
}

static int
lrender(lua_State *L){
    HDC dc = (HDC)luaL_checkinteger(L,1);
    glUseProgram(_program);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f); 
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui::Render();
    _render(ImGui::GetDrawData());
    SwapBuffers(dc);
    return 0;
}

static int
linit(lua_State *L){
    HDC dc = (HDC)luaL_checkinteger(L,1);
    int w = luaL_checkinteger(L,2);
    int h = luaL_checkinteger(L,3);
    _initgl(dc,w,h);
    _program = _compile_shader(_vs,_fs);
    glGenBuffers(1,&_vbo);
    glGenBuffers(1,&_ebo);
    _create_font_texture();
    return 0;
}

extern "C" int
luaopen_imgui_render(lua_State *L){
    luaL_Reg l[] = {
        {"init",linit},
        {"render",lrender},
        {NULL,NULL}
    };
    luaL_newlib(L,l);
    return 1;
}

extern "C" {
    int luaopen_window(lua_State *L);
}

extern "C"
luaopen_imgui_window(lua_State *L){
    return luaopen_window(L);
}