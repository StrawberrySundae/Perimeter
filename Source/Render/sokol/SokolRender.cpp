#include "StdAfxRD.h"
#include "xmath.h"
#include "Umath.h"
#include "sokol_gfx.h"
#include "SokolResources.h"
#include "IRenderDevice.h"
#include "SokolRender.h"
#include "SokolRenderPipeline.h"
#include "xerrhand.h"
#include "DrawBuffer.h"
#include "SokolShaders.h"
#include "RenderTracker.h"

cSokolRender::cSokolRender() = default;

cSokolRender::~cSokolRender() {
    Done();
};

int cSokolRender::Init(int xScr, int yScr, int mode, void* wnd, int RefreshRateInHz) {
    RenderSubmitEvent(RenderEvent::INIT, "Sokol start");
    int res = cInterfaceRenderDevice::Init(xScr, yScr, mode, wnd, RefreshRateInHz);
    if (res != 0) {
        return res;
    }

    ClearCommands();
    ClearPipelines();
    
    sdlWindow = static_cast<SDL_Window*>(wnd);
    
    //Init some state
    activePipelineType = PIPELINE_TYPE_TRIANGLE;
    activePipelineBlend = ALPHA_NONE;
    activePipelineCull = CULL_CCW;
    
    //Set some attributes before context creation
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    //Use OpenGL 3.2 Core
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    // Create an OpenGL context associated with the window.
    sdlGlContext = SDL_GL_CreateContext(sdlWindow);
    if (sdlGlContext == nullptr) {
        ErrH.Abort("Error creating SDL GL Context", XERR_CRITICAL, 0, SDL_GetError());
    }
    
    //Init sokol gfx
    {
        sg_desc desc = {};
        desc.pipeline_pool_size = PIPELINE_ID_MAX,
#ifdef PERIMETER_SOKOL_SHARE_SHADERS
        desc.shader_pool_size = VERTEX_FMT_MAX,
#else
        desc.shader_pool_size = desc.pipeline_pool_size,
#endif
        desc.buffer_pool_size = 2048 * 2; //2048 is enough for PGW+PET game
        desc.image_pool_size = 1024 * 4; //1024 is enough for PGW+PET game
        desc.context.color_format = SG_PIXELFORMAT_RGBA8;
        sg_setup(&desc);
    }
    
    //Create empty texture
    sg_image_desc desc = {};
    desc.label = "EmptySlotTexture";
    desc.width = desc.height = 64;
    desc.wrap_u = desc.wrap_v = SG_WRAP_REPEAT;
    desc.pixel_format = SG_PIXELFORMAT_RGBA8;
    desc.num_mipmaps = 1;
    desc.min_filter = desc.mag_filter = SG_FILTER_NEAREST;
    desc.usage = SG_USAGE_IMMUTABLE;
    size_t pixel_len = sokol_pixelformat_bytesize(desc.pixel_format);
    size_t buf_len = desc.height * desc.width * pixel_len;
    uint8_t* buf = new uint8_t[buf_len];
    memset(buf, 0xFF, buf_len);
    desc.data.subimage[0][0] = { buf, buf_len };
    emptyTexture = new SokolTexture2D(desc);
    
    //Create test texture
    desc.label = "TestTexture";
    for (int i = 0; i < buf_len; i += pixel_len * 4) {
        *reinterpret_cast<uint32_t*>(&buf[i]) = 0xFF0000FF;
        *reinterpret_cast<uint32_t*>(&buf[i+pixel_len*2]) = 0xFF00FF00;
        *reinterpret_cast<uint32_t*>(&buf[i+pixel_len*3]) = 0xFFFF0000;
        *reinterpret_cast<uint32_t*>(&buf[i+pixel_len*4]) = 0xFFAAAAAA;
    }
    desc.data.subimage[0][0] = { buf, buf_len };
    testTexture = new SokolTexture2D(desc);
    delete[] buf;

    //Register a pipeline for each vertex format in game
    RegisterPipelines();

    RenderSubmitEvent(RenderEvent::INIT, "Sokol done");
    return UpdateRenderMode();
}

bool cSokolRender::ChangeSize(int xScr, int yScr, int mode) {
    int mode_mask = RENDERDEVICE_MODE_WINDOW;

    if (xScr == ScreenSize.x && yScr == ScreenSize.y && (RenderMode&mode_mask) == mode) {
        return true; //Nothing to do
    }

    ScreenSize.x = xScr;
    ScreenSize.y = yScr;
    RenderMode &= ~mode_mask;
    RenderMode |= mode;
    
    return UpdateRenderMode() == 0;
}

int cSokolRender::UpdateRenderMode() {
    RenderSubmitEvent(RenderEvent::UPDATE_MODE);
    orthoVP = Mat4f::ID;
    SetOrthographic(orthoVP, ScreenSize.x, -ScreenSize.y, -10, 10);
    return 0;
}

int cSokolRender::Done() {
    RenderSubmitEvent(RenderEvent::DONE, "Sokol start");
    int ret = cInterfaceRenderDevice::Done();
    ClearCommands();
    ClearPipelines();
#ifdef PERIMETER_SOKOL_SHARE_SHADERS
    shaders.clear();
#endif
    delete emptyTexture;
    emptyTexture = nullptr;
    delete testTexture;
    testTexture = nullptr;
    sg_shutdown();
    if (sdlGlContext != nullptr) {
        SDL_GL_DeleteContext(sdlGlContext);
        sdlGlContext = nullptr;
    }
    RenderSubmitEvent(RenderEvent::DONE, "Sokol done");
    return ret;
}

int cSokolRender::SetGamma(float fGamma, float fStart, float fFinish) {
    //TODO
    printf("cSokolRender::SetGamma not implemented!\n");
    return -1;
}

void cSokolRender::CreateVertexBuffer(VertexBuffer& vb, uint32_t NumberVertex, vertex_fmt_t fmt, bool dynamic) {
    RenderSubmitEvent(RenderEvent::CREATE_VERTEXBUF,
                      "Len: " + std::to_string(NumberVertex)
                      + " Fmt: " + std::to_string(fmt)
                      + " Dyn: " + std::to_string(dynamic),
                      &vb);
    xassert(!vb.sg);
    xassert(NumberVertex <= std::numeric_limits<indices_t>().max());
    size_t size = GetSizeFromFormat(fmt);

    sg_buffer_desc desc = {};
    desc.size = NumberVertex * size;
    desc.type = SG_BUFFERTYPE_VERTEXBUFFER;
    desc.usage = dynamic ? SG_USAGE_STREAM : SG_USAGE_DYNAMIC;
    desc.label = "CreateVertexBuffer";

    vb.VertexSize = size;
    vb.fmt = fmt;
    vb.dynamic = dynamic;
    vb.NumberVertex = NumberVertex;
    vb.sg = new SokolBuffer(desc);
}

void cSokolRender::DeleteVertexBuffer(VertexBuffer &vb) {
    RenderSubmitEvent(RenderEvent::DELETE_VERTEXBUF, "", &vb);
    delete vb.sg;
    vb.sg = nullptr;
}

void* cSokolRender::LockVertexBuffer(VertexBuffer &vb) {
    RenderSubmitEvent(RenderEvent::LOCK_VERTEXBUF, "", &vb);
    if (!vb.sg) {
        xassert(0);
        return nullptr;
    }
    xassert(!vb.sg->locked);
    vb.sg->dirty = true;
    vb.sg->locked = true;
    return vb.sg->data;
}

void* cSokolRender::LockVertexBuffer(VertexBuffer &vb, uint32_t Start, uint32_t Amount) {
    RenderSubmitEvent(RenderEvent::LOCK_VERTEXBUF, "Idx: " + std::to_string(Start) + " Len: " + std::to_string(Amount) , &vb);
    xassert(Start + Amount <= vb.NumberVertex);
    return &static_cast<uint8_t*>(LockVertexBuffer(vb))[vb.VertexSize * Start];
}

void cSokolRender::UnlockVertexBuffer(VertexBuffer &vb) {
    RenderSubmitEvent(RenderEvent::UNLOCK_VERTEXBUF, "", &vb);
    xassert(vb.sg->locked);
    vb.sg->locked = false;
}

void cSokolRender::CreateIndexBuffer(IndexBuffer& ib, uint32_t NumberIndices, bool dynamic) {
    RenderSubmitEvent(RenderEvent::CREATE_INDEXBUF,
                      "Len: " + std::to_string(NumberIndices)
                      + " Dyn: " + std::to_string(dynamic),
                      &ib);
    xassert(!ib.sg);
    ib.NumberIndices = NumberIndices;
    ib.dynamic = dynamic;
    
    sg_buffer_desc desc = {};
    desc.size = ib.NumberIndices * sizeof(indices_t);
    desc.type = SG_BUFFERTYPE_INDEXBUFFER;
    desc.usage = dynamic ? SG_USAGE_STREAM : SG_USAGE_DYNAMIC;
    desc.label = "CreateIndexBuffer";
    ib.sg = new SokolBuffer(desc);
}

void cSokolRender::DeleteIndexBuffer(IndexBuffer &ib) {
    RenderSubmitEvent(RenderEvent::DELETE_INDEXBUF, "", &ib);
    delete ib.sg;
    ib.sg = nullptr;
}

indices_t* cSokolRender::LockIndexBuffer(IndexBuffer &ib) {
    RenderSubmitEvent(RenderEvent::LOCK_INDEXBUF, "", &ib);
    if (!ib.sg) {
        xassert(0);
        return nullptr;
    }
    xassert(!ib.sg->locked);
    ib.sg->dirty = true;
    ib.sg->locked = true;
    return static_cast<indices_t*>(ib.sg->data);
}

indices_t* cSokolRender::LockIndexBuffer(IndexBuffer &ib, uint32_t Start, uint32_t Amount) {
    RenderSubmitEvent(RenderEvent::LOCK_INDEXBUF, "Idx: " + std::to_string(Start) + " Len: " + std::to_string(Amount) , &ib);
    xassert(Start + Amount <= ib.NumberIndices);
    return &LockIndexBuffer(ib)[Start];
}

void cSokolRender::UnlockIndexBuffer(IndexBuffer &ib) {
    RenderSubmitEvent(RenderEvent::UNLOCK_INDEXBUF, "", &ib);
    xassert(ib.sg->locked);
    ib.sg->locked = false;
}

void cSokolRender::ClearCommands() {
    for (SokolCommand* command : commands) {
        delete command;
    }
    commands.clear();
}

void cSokolRender::ClearPipelines() {
    for (auto pipeline : pipelines) {
        delete pipeline;
    }
    pipelines.clear();
}

int cSokolRender::GetClipRect(int *xmin,int *ymin,int *xmax,int *ymax) {
    *xmin=viewportPos.x; *xmax=viewportSize.x + viewportPos.x;
    *ymin=viewportPos.y; *ymax=viewportSize.y + viewportPos.y;
    return 0;
}

int cSokolRender::SetClipRect(int xmin,int ymin,int xmax,int ymax) {
    viewportPos.x = xmin;
    viewportPos.y = ymin;
    viewportSize.x = xmax-xmin;
    viewportSize.y = ymax-ymin;
    return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////

SokolCommand::SokolCommand() {
    Clear();
}

SokolCommand::~SokolCommand() {
    Clear();
}

void SokolCommand::ClearDrawData() {
    if (owned_buffers) {
        delete vertex_buffer;
        delete index_buffer;
        owned_buffers = false;
    }
    vertex_buffer = nullptr;
    index_buffer = nullptr;
    vertices = 0;
    indices = 0;
    
}

void SokolCommand::ClearMVP() {
    if (owned_mvp) {
        delete vs_mvp;
        owned_mvp = false;
    }
    vs_mvp = nullptr;
}

void SokolCommand::Clear() {
    ClearDrawData();
    ClearMVP();
    memset(textures, 0, PERIMETER_SOKOL_TEXTURES * sizeof(SokolTexture2D*));
}
