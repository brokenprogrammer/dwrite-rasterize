// make sure you use functions that are valid for selected GL version (specified when context is created)
#define GL_FUNCTIONS(X) \
    X(PFNGLCREATEBUFFERSPROC,            glCreateBuffers            ) \
    X(PFNGLNAMEDBUFFERSTORAGEPROC,       glNamedBufferStorage       ) \
    X(PFNGLBINDVERTEXARRAYPROC,          glBindVertexArray          ) \
    X(PFNGLCREATEVERTEXARRAYSPROC,       glCreateVertexArrays       ) \
    X(PFNGLVERTEXARRAYATTRIBBINDINGPROC, glVertexArrayAttribBinding ) \
    X(PFNGLVERTEXARRAYVERTEXBUFFERPROC,  glVertexArrayVertexBuffer  ) \
    X(PFNGLVERTEXARRAYATTRIBFORMATPROC,  glVertexArrayAttribFormat  ) \
    X(PFNGLENABLEVERTEXARRAYATTRIBPROC,  glEnableVertexArrayAttrib  ) \
    X(PFNGLCREATESHADERPROGRAMVPROC,     glCreateShaderProgramv     ) \
    X(PFNGLGETPROGRAMIVPROC,             glGetProgramiv             ) \
    X(PFNGLGETPROGRAMINFOLOGPROC,        glGetProgramInfoLog        ) \
    X(PFNGLGENPROGRAMPIPELINESPROC,      glGenProgramPipelines      ) \
    X(PFNGLUSEPROGRAMSTAGESPROC,         glUseProgramStages         ) \
    X(PFNGLBINDPROGRAMPIPELINEPROC,      glBindProgramPipeline      ) \
    X(PFNGLPROGRAMUNIFORMMATRIX2FVPROC,  glProgramUniformMatrix2fv  ) \
    X(PFNGLBINDTEXTUREUNITPROC,          glBindTextureUnit          ) \
    X(PFNGLCREATETEXTURESPROC,           glCreateTextures           ) \
    X(PFNGLTEXTUREPARAMETERIPROC,        glTextureParameteri        ) \
    X(PFNGLTEXTURESTORAGE2DPROC,         glTextureStorage2D         ) \
    X(PFNGLTEXTURESUBIMAGE2DPROC,        glTextureSubImage2D        ) \
    X(PFNGLDEBUGMESSAGECALLBACKPROC,     glDebugMessageCallback     ) \
    X(PFNGLCREATESHADERPROC,             glCreateShader             ) \
    X(PFNGLSHADERSOURCEPROC,             glShaderSource             ) \
    X(PFNGLCOMPILESHADERPROC,            glCompileShader            ) \
    X(PFNGLCREATEPROGRAMPROC,            glCreateProgram            ) \
    X(PFNGLATTACHSHADERPROC,             glAttachShader             ) \
    X(PFNGLLINKPROGRAMPROC,              glLinkProgram              ) \
    X(PFNGLDELETESHADERPROC,             glDeleteShader             )\
    X(PFNGLGENVERTEXARRAYSPROC,          glGenVertexArrays          ) \
    X(PFNGLGENBUFFERSPROC,               glGenBuffers               ) \
    X(PFNGLBINDBUFFERPROC,               glBindBuffer               ) \
    X(PFNGLBUFFERDATAPROC,               glBufferData               ) \
    X(PFNGLVERTEXATTRIBPOINTERPROC,      glVertexAttribPointer      ) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC,  glEnableVertexAttribArray  ) \
    X(PFNGLUSEPROGRAMPROC,               glUseProgram               ) \
    X(PFNGLDELETEVERTEXARRAYSPROC,       glDeleteVertexArrays       ) \
    X(PFNGLDELETEBUFFERSPROC,            glDeleteBuffers            ) \
    X(PFNGLDELETEPROGRAMPROC,            glDeleteProgram            ) \
    X(PFNGLUNIFORM1FPROC,                glUniform1f                ) \
    X(PFNGLGETUNIFORMLOCATIONPROC,       glGetUniformLocation       )

#define X(type, name) static type name;
GL_FUNCTIONS(X)
#undef X

static PFNWGLCHOOSEPIXELFORMATARBPROC       wglChoosePixelFormatARB = NULL;
static PFNWGLCREATECONTEXTATTRIBSARBPROC    wglCreateContextAttribsARB = NULL;
static PFNWGLSWAPINTERVALEXTPROC            wglSwapIntervalEXT = NULL;

static void APIENTRY DebugCallback(
    GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar* message, const void* user)
{
    OutputDebugStringA(message);
    OutputDebugStringA("\n");
    if (severity == GL_DEBUG_SEVERITY_HIGH || severity == GL_DEBUG_SEVERITY_MEDIUM)
    {
        if (IsDebuggerPresent())
        {
            Assert(!"OpenGL error - check the callstack in debugger");
        }
        FatalError("OpenGL API usage error! Use debugger to examine call stack!");
    }
}

static HWND
Win32OpenGLCreateDummyWindow()
{
    // To get WGL functions we need valid GL context, so create dummy window for dummy GL contetx
    HWND DummyContext = CreateWindowExW(
        0,                              // Extended Window Style
        L"STATIC",                      // Class Name
        L"DummyWindow",                 // Window Name
        WS_OVERLAPPED,                  // Window Style
        CW_USEDEFAULT, CW_USEDEFAULT,   // X, Y
        CW_USEDEFAULT, CW_USEDEFAULT,   // Width, Height
        NULL, NULL, NULL, NULL);        // Parent, Menu, Related HInstance, LPParam 
    Assert(DummyContext && "Failed to create DummyContext window");

    return (DummyContext);
}

static void
Win32DestroyOpenGLDummyWindow(HWND WindowHandle, HDC DeviceContext, HGLRC OpenGLRenderingContext)
{
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(OpenGLRenderingContext);
    ReleaseDC(WindowHandle, DeviceContext);
    DestroyWindow(WindowHandle);
}

static void
_Win32OpenGLGetWGLFunctions(HDC DeviceContext)
{
    wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

    if (!wglChoosePixelFormatARB || !wglCreateContextAttribsARB || !wglSwapIntervalEXT)
    {
        FatalError("OpenGL does not support required WGL extensions for modern context!");
    }
}

static void 
Win32OpenGLGetWGLFunctions(void)
{
    HWND DummyWindowContext = Win32OpenGLCreateDummyWindow();
    HDC DeviceContext = GetDC(DummyWindowContext);
    Assert(DeviceContext && "Failed to get device context for dummy window");

    PIXELFORMATDESCRIPTOR PDF =
    {
        sizeof(PDF),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        24,
        0, 0, 0, 0, 0, 0,
        0, 0,
        0, 0, 0, 0, 0,
        24,
        8,
        0,
        PFD_MAIN_PLANE,
        0, 0, 0, 0      
    };

    int PixelFormat = ChoosePixelFormat(DeviceContext, &PDF);
    if (!PixelFormat)
    {
        FatalError("Cannot choose OpenGL pixel format for dummy window!");
    }

    int Ok = DescribePixelFormat(DeviceContext, PixelFormat, sizeof(PDF), &PDF);
    Assert(Ok && "Failed to describe OpenGL pixel format");

    if (!SetPixelFormat(DeviceContext, PixelFormat, &PDF))
    {
        FatalError("Cannot set OpenGL pixel format for dummy window!");
    }

    HGLRC RenderContext = wglCreateContext(DeviceContext);
    Assert(RenderContext && "Failed to create OpenGL context for dummy window");

    Ok = wglMakeCurrent(DeviceContext, RenderContext);
    Assert(Ok && "Failed to make current OpenGL context for dummy window");

    _Win32OpenGLGetWGLFunctions(DeviceContext);
   
    Win32DestroyOpenGLDummyWindow(DummyWindowContext, DeviceContext, RenderContext);
}

static HGLRC
Win32InitializeOpenGLContext(HDC DeviceContext)
{
    HGLRC RenderContext;

    // Set pixel format for OpenGL context
    {
        int Attributes[] =
        {
            WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
            WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
            WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
            WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB,     24,
            WGL_DEPTH_BITS_ARB,     24,
            WGL_STENCIL_BITS_ARB,   8,
            0,
        };

        int Format;
        UINT Formats;
        if (!wglChoosePixelFormatARB(DeviceContext, Attributes, NULL, 1, &Format, &Formats) || Formats == 0)
        {
            FatalError("OpenGL does not support required pixel format!");
        }

        PIXELFORMATDESCRIPTOR PDF = { sizeof(PDF) };
        int Ok = DescribePixelFormat(DeviceContext, Format, sizeof(PDF), &PDF);
        Assert(Ok && "Failed to describe OpenGL pixel format");

        if (!SetPixelFormat(DeviceContext, Format, &PDF))
        {
            FatalError("Cannot set OpenGL selected pixel format!");
        }
    }

    // Create modern OpenGL context
    {
        int Attributes[] =
        {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
            WGL_CONTEXT_MINOR_VERSION_ARB, 5,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
            0,
        };

        RenderContext = wglCreateContextAttribsARB(DeviceContext, NULL, Attributes);
        if (!RenderContext)
        {
            FatalError("Cannot create modern OpenGL context! OpenGL version 4.5 not supported?");
        }

        BOOL Ok = wglMakeCurrent(DeviceContext, RenderContext);
        Assert(Ok && "Failed to make current OpenGL context");

        // X Macro to load OpenGL functions
#define X(type, name) name = (type)wglGetProcAddress(#name); Assert(name);
        GL_FUNCTIONS(X)
#undef X

        // Enable debug callback
        glDebugMessageCallback(&DebugCallback, NULL);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    }

    return (RenderContext);
}