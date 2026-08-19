#include "Gles.hpp"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cmath>
#include <cstddef>
#include <cstring>

#include "AnmManager.hpp"
#include "FileSystem.hpp"
#include "GameWindow.hpp"
#include "Supervisor.hpp"
#include "TouchButtons.hpp"

#ifdef USING_GL
#define GLSL_VERSION "#version 330 core\n"
#define GLSL_PRECISION
#else
#define GLSL_VERSION "#version 300 es\n"
#define GLSL_PRECISION "precision mediump float;\n"
#endif

// clang-format off

// Screen-space virtual button overlay shaders (independent of the game
// shader; vertex colors are straight RGBA, no .bgra swizzle).
const char *buttonVSSource =
    GLSL_VERSION
    "layout(location = 0) in vec2 a_Pos;\n"
    "layout(location = 1) in vec4 a_Color;\n"
    "layout(location = 2) in vec2 a_UV;\n"
    "uniform vec2 u_ScreenSize;\n"
    "out vec4 v_Color;\n"
    "out vec2 v_UV;\n"
    "void main() {\n"
    "    vec2 ndc = vec2(a_Pos.x / u_ScreenSize.x * 2.0 - 1.0,\n"
    "                    1.0 - a_Pos.y / u_ScreenSize.y * 2.0);\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "    v_Color = a_Color;\n"
    "    v_UV = a_UV;\n"
    "}\n";

const char *buttonFSSource =
    GLSL_VERSION
    GLSL_PRECISION
    "in vec4 v_Color;\n"
    "in vec2 v_UV;\n"
    "uniform sampler2D u_Tex;\n"
    "uniform bool u_UseTex;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    if (u_UseTex) {\n"
    "        vec4 tex = texture(u_Tex, v_UV);\n"
    "        FragColor = vec4(v_Color.rgb, v_Color.a * tex.a);\n"
    "    } else {\n"
    "        FragColor = v_Color;\n"
    "    }\n"
    "}\n";

const char *vertexShaderSource =
    GLSL_VERSION
    "uniform mat4 u_Model;\n"
    "uniform mat4 u_View;\n"
    "uniform mat4 u_Proj;\n"
    "uniform mat4 u_TextureMatrix;\n"
    "uniform bool u_ScreenSpace;\n"
    "uniform vec4 u_Viewport;\n"
    "\n"
    "layout(location = 0) in vec3 a_Position;\n"
    "layout(location = 1) in vec4 a_Color;\n"
    "layout(location = 2) in vec2 a_TexCoord;\n"
    "\n"
    "out vec4 v_Color;\n"
    "out vec2 v_TexCoord;\n"
    "out float v_FogFragCoord;\n"
    "\n"
    "void main() {\n"
    "    v_Color = a_Color.bgra;\n"
    "    if (u_ScreenSpace) {\n"
    "        float x = (a_Position.x - u_Viewport.x) / u_Viewport.z * 2.0 - 1.0;\n"
    "        float y = 1.0 - (a_Position.y - u_Viewport.y) / u_Viewport.w * 2.0;\n"
    "        gl_Position = vec4(x, y, a_Position.z, 1.0);\n"
    "        v_TexCoord = a_TexCoord;\n"
    "        v_FogFragCoord = a_Position.z;\n"
    "    } else {\n"
    "        vec4 worldPos = u_Model * vec4(a_Position, 1.0);\n"
    "        vec4 viewPos = u_View * worldPos;\n"
    "        gl_Position = u_Proj * viewPos;\n"
    "        v_TexCoord = (u_TextureMatrix * vec4(a_TexCoord, 1.0, 0.0)).xy;\n"
    "        v_FogFragCoord = length(viewPos.xyz);\n"
    "    }\n"
    "}\n";

const char *fragmentShaderSource =
    GLSL_VERSION
    GLSL_PRECISION
    "\n"
    "in vec4 v_Color;\n"
    "in vec2 v_TexCoord;\n"
    "in float v_FogFragCoord;\n"
    "\n"
    "uniform sampler2D u_Texture;\n"
    "uniform bool u_UseTexture;\n"
    "uniform int u_ColorOpRgb;\n"
    "uniform int u_ColorOpAlpha;\n"
    "uniform int u_TexArg;\n"
    "uniform vec4 u_TextureFactor;\n"
    "uniform bool u_AlphaTest;\n"
    "uniform float u_AlphaRef;\n"
    "uniform bool u_FogEnabled;\n"
    "uniform vec4 u_FogColor;\n"
    "uniform float u_FogNear;\n"
    "uniform float u_FogFar;\n"
    "\n"
    "out vec4 FragColor;\n"
    "\n"
    "void main() {\n"
    "    vec4 texColor = vec4(1.0);\n"
    "    if (u_UseTexture) {\n"
    "        texColor = texture(u_Texture, v_TexCoord);\n"
    "    }\n"
    "    \n"
    "    vec4 argColor = v_Color;\n"
    "    if (u_TexArg == 1) { // TEXTURE\n"
    "        argColor = vec4(1.0);\n"
    "    } else if (u_TexArg == 2) { // TFACTOR\n"
    "        argColor = u_TextureFactor;\n"
    "    }\n"
    "    \n"
    "    vec4 finalColor = v_Color;\n"
    "    \n"
    "    if (u_UseTexture) {\n"
    "        if (u_ColorOpRgb == 0) finalColor.rgb = texColor.rgb * argColor.rgb;\n"
    "        else if (u_ColorOpRgb == 1) finalColor.rgb = min(texColor.rgb + argColor.rgb, "
    "vec3(1.0));\n"
    "        else if (u_ColorOpRgb == 2) finalColor.rgb = texColor.rgb;\n"
    "        else if (u_ColorOpRgb == 3) finalColor.rgb = argColor.rgb;\n"
    "        \n"
    "        if (u_ColorOpAlpha == 0) finalColor.a = texColor.a * argColor.a;\n"
    "        else if (u_ColorOpAlpha == 1) finalColor.a = min(texColor.a + argColor.a, 1.0);\n"
    "        else if (u_ColorOpAlpha == 2) finalColor.a = texColor.a;\n"
    "        else if (u_ColorOpAlpha == 3) finalColor.a = argColor.a;\n"
    "    } else {\n"
    "        finalColor = argColor;\n"
    "    }\n"
    "    \n"
    "    if (u_AlphaTest && finalColor.a < u_AlphaRef) {\n"
    "        discard;\n"
    "    }\n"
    "    \n"
    "    if (u_FogEnabled) {\n"
    "        float f = (u_FogFar - v_FogFragCoord) / (u_FogFar - u_FogNear);\n"
    "        f = clamp(f, 0.0, 1.0);\n"
    "        finalColor.rgb = mix(u_FogColor.rgb, finalColor.rgb, f);\n"
    "    }\n"
    "    \n"
    "    FragColor = finalColor;\n"
    "}\n";

const char *blitVSSource =
    GLSL_VERSION
    "out vec2 v_TexCoord;\n"
    "void main() {\n"
    "    float x = float((gl_VertexID & 1) << 2) - 1.0;\n"
    "    float y = float((gl_VertexID & 2) << 1) - 1.0;\n"
    "    v_TexCoord = vec2((x + 1.0) * 0.5, (y + 1.0) * 0.5);\n"
    "    gl_Position = vec4(x, y, 0.0, 1.0);\n"
    "}\n";

const char *blitFSSource =
    GLSL_VERSION
    GLSL_PRECISION
    "in vec2 v_TexCoord;\n"
    "uniform sampler2D u_Texture;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    FragColor = texture(u_Texture, v_TexCoord);\n"
    "}\n";
// clang-format on

ZunGraphics *GlesGraphics::Init()
{
    GlesGraphics *gfx = new GlesGraphics;

    SDL_GLContext ctx = SDL_GL_CreateContext(g_GameWindow.window);
    if (!ctx)
    {
        delete gfx;
        Supervisor::DebugPrint("gles renderer create failed: %s\n", SDL_GetError());
        return nullptr;
    }
    gfx->ctx = ctx;

    SDL_GL_MakeCurrent(g_GameWindow.window, ctx);

    glGenFramebuffers(1, &gfx->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, gfx->fbo);

    glGenTextures(1, &gfx->fboColor);
    glBindTexture(GL_TEXTURE_2D, gfx->fboColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gfx->fboColor, 0);

    glGenRenderbuffers(1, &gfx->fboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, gfx->fboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 640, 480);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              gfx->fboDepth);

    glBindFramebuffer(GL_FRAMEBUFFER, gfx->fbo);

    RenderVertexInfo unitQuadData[4] = {{{-128.0f, -128.0f, 0.0f}, {0.0f, 0.0f}},
                                        {{128.0f, -128.0f, 0.0f}, {1.0f, 0.0f}},
                                        {{-128.0f, 128.0f, 0.0f}, {0.0f, 1.0f}},
                                        {{128.0f, 128.0f, 0.0f}, {1.0f, 1.0f}}};

    glGenVertexArrays(1, &gfx->unitQuadVao);
    glGenBuffers(1, &gfx->unitQuadVbo);
    glBindVertexArray(gfx->unitQuadVao);
    glBindBuffer(GL_ARRAY_BUFFER, gfx->unitQuadVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(unitQuadData), unitQuadData, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(RenderVertexInfo),
                          (void *)offsetof(RenderVertexInfo, pos));
    glDisableVertexAttribArray(1);
    glVertexAttrib4f(1, 1.0f, 1.0f, 1.0f, 1.0f);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(RenderVertexInfo),
                          (void *)offsetof(RenderVertexInfo, textureUV));
    glBindVertexArray(0);

    if (!SDL_GL_SetSwapInterval(-1) && !SDL_GL_SetSwapInterval(1))
    {
        // technically this isnt fatal we just go into 60 fps later on in gamewindow::render
        Supervisor::DebugPrint("SDL_GL_SetSwapInterval failed: %s\n", SDL_GetError());
    }

    u32 vertexShader = CompileShader(GL_VERTEX_SHADER, vertexShaderSource);
    u32 fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (vertexShader == 0 || fragmentShader == 0)
    {
        return nullptr;
    }

    gfx->shaderProgram = glCreateProgram();
    glAttachShader(gfx->shaderProgram, vertexShader);
    glAttachShader(gfx->shaderProgram, fragmentShader);
    glLinkProgram(gfx->shaderProgram);
    glUseProgram(gfx->shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    vertexShader = CompileShader(GL_VERTEX_SHADER, blitVSSource);
    fragmentShader = CompileShader(GL_FRAGMENT_SHADER, blitFSSource);
    if (vertexShader == 0 || fragmentShader == 0)
    {
        return nullptr;
    }

    gfx->blitProgram = glCreateProgram();
    glAttachShader(gfx->blitProgram, vertexShader);
    glAttachShader(gfx->blitProgram, fragmentShader);
    glLinkProgram(gfx->blitProgram);
    glUseProgram(gfx->blitProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    gfx->u_Model = glGetUniformLocation(gfx->shaderProgram, "u_Model");
    gfx->u_View = glGetUniformLocation(gfx->shaderProgram, "u_View");
    gfx->u_Proj = glGetUniformLocation(gfx->shaderProgram, "u_Proj");
    gfx->u_TextureMatrix = glGetUniformLocation(gfx->shaderProgram, "u_TextureMatrix");
    gfx->u_ScreenSpace = glGetUniformLocation(gfx->shaderProgram, "u_ScreenSpace");
    gfx->u_Viewport = glGetUniformLocation(gfx->shaderProgram, "u_Viewport");
    gfx->u_UseTexture = glGetUniformLocation(gfx->shaderProgram, "u_UseTexture");
    gfx->u_Texture = glGetUniformLocation(gfx->shaderProgram, "u_Texture");
    gfx->u_ColorOpRgb = glGetUniformLocation(gfx->shaderProgram, "u_ColorOpRgb");
    gfx->u_ColorOpAlpha = glGetUniformLocation(gfx->shaderProgram, "u_ColorOpAlpha");
    gfx->u_TexArg = glGetUniformLocation(gfx->shaderProgram, "u_TexArg");
    gfx->u_TextureFactor = glGetUniformLocation(gfx->shaderProgram, "u_TextureFactor");
    gfx->u_AlphaTest = glGetUniformLocation(gfx->shaderProgram, "u_AlphaTest");
    gfx->u_AlphaRef = glGetUniformLocation(gfx->shaderProgram, "u_AlphaRef");
    gfx->u_FogEnabled = glGetUniformLocation(gfx->shaderProgram, "u_FogEnabled");
    gfx->u_FogColor = glGetUniformLocation(gfx->shaderProgram, "u_FogColor");
    gfx->u_FogNear = glGetUniformLocation(gfx->shaderProgram, "u_FogNear");
    gfx->u_FogFar = glGetUniformLocation(gfx->shaderProgram, "u_FogFar");
    gfx->u_BlitTexture = glGetUniformLocation(gfx->blitProgram, "u_Texture");

    glUseProgram(gfx->shaderProgram);
    glUniform1i(gfx->u_Texture, 0);

    glUseProgram(gfx->blitProgram);
    glUniform1i(gfx->u_BlitTexture, 0);

    glGenVertexArrays(9, &gfx->vaos[0][0]);
    glGenBuffers(3, gfx->vbos);

    for (i32 i = 0; i < 3; i++)
    {
        glBindBuffer(GL_ARRAY_BUFFER, gfx->vbos[i]);
        glBufferData(GL_ARRAY_BUFFER, VBO_CAPACITY, nullptr, GL_DYNAMIC_DRAW);
    }

    for (i32 i = 0; i < 3; i++)
    {
        glBindVertexArray(gfx->vaos[0][i]);
        glBindBuffer(GL_ARRAY_BUFFER, gfx->vbos[i]);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexTex1DiffuseXyzrhw),
                              (void *)offsetof(VertexTex1DiffuseXyzrhw, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VertexTex1DiffuseXyzrhw),
                              (void *)offsetof(VertexTex1DiffuseXyzrhw, color));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexTex1DiffuseXyzrhw),
                              (void *)offsetof(VertexTex1DiffuseXyzrhw, textureUV));

        glBindVertexArray(gfx->vaos[1][i]);
        glBindBuffer(GL_ARRAY_BUFFER, gfx->vbos[i]);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexTex1DiffuseXyz),
                              (void *)offsetof(VertexTex1DiffuseXyz, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VertexTex1DiffuseXyz),
                              (void *)offsetof(VertexTex1DiffuseXyz, diffuse));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexTex1DiffuseXyz),
                              (void *)offsetof(VertexTex1DiffuseXyz, textureUV));

        glBindVertexArray(gfx->vaos[2][i]);
        glBindBuffer(GL_ARRAY_BUFFER, gfx->vbos[i]);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(VertexDiffuseXyzrhw),
                              (void *)offsetof(VertexDiffuseXyzrhw, pos));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VertexDiffuseXyzrhw),
                              (void *)offsetof(VertexDiffuseXyzrhw, diffuse));
        glDisableVertexAttribArray(2);
    }
    glBindVertexArray(0);

    glGenVertexArrays(1, &gfx->blitVao);

    u32 btnVS = CompileShader(GL_VERTEX_SHADER, buttonVSSource);
    u32 btnFS = CompileShader(GL_FRAGMENT_SHADER, buttonFSSource);
    if (btnVS != 0 && btnFS != 0)
    {
        gfx->btnShaderProgram = glCreateProgram();
        glAttachShader(gfx->btnShaderProgram, btnVS);
        glAttachShader(gfx->btnShaderProgram, btnFS);
        glLinkProgram(gfx->btnShaderProgram);
        glDeleteShader(btnVS);
        glDeleteShader(btnFS);
        gfx->btn_u_ScreenSize = glGetUniformLocation(gfx->btnShaderProgram, "u_ScreenSize");
        gfx->btn_u_Tex = glGetUniformLocation(gfx->btnShaderProgram, "u_Tex");
        gfx->btn_u_UseTex = glGetUniformLocation(gfx->btnShaderProgram, "u_UseTex");
    }
    glGenVertexArrays(1, &gfx->btnVao);
    glGenBuffers(1, &gfx->btnVbo);

    for (i32 i = 0; i < 4; i++)
    {
        gfx->transforms[i].Identity();
    }

    Supervisor::DebugPrint("using gles rendering.\n");

    return gfx;
}

void GlesGraphics::Exit()
{
    SDL_GL_DestroyContext(this->ctx);
}

void GlesGraphics::BeginFrame()
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    curVbo = (curVbo + 1) % 3;

    glBindBuffer(GL_ARRAY_BUFFER, vbos[curVbo]);
    glBufferData(GL_ARRAY_BUFFER, VBO_CAPACITY, nullptr, GL_STREAM_DRAW);
    vboOffset = 0;

    stateCache.Invalidate();
}

void GlesGraphics::EndFrame()
{
    Flush();
}

void GlesGraphics::SetFogRange(f32 nearPlane, f32 farPlane)
{
    if (fogNear != nearPlane || fogFar != farPlane)
    {
        fogNear = nearPlane;
        fogFar = farPlane;
        stateCache.dirtyFog = true;
    }
}

void GlesGraphics::SetFogColor(ZunColor color)
{
    if (fogColor.color != color.color)
    {
        fogColor = color;
        stateCache.dirtyFog = true;
    }
}

void GlesGraphics::SetColorOp(TextureOpComponent component, ColorOp op)
{
    if (component == COMPONENT_RGB)
    {
        if (colorOpRgb != op)
        {
            colorOpRgb = op;
            stateCache.dirtyColorOp = true;
        }
    }
    else
    {
        if (colorOpAlpha != op)
        {
            colorOpAlpha = op;
            stateCache.dirtyColorOp = true;
        }
    }
}

void GlesGraphics::SetTextureFactor(ZunColor factor)
{
    if (textureFactor.color != factor.color)
    {
        textureFactor = factor;
        stateCache.dirtyTexFactor = true;
    }
}

void GlesGraphics::SetTextureArg(TextureArg arg)
{
    if (texArg != arg)
    {
        texArg = arg;
        stateCache.dirtyTexArg = true;
    }
}

void GlesGraphics::SetTransformMatrix(TransformMatrix type, const ZunMatrix &matrix)
{
    transforms[type] = matrix;
    stateCache.dirtyMatrix = true;
}

void GlesGraphics::SetTextureFilter()
{
}

void GlesGraphics::GetViewport(ZunViewport &viewport)
{
    viewport = this->viewport;
}

void GlesGraphics::SetViewport(const ZunViewport &viewport)
{
    this->viewport = viewport;
    glViewport(viewport.x, 480 - (viewport.y + viewport.height), viewport.width, viewport.height);
    stateCache.dirtyViewport = true;
}

void GlesGraphics::Enable(Capabilities cap)
{
    switch (cap)
    {
    case CAPS_BLEND:
        if (!blendEnabled)
        {
            glEnable(GL_BLEND);
            blendEnabled = true;
        }
        break;
    case CAPS_DEPTH_TEST:
        if (!depthTestEnabled)
        {
            glEnable(GL_DEPTH_TEST);
            depthTestEnabled = true;
        }
        break;
    case CAPS_ALPHA_TEST:
        if (!alphaTestEnabled)
        {
            alphaTestEnabled = true;
            stateCache.dirtyAlphaTest = true;
        }
        break;
    case CAPS_FOG:
        if (!fogEnabled)
        {
            fogEnabled = true;
            stateCache.dirtyFog = true;
        }
        break;
    }
}

void GlesGraphics::Disable(Capabilities cap)
{
    switch (cap)
    {
    case CAPS_BLEND:
        if (blendEnabled)
        {
            glDisable(GL_BLEND);
            blendEnabled = false;
        }
        break;
    case CAPS_DEPTH_TEST:
        if (depthTestEnabled)
        {
            glDisable(GL_DEPTH_TEST);
            depthTestEnabled = false;
        }
        break;
    case CAPS_ALPHA_TEST:
        if (alphaTestEnabled)
        {
            alphaTestEnabled = false;
            stateCache.dirtyAlphaTest = true;
        }
        break;
    case CAPS_FOG:
        if (fogEnabled)
        {
            fogEnabled = false;
            stateCache.dirtyFog = true;
        }
        break;
    }
}

void GlesGraphics::SetBlendMode(BlendMode srcMode, BlendMode dstMode)
{
    GLenum glSrcMode = GL_SRC_ALPHA;
    switch (srcMode)
    {
    case BLEND_ALPHA:
        glSrcMode = GL_SRC_ALPHA;
        break;
    case BLEND_ONE:
        glSrcMode = GL_ONE;
        break;
    case BLEND_NONE:
        glSrcMode = GL_ONE;
        break;
    }

    GLenum glDstMode = GL_ONE_MINUS_SRC_ALPHA;
    switch (dstMode)
    {
    case BLEND_ALPHA:
        glDstMode = GL_ONE_MINUS_SRC_ALPHA;
        break;
    case BLEND_ONE:
        glDstMode = GL_ONE;
        break;
    case BLEND_NONE:
        glDstMode = GL_ZERO;
        break;
    }
    glBlendFunc(glSrcMode, glDstMode);
}

void GlesGraphics::SetDepthMask(bool enable)
{
    depthMaskEnabled = enable;
    glDepthMask(enable);
}

void GlesGraphics::SetDepthFunc(DepthFunc func)
{
    switch (func)
    {
    case DEPTH_FUNC_LEQUAL:
        glDepthFunc(GL_LEQUAL);
        break;
    case DEPTH_FUNC_ALWAYS:
        glDepthFunc(GL_ALWAYS);
        break;
    }
}

void GlesGraphics::SetClearDepth(f32 depth)
{
#ifdef USING_GL
    glClearDepth(depth);
#else
    glClearDepthf(depth);
#endif
}

void GlesGraphics::SetClearColor(ZunColor color)
{
    clearColor = color;
    glClearColor(color.bytes.r / 255.0f, color.bytes.g / 255.0f, color.bytes.b / 255.0f,
                 color.bytes.a / 255.0f);
}

void GlesGraphics::SetAlphaTestRef(u8 ref)
{
    if (alphaRef != ref)
    {
        alphaRef = ref;
        stateCache.dirtyAlphaTest = true;
    }
}

void GlesGraphics::Clear(u32 clearBits)
{
    GLbitfield bits = 0;
    if (clearBits & CLEAR_COLOR_BUFFER)
    {
        bits |= GL_COLOR_BUFFER_BIT;
    }
    if (clearBits & CLEAR_DEPTH_BUFFER)
    {
        bits |= GL_DEPTH_BUFFER_BIT;
        if (!depthMaskEnabled)
        {
            glDepthMask(GL_TRUE);
        }
    }
    glClear(bits);
    if ((clearBits & CLEAR_DEPTH_BUFFER) && !depthMaskEnabled)
    {
        glDepthMask(GL_FALSE);
    }
}

GfxTextureHandle GlesGraphics::CreateTexture()
{
    GLuint tex;
    glGenTextures(1, &tex);
    return GfxTextureHandle(tex);
}

void GlesGraphics::BindTexture(GfxTextureHandle handle)
{
    glBindTexture(GL_TEXTURE_2D, handle.id);
}

void GlesGraphics::DeleteTexture(GfxTextureHandle handle)
{
    GLuint tex = handle.id;
    glDeleteTextures(1, &tex);
}

void GlesGraphics::SetTextureImage(u32 width, u32 height, PixelFormat fmt, PixelDataType type,
                                   const void *data)
{
    GLenum internalformat;
    GLenum format;
    GLenum datatype;

    switch (fmt)
    {
    case PIXEL_RGB:
        internalformat = GL_RGB8;
        format = GL_RGB;
        break;
    case PIXEL_RGBA:
    default:
        internalformat = GL_RGBA8;
        format = GL_RGBA;
        break;
    }

    switch (type)
    {
    case PIXEL_UNSIGNED_BYTE:
        datatype = GL_UNSIGNED_BYTE;
        break;
    case PIXEL_UNSIGNED_SHORT_5_5_5_1:
        datatype = GL_UNSIGNED_SHORT_5_5_5_1;
        break;
    case PIXEL_UNSIGNED_SHORT_5_6_5:
        datatype = GL_UNSIGNED_SHORT_5_6_5;
        break;
    case PIXEL_UNSIGNED_SHORT_4_4_4_4:
        datatype = GL_UNSIGNED_SHORT_4_4_4_4;
        break;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, datatype, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void GlesGraphics::SetTextureSubImage(i32 xoffset, i32 yoffset, i32 width, i32 height,
                                      const void *data)
{
    glTexSubImage2D(GL_TEXTURE_2D, 0, xoffset, yoffset, width, height, GL_RGBA, GL_UNSIGNED_BYTE,
                    data);
}

void GlesGraphics::ReadPixels(i32 x, i32 y, i32 width, i32 height, void *pixels)
{
    glReadPixels(x, 480 - (y + height), width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    u32 rowSize = width * 4;
    u8 *p = (u8 *)pixels;
    u8 *tempRow = new u8[rowSize];
    for (i32 i = 0; i < height / 2; ++i)
    {
        u8 *top = p + i * rowSize;
        u8 *bottom = p + (height - 1 - i) * rowSize;
        memcpy(tempRow, top, rowSize);
        memcpy(top, bottom, rowSize);
        memcpy(bottom, tempRow, rowSize);
    }
    delete[] tempRow;
}

void GlesGraphics::DrawPrimitive(PrimitiveType type, i32 startVertex, i32 primitiveCount)
{
    i32 vertexCount = 0;
    GLenum glMode = GL_TRIANGLES;

    if (type == PRIM_TRIANGLES)
    {
        vertexCount = primitiveCount * 3;
        glMode = GL_TRIANGLES;
    }
    else if (type == PRIM_TRIANGLE_STRIP)
    {
        vertexCount = primitiveCount + 2;
        glMode = GL_TRIANGLE_STRIP;
    }
    else if (type == PRIM_TRIANGLE_FAN)
    {
        vertexCount = primitiveCount + 2;
        glMode = GL_TRIANGLE_FAN;
    }

    if (stateCache.currentVao != unitQuadVao)
    {
        glBindVertexArray(unitQuadVao);
        stateCache.currentVao = unitQuadVao;
    }

    if (stateCache.currentStride != sizeof(RenderVertexInfo))
    {
        glUniform1i(u_ScreenSpace, false);
        glUniform1i(u_UseTexture, true);
        stateCache.currentStride = sizeof(RenderVertexInfo);
    }

    if (stateCache.dirtyViewport)
    {
        glUniform4f(u_Viewport, (f32)viewport.x, (f32)viewport.y, (f32)viewport.width,
                    (f32)viewport.height);
        stateCache.dirtyViewport = false;
    }

    if (stateCache.dirtyMatrix)
    {
        glUniformMatrix4fv(u_Model, 1, GL_FALSE, (GLfloat *)&transforms[MATRIX_MODEL]);
        glUniformMatrix4fv(u_View, 1, GL_FALSE, (GLfloat *)&transforms[MATRIX_VIEW]);
        glUniformMatrix4fv(u_Proj, 1, GL_FALSE, (GLfloat *)&transforms[MATRIX_PROJECTION]);
        glUniformMatrix4fv(u_TextureMatrix, 1, GL_FALSE, (GLfloat *)&transforms[MATRIX_TEXTURE]);
        stateCache.dirtyMatrix = false;
    }

    if (stateCache.dirtyColorOp)
    {
        glUniform1i(u_ColorOpRgb, colorOpRgb);
        glUniform1i(u_ColorOpAlpha, colorOpAlpha);
        stateCache.dirtyColorOp = false;
    }

    if (stateCache.dirtyTexArg)
    {
        glUniform1i(u_TexArg, texArg);
        stateCache.dirtyTexArg = false;
    }

    if (stateCache.dirtyTexFactor)
    {
        glUniform4f(u_TextureFactor, textureFactor.bytes.r / 255.0f, textureFactor.bytes.g / 255.0f,
                    textureFactor.bytes.b / 255.0f, textureFactor.bytes.a / 255.0f);
        stateCache.dirtyTexFactor = false;
    }

    if (stateCache.dirtyAlphaTest)
    {
        glUniform1i(u_AlphaTest, alphaTestEnabled);
        glUniform1f(u_AlphaRef, alphaRef / 255.0f);
        stateCache.dirtyAlphaTest = false;
    }

    if (stateCache.dirtyFog)
    {
        glUniform1i(u_FogEnabled, fogEnabled);
        glUniform4f(u_FogColor, fogColor.bytes.r / 255.0f, fogColor.bytes.g / 255.0f,
                    fogColor.bytes.b / 255.0f, fogColor.bytes.a / 255.0f);
        glUniform1f(u_FogNear, fogNear);
        glUniform1f(u_FogFar, fogFar);
        stateCache.dirtyFog = false;
    }

    glDrawArrays(glMode, startVertex, vertexCount);
}

void GlesGraphics::DrawPrimitiveUP(PrimitiveType type, i32 primitiveCount, const void *vertexData,
                                   i32 vertexStride)
{
    i32 vertexCount = 0;
    GLenum glMode = GL_TRIANGLES;

    if (type == PRIM_TRIANGLES)
    {
        vertexCount = primitiveCount * 3;
        glMode = GL_TRIANGLES;
    }
    else if (type == PRIM_TRIANGLE_STRIP)
    {
        vertexCount = primitiveCount + 2;
        glMode = GL_TRIANGLE_STRIP;
    }
    else if (type == PRIM_TRIANGLE_FAN)
    {
        vertexCount = primitiveCount + 2;
        glMode = GL_TRIANGLE_FAN;
    }

    GLsizeiptr bytesNeeded = vertexCount * vertexStride;
    vboOffset = ((vboOffset + vertexStride - 1) / vertexStride) * vertexStride;
    GLuint vbo = vbos[curVbo];

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    if (vboOffset + bytesNeeded > VBO_CAPACITY)
    {
        glBufferData(GL_ARRAY_BUFFER, VBO_CAPACITY, nullptr, GL_STREAM_DRAW);
        vboOffset = 0;
    }
    glBufferSubData(GL_ARRAY_BUFFER, vboOffset, bytesNeeded, vertexData);

    GLint firstVertex = (GLint)(vboOffset / vertexStride);

    bool isScreenSpace = false;
    bool hasTex = false;
    GLuint targetVao = 0;
    switch (vertexStride)
    {
    case sizeof(VertexTex1DiffuseXyzrhw):
        isScreenSpace = true;
        hasTex = true;
        targetVao = vaos[0][curVbo];
        break;
    case sizeof(VertexTex1DiffuseXyz):
        isScreenSpace = false;
        hasTex = true;
        targetVao = vaos[1][curVbo];
        break;
    case sizeof(VertexDiffuseXyzrhw):
        isScreenSpace = true;
        hasTex = false;
        targetVao = vaos[2][curVbo];
        break;
    }

    vboOffset += bytesNeeded;

    if (stateCache.currentVao != targetVao)
    {
        glBindVertexArray(targetVao);
        stateCache.currentVao = targetVao;
    }

    if (stateCache.currentStride != vertexStride)
    {
        glUniform1i(u_ScreenSpace, isScreenSpace);
        glUniform1i(u_UseTexture, hasTex);
        stateCache.currentStride = vertexStride;
    }

    if (stateCache.dirtyViewport)
    {
        glUniform4f(u_Viewport, (f32)viewport.x, (f32)viewport.y, (f32)viewport.width,
                    (f32)viewport.height);
        stateCache.dirtyViewport = false;
    }

    if (stateCache.dirtyMatrix)
    {
        glUniformMatrix4fv(u_Model, 1, GL_FALSE, (GLfloat *)&transforms[MATRIX_MODEL]);
        glUniformMatrix4fv(u_View, 1, GL_FALSE, (GLfloat *)&transforms[MATRIX_VIEW]);
        glUniformMatrix4fv(u_Proj, 1, GL_FALSE, (GLfloat *)&transforms[MATRIX_PROJECTION]);
        glUniformMatrix4fv(u_TextureMatrix, 1, GL_FALSE, (GLfloat *)&transforms[MATRIX_TEXTURE]);
        stateCache.dirtyMatrix = false;
    }

    if (stateCache.dirtyColorOp)
    {
        glUniform1i(u_ColorOpRgb, colorOpRgb);
        glUniform1i(u_ColorOpAlpha, colorOpAlpha);
        stateCache.dirtyColorOp = false;
    }

    if (stateCache.dirtyTexArg)
    {
        glUniform1i(u_TexArg, texArg);
        stateCache.dirtyTexArg = false;
    }

    if (stateCache.dirtyTexFactor)
    {
        glUniform4f(u_TextureFactor, textureFactor.bytes.r / 255.0f, textureFactor.bytes.g / 255.0f,
                    textureFactor.bytes.b / 255.0f, textureFactor.bytes.a / 255.0f);
        stateCache.dirtyTexFactor = false;
    }

    if (stateCache.dirtyAlphaTest)
    {
        glUniform1i(u_AlphaTest, alphaTestEnabled);
        glUniform1f(u_AlphaRef, alphaRef / 255.0f);
        stateCache.dirtyAlphaTest = false;
    }

    if (stateCache.dirtyFog)
    {
        glUniform1i(u_FogEnabled, fogEnabled);
        glUniform4f(u_FogColor, fogColor.bytes.r / 255.0f, fogColor.bytes.g / 255.0f,
                    fogColor.bytes.b / 255.0f, fogColor.bytes.a / 255.0f);
        glUniform1f(u_FogNear, fogNear);
        glUniform1f(u_FogFar, fogFar);
        stateCache.dirtyFog = false;
    }

    glDrawArrays(glMode, firstVertex, vertexCount);
}

// ────────────────────────────────────────────────────────────────────────────
// Screen-space virtual button overlay (th06-style circular buttons, no ImGui)
// ────────────────────────────────────────────────────────────────────────────

static TTF_Font *s_btnFont = nullptr;
static GLuint s_btnLabelTex[6] = {0};
static i32 s_btnLabelW[6] = {0};
static i32 s_btnLabelH[6] = {0};
static bool s_btnLabelsReady = false;

static i32 LabelToTexIndex(const char *label)
{
    if (label == nullptr)
    {
        return -1;
    }
    if (label[0] == 'E') return 0; // ESC
    if (label[0] == 'Z') return 1;
    if (label[0] == 'S') return 2;
    if (label[0] == 'X') return 3;
    if (label[0] == '<') return 4;
    if (label[0] == '>') return 5;
    return -1;
}

static void EnsureButtonFont()
{
    if (s_btnLabelsReady)
    {
        return;
    }
    s_btnLabelsReady = true; // try only once

    if (!TTF_Init())
    {
        Supervisor::DebugPrint("btnfont: TTF_Init fail : %s\n", SDL_GetError());
        return;
    }

    s_btnFont = TTF_OpenFont(FileSystem::GetBasePath("msgothic.ttc").c_str(), 48);
    if (!s_btnFont)
    {
        Supervisor::DebugPrint("btnfont: TTF_OpenFont fail : %s\n", SDL_GetError());
        return;
    }
    TTF_SetFontStyle(s_btnFont, TTF_STYLE_BOLD);

    const char *labels[6] = {"ESC", "Z", "S", "X", "<", ">"};
    SDL_Color white = {255, 255, 255, 255};

    for (i32 i = 0; i < 6; i++)
    {
        SDL_Surface *surf = TTF_RenderText_Blended(s_btnFont, labels[i], 0, white);
        if (surf == nullptr)
        {
            continue;
        }

        SDL_Surface *rgba = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
        if (rgba != nullptr)
        {
            SDL_DestroySurface(surf);
            surf = rgba;
        }

        glGenTextures(1, &s_btnLabelTex[i]);
        glBindTexture(GL_TEXTURE_2D, s_btnLabelTex[i]);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, surf->w, surf->h, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, surf->pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        s_btnLabelW[i] = surf->w;
        s_btnLabelH[i] = surf->h;
        SDL_DestroySurface(surf);
    }

    TTF_CloseFont(s_btnFont);
    s_btnFont = nullptr;
}

void GlesGraphics::DrawButtonLabels(const TouchButtons::ButtonInfo *buttons, i32 count,
                                    i32 rw, i32 offsetX, i32 offsetY, i32 scaledH)
{
    EnsureButtonFont();

    glUniform1i(this->btn_u_UseTex, 1);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(this->btn_u_Tex, 0);

    f32 yScale = (f32)scaledH / 480.0f;

    for (i32 i = 0; i < count; i++)
    {
        i32 ti = LabelToTexIndex(buttons[i].label);
        if (ti < 0 || s_btnLabelTex[ti] == 0)
        {
            continue;
        }

        f32 sy = offsetY + (buttons[i].gameY / 480.0f) * scaledH;
        f32 sr = buttons[i].gameRadius * yScale;
        f32 sx;
        if (buttons[i].anchor == TouchButtons::Anchor::RightPillar)
        {
            sx = (f32)(rw) - offsetX + sr;
        }
        else
        {
            sx = (f32)offsetX - sr;
        }

        // Text height ~ 90% of the button radius; keep aspect from the texture.
        f32 th = sr * 0.9f;
        f32 tw = th * (f32)s_btnLabelW[ti] / (f32)s_btnLabelH[ti];

        f32 x0 = sx - tw * 0.5f;
        f32 y0 = sy - th * 0.5f;
        f32 x1 = x0 + tw;
        f32 y1 = y0 + th;

        // pos(2) color(4) uv(2)
        f32 verts[4 * 8];
        f32 *v = verts;
        // top-left, top-right, bottom-left, bottom-right (triangle strip)
        v[0] = x0; v[1] = y0; v[2] = 1; v[3] = 1; v[4] = 1; v[5] = 1; v[6] = 0; v[7] = 0;
        v += 8;
        v[0] = x1; v[1] = y0; v[2] = 1; v[3] = 1; v[4] = 1; v[5] = 1; v[6] = 1; v[7] = 0;
        v += 8;
        v[0] = x0; v[1] = y1; v[2] = 1; v[3] = 1; v[4] = 1; v[5] = 1; v[6] = 0; v[7] = 1;
        v += 8;
        v[0] = x1; v[1] = y1; v[2] = 1; v[3] = 1; v[4] = 1; v[5] = 1; v[6] = 1; v[7] = 1;

        glBindTexture(GL_TEXTURE_2D, s_btnLabelTex[ti]);
        glBufferData(GL_ARRAY_BUFFER, 4 * 8 * sizeof(f32), verts, GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
}

void GlesGraphics::DrawScreenSpaceButtons()
{
    TouchButtons::ButtonInfo buttons[8];
    i32 count = TouchButtons::GetButtonInfo(buttons, 8);
    if (count == 0 || this->btnShaderProgram == 0)
    {
        return;
    }

    i32 rw, rh;
    SDL_GetWindowSizeInPixels(g_GameWindow.window, &rw, &rh);
    if (rw <= 0 || rh <= 0)
    {
        return;
    }

    // Pillarbox layout (same math as SwapBuffers).
    f32 targetAspect = 640.0f / 480.0f;
    f32 windowAspect = (f32)rw / (f32)rh;
    i32 scaledW, scaledH, offsetX, offsetY;
    if (windowAspect > targetAspect)
    {
        scaledH = rh;
        scaledW = (i32)(scaledH * targetAspect);
        offsetX = (rw - scaledW) / 2;
        offsetY = 0;
    }
    else
    {
        scaledW = rw;
        scaledH = (i32)(scaledW / targetAspect);
        offsetX = 0;
        offsetY = (rh - scaledH) / 2;
    }

    // Need at least some black border for the pillarbox buttons.
    if (offsetX < 8)
    {
        return;
    }

    // ---- save GL state ----
    GLint savedViewport[4];
    glGetIntegerv(GL_VIEWPORT, savedViewport);
    GLboolean savedScissorTest = glIsEnabled(GL_SCISSOR_TEST);
    GLint savedScissorBox[4];
    glGetIntegerv(GL_SCISSOR_BOX, savedScissorBox);
    GLboolean savedBlend = glIsEnabled(GL_BLEND);
    GLint savedBlendSrc, savedBlendDst;
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &savedBlendSrc);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &savedBlendDst);
    GLboolean savedDepthTest = glIsEnabled(GL_DEPTH_TEST);
    GLboolean savedCullFace = glIsEnabled(GL_CULL_FACE);
    GLboolean savedDepthMask;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &savedDepthMask);
    GLint savedProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, &savedProgram);
    GLint savedVao;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &savedVao);
    GLint savedActiveTexture;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &savedActiveTexture);
    GLint savedTexture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &savedTexture);

    // ---- setup ----
    glViewport(0, 0, rw, rh);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    glUseProgram(this->btnShaderProgram);
    glUniform2f(this->btn_u_ScreenSize, (f32)rw, (f32)rh);
    glUniform1i(this->btn_u_UseTex, 0);

    glBindVertexArray(this->btnVao);
    glBindBuffer(GL_ARRAY_BUFFER, this->btnVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (const void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (const void *)(2 * sizeof(f32)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (const void *)(6 * sizeof(f32)));

    constexpr i32 kHalfSegs = 16;
    constexpr f32 kBorderW = 2.0f;
    constexpr f32 kPi = 3.14159265358979323846f;
    f32 yScale = (f32)scaledH / 480.0f;
    f32 verts[33 * 2 * 8]; // border ring max: (32+1)*2 verts

    for (i32 i = 0; i < count; i++)
    {
        f32 sy = offsetY + (buttons[i].gameY / 480.0f) * scaledH;
        f32 sr = buttons[i].gameRadius * yScale;
        f32 sx;
        if (buttons[i].anchor == TouchButtons::Anchor::RightPillar)
        {
            sx = (f32)(rw - offsetX) + sr;
            if (sx > (f32)rw - sr)
            {
                sx = (f32)rw - sr;
            }
        }
        else
        {
            sx = (f32)offsetX - sr;
            if (sx < sr)
            {
                sx = sr;
            }
        }

        // Filled circle (horizontal band triangle strip).
        {
            i32 nv = (kHalfSegs + 1) * 2;
            for (i32 j = 0; j <= kHalfSegs; j++)
            {
                f32 ang = kPi * 0.5f - j * kPi / (f32)kHalfSegs;
                f32 ca = cosf(ang), sa = sinf(ang);
                f32 *L = verts + (j * 2) * 8;
                L[0] = sx - sr * ca; L[1] = sy - sr * sa;
                L[2] = buttons[i].fillR; L[3] = buttons[i].fillG;
                L[4] = buttons[i].fillB; L[5] = buttons[i].fillA;
                L[6] = 0; L[7] = 0;
                f32 *R = verts + (j * 2 + 1) * 8;
                R[0] = sx + sr * ca; R[1] = sy - sr * sa;
                R[2] = buttons[i].fillR; R[3] = buttons[i].fillG;
                R[4] = buttons[i].fillB; R[5] = buttons[i].fillA;
                R[6] = 0; R[7] = 0;
            }
            glBufferData(GL_ARRAY_BUFFER, nv * 8 * sizeof(f32), verts, GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, nv);
        }

        // Border ring.
        {
            i32 segs = kHalfSegs * 2;
            i32 nv = (segs + 1) * 2;
            f32 innerR = sr - kBorderW;
            if (innerR < 1.0f)
            {
                innerR = 1.0f;
            }
            for (i32 j = 0; j <= segs; j++)
            {
                f32 ang = j * 2.0f * kPi / (f32)segs;
                f32 ca = cosf(ang), sa = sinf(ang);
                f32 *O = verts + (j * 2) * 8;
                O[0] = sx + sr * ca; O[1] = sy + sr * sa;
                O[2] = buttons[i].borderR; O[3] = buttons[i].borderG;
                O[4] = buttons[i].borderB; O[5] = buttons[i].borderA;
                O[6] = 0; O[7] = 0;
                f32 *I = verts + (j * 2 + 1) * 8;
                I[0] = sx + innerR * ca; I[1] = sy + innerR * sa;
                I[2] = buttons[i].borderR; I[3] = buttons[i].borderG;
                I[4] = buttons[i].borderB; I[5] = buttons[i].borderA;
                I[6] = 0; I[7] = 0;
            }
            glBufferData(GL_ARRAY_BUFFER, nv * 8 * sizeof(f32), verts, GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, nv);
        }
    }

    DrawButtonLabels(buttons, count, rw, offsetX, offsetY, scaledH);

    // ---- restore GL state ----
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glUseProgram(savedProgram);
    glActiveTexture(savedActiveTexture);
    glBindTexture(GL_TEXTURE_2D, savedTexture);
    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
    if (savedScissorTest)
    {
        glEnable(GL_SCISSOR_TEST);
    }
    else
    {
        glDisable(GL_SCISSOR_TEST);
    }
    glScissor(savedScissorBox[0], savedScissorBox[1], savedScissorBox[2], savedScissorBox[3]);
    if (savedBlend)
    {
        glEnable(GL_BLEND);
    }
    else
    {
        glDisable(GL_BLEND);
    }
    glBlendFunc(savedBlendSrc, savedBlendDst);
    if (savedDepthTest)
    {
        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
    if (savedCullFace)
    {
        glEnable(GL_CULL_FACE);
    }
    else
    {
        glDisable(GL_CULL_FACE);
    }
    glDepthMask(savedDepthMask);
}

void GlesGraphics::SwapBuffers()
{
    i32 drawableWidth, drawableHeight;
    SDL_GetWindowSizeInPixels(g_GameWindow.window, &drawableWidth, &drawableHeight);

#if defined(__APPLE__) && TARGET_OS_IPHONE
    SDL_PropertiesID props = SDL_GetWindowProperties(g_GameWindow.window);
    this->defaultFbo = (GLuint)SDL_GetNumberProperty(
        props, SDL_PROP_WINDOW_UIKIT_OPENGL_FRAMEBUFFER_NUMBER, this->defaultFbo);
#endif

    glBindFramebuffer(GL_READ_FRAMEBUFFER, this->fbo);
#ifndef USING_GL
    const GLenum attachments[] = {GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT};
    glInvalidateFramebuffer(GL_READ_FRAMEBUFFER, 2, attachments);
#endif

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->defaultFbo);

    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, drawableWidth, drawableHeight);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // the original game didnt pillarbox but that looks really ugly so im pillarboxing anyways
    f32 targetAspect = 640.0f / 480.0f;
    f32 windowAspect = (f32)drawableWidth / (f32)drawableHeight;

    i32 dstWidth;
    i32 dstHeight;
    i32 dstX;
    i32 dstY;

    if (windowAspect > targetAspect)
    {
        dstHeight = drawableHeight;
        dstWidth = (i32)(dstHeight * targetAspect);
        dstX = (drawableWidth - dstWidth) / 2;
        dstY = 0;
    }
    else
    {
        dstWidth = drawableWidth;
        dstHeight = (i32)(dstWidth / targetAspect);
        dstX = 0;
        dstY = (drawableHeight - dstHeight) / 2;
    }

    glViewport(dstX, dstY, dstWidth, dstHeight);

    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    glUseProgram(this->blitProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, this->fboColor);

    glBindVertexArray(this->blitVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    DrawScreenSpaceButtons();

#if defined(__APPLE__) && TARGET_OS_IPHONE
    glBindRenderbuffer(
        GL_RENDERBUFFER,
        SDL_GetNumberProperty(props, SDL_PROP_WINDOW_UIKIT_OPENGL_RENDERBUFFER_NUMBER, 0));
#endif
    SDL_GL_SwapWindow(g_GameWindow.window);

    glBindFramebuffer(GL_FRAMEBUFFER, this->fbo);
    glViewport(viewport.x, 480 - (viewport.y + viewport.height), viewport.width, viewport.height);

    if (blendEnabled)
    {
        glEnable(GL_BLEND);
    }
    else
    {
        glDisable(GL_BLEND);
    }
    if (depthTestEnabled)
    {
        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(depthMaskEnabled ? GL_TRUE : GL_FALSE);

    glClearColor(clearColor.bytes.r / 255.0f, clearColor.bytes.g / 255.0f,
                 clearColor.bytes.b / 255.0f, clearColor.bytes.a / 255.0f);

    glUseProgram(this->shaderProgram);
    stateCache.Invalidate();
}
