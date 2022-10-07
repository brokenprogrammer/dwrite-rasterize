#define UNICODE
#include <Windows.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <intrin.h>

#include <gl/gl.h>
#include "../ext/wglext.h"
#include "../ext/glext.h"

#include <dwrite_1.h>

#define STN_USE_STRING
#include "../ext/stn.h"

#include "dwrite_util.cpp"
#include "dwrite_opengl.cpp"

#include "dwrite_text.h"
#include "dwrite_text.cpp"

#define L_WINDOW_CLASS_NAME L"window-class"

char *VS = "\n"
    "#version 330 core\n"
    "in vec2 pos;\n"
    "in vec2 tex_position;\n"
    "\n"
    "out vec2 uv;\n"
    "\n"
    "uniform mat3 pixel_to_normal;\n"
    "\n"
    "void main()\n"
    "{\n"
    "   gl_Position.xy = (pixel_to_normal * vec3(pos, 1.f)).xy;\n"
    "   gl_Position.z  = 0.f;\n"
    "   gl_Position.w  = 1.f;\n"
    "   uv = tex_position;\n"
    "}\n"
    "\n";

char *FS = "\n"
    "#version 330 core\n"
    "in vec2 uv;\n"
    "\n"
    "out vec4 mask;\n"
    "\n"
    "uniform sampler2D tex;\n"
    "uniform float mask_value_table[7];\n"
    "\n"
    "void main()\n"
    "{\n"
    "   vec3 S = texture(tex, uv).rgb;\n"
    "   int C0 = int(S.r*6 + 0.1); // + 0.1 just incase we have some small rounding taking us below the integer we should be hitting.\n"
    "   int C1 = int(S.g*6 + 0.1);\n"
    "   int C2 = int(S.b*6 + 0.1);\n"
    "   mask.rgb = vec3(mask_value_table[C0], \n"
    "       mask_value_table[C1],\n"
    "       mask_value_table[C2]);\n"
    "   mask.a = 1;\n"
    "}\n"
    "\n";

float vertices[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.0f,  0.5f,
};
float PointSize = 48.0f;
float DPI = 96.0f;
int32_t WindowWidth = 800;
int32_t WindowHeight = 600;

GLuint PixelToNormalLocation;
GLuint UniformTextureLocation;
GLuint UniformMaskValueTableLocation;

GLuint AttribPosition;
GLuint AttribTexturePosition;

GLuint VShader;
GLuint FShader;

// NOTE(Oskar): Thanks to: https://nullprogram.com/blog/2017/10/06/#what-is-utf-8
void *
utf8_decode(void *buf, uint32_t *c, int *e)
{
    static const char lengths[] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2, 2, 3, 3, 4, 0
    };
    static const int masks[]  = {0x00, 0x7f, 0x1f, 0x0f, 0x07};
    static const uint32_t mins[] = {4194304, 0, 128, 2048, 65536};
    static const int shiftc[] = {0, 18, 12, 6, 0};
    static const int shifte[] = {0, 6, 4, 2, 0};

    unsigned char *s = (unsigned char *)buf;
    int len = lengths[s[0] >> 3];

    /* Compute the pointer to the next character early so that the next
     * iteration can start working on the next character. Neither Clang
     * nor GCC figure out this reordering on their own.
     */
    unsigned char *next = s + len + !len;

    /* Assume a four-byte character and load four bytes. Unused bits are
     * shifted out.
     */
    *c  = (uint32_t)(s[0] & masks[len]) << 18;
    *c |= (uint32_t)(s[1] & 0x3f) << 12;
    *c |= (uint32_t)(s[2] & 0x3f) <<  6;
    *c |= (uint32_t)(s[3] & 0x3f) <<  0;
    *c >>= shiftc[len];

    /* Accumulate the various error conditions. */
    *e  = (*c < mins[len]) << 6;
    *e |= ((*c >> 11) == 0x1b) << 7;  // surrogate half?
    *e |= (s[1] & 0xc0) >> 2;
    *e |= (s[2] & 0xc0) >> 4;
    *e |= (s[3]       ) >> 6;
    *e ^= 0x2a; // top two bits of each tail byte correct?
    *e >>= shifte[len];

    return next;
}

void RenderString(dwrite_font Font, char *Text, int32_t X, int32_t Y, float R, float G, float B, float Alpha, float Scale)
{
    int32_t Length = StringLength(Text);
    uint16_t *Indices = (uint16_t*)malloc(sizeof(uint16_t)*Length);

    char *TextPointer = Text;
    int32_t PointerIndex = 0;
    int32_t PointerLength = 0;
    while (StringLength(TextPointer) > 0)
    {
        // TODO(Oskar): Check error code.
        uint32_t Codepoint;
        int Error;
        TextPointer = (char *)utf8_decode(TextPointer, &Codepoint, &Error);
        Font.Font->GetGlyphIndices(&Codepoint, 1, &Indices[PointerIndex++]);
        PointerLength++;
    }
    Length = PointerLength;

    int32_t FloatPerVertex = 4;
    int32_t BytePerVertex = FloatPerVertex*sizeof(float);
    int32_t VertexPerCharacter = 6;
    int32_t TotalFloatCount = Length*VertexPerCharacter*FloatPerVertex;
    float *Vertices = (float*)malloc(sizeof(float)*TotalFloatCount);

    {
        float LayoutX = (float)X;
        float LayoutY = (float)Y;

        float *Vertex = Vertices;
        for (int32_t I = 0; I < Length; ++I)
        {
            uint16_t Index = Indices[I];
            Assert(Index < Font.GlyphCount);

            glyph_metrics Metrics = Font.Metrics[Index];
            float UVX = Metrics.UVX;
            float UVY = Metrics.UVY;
            for (int32_t J = 0; J < VertexPerCharacter; ++J)
            {
                float GX = LayoutX + (Metrics.OffsetX * Scale);
                float GY = LayoutY + (Metrics.OffsetY * Scale);

                switch (J)
                {
                    case 0:
                    {
                        Vertex[0] = GX;
                        Vertex[1] = GY;
                        Vertex[2] = UVX;
                        Vertex[3] = UVY;
                    } break;

                    case 1:
                    case 3:
                    {
                        Vertex[0] = GX;
                        Vertex[1] = GY + (Metrics.XYH * Scale);
                        Vertex[2] = UVX;
                        Vertex[3] = UVY + Metrics.UVH;
                    } break;

                    case 2:
                    case 4:
                    {   
                        Vertex[0] = GX + (Metrics.XYW * Scale);
                        Vertex[1] = GY;
                        Vertex[2] = UVX + Metrics.UVW;
                        Vertex[3] = UVY;
                    } break;

                    case 5:
                    {
                        Vertex[0] = GX + (Metrics.XYW * Scale);
                        Vertex[1] = GY + (Metrics.XYH * Scale);
                        Vertex[2] = UVX + Metrics.UVW;
                        Vertex[3] = UVY + Metrics.UVH;
                    } break;
                }
                Vertex += FloatPerVertex;
            }

            LayoutX += (Metrics.Advance * Scale);
        }
    }

    float V = R * 0.5f + G + B * 0.1875f;
    float MaskValueTable[7];

    static float CMaxTable[] = {
        0.f,
        0.380392157f,
        0.600000000f,
        0.749019608f,
        0.854901961f,
        0.937254902f,
        1.f,
    };
    static float CMinTable[] = {
        0.f,
        0.166666667f,
        0.333333333f,
        0.500000000f,
        0.666666667f,
        0.833333333f,
        1.f,
    };

    static float A = 0.839215686374509f; // 214/255
    static float B1 = 1.266666666666667f; // 323/255
    float L = (V - A)/(B1 - A);

    MaskValueTable[0] = 0.0f;
    for (int32_t Index = 1; Index <= 5; ++Index)
    {
        float CMax = CMaxTable[Index];
        float CMin = CMinTable[Index];
        float M = CMax + (CMin - CMax)*L;
        if (M > CMax){
            M = CMax;
        }
        if (M < CMin){
            M = CMin;
        }
        MaskValueTable[Index] = M*Alpha;
    }
    MaskValueTable[6]= Alpha;

    float Matrix[9];
    Matrix[0] = 2.f/(float)WindowWidth;  Matrix[3] = 0.f;                        Matrix[6] = -1.f;
    Matrix[1] = 0.f;                     Matrix[4] = -2.f/(float)WindowHeight;   Matrix[7] =  1.f,
    Matrix[2] = 0.f;                     Matrix[5] = 0.f;                        Matrix[8] =  1.f,

    // Perform drawing
    glBlendColor(R, G, B, Alpha);
    glBufferData(GL_ARRAY_BUFFER, TotalFloatCount * sizeof(float), Vertices, GL_DYNAMIC_DRAW);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, Font.Texture);
    glProgramUniform1i(FShader, UniformTextureLocation, 0);
    glProgramUniform1fv(FShader, UniformMaskValueTableLocation, 7, MaskValueTable);
    glProgramUniformMatrix3fv(VShader, PixelToNormalLocation, 1, GL_FALSE, Matrix);
    glVertexAttribPointer(AttribPosition, 2, GL_FLOAT, GL_FALSE, BytePerVertex, 0);
    glVertexAttribPointer(AttribTexturePosition, 2, GL_FLOAT, GL_FALSE, BytePerVertex, (void*)(sizeof(float)*2));
    glDrawArrays(GL_TRIANGLES, 0, VertexPerCharacter*Length);

    free(Vertices);
    free(Indices);
}

LRESULT 
Win32WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    switch (Message)
    {
        case WM_CREATE: break;
        
        case WM_CLOSE:
        case WM_DESTROY:
        {
            ExitProcess(0);
        } break;
        
        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }
    return(Result);
}

int 
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    SetProcessDPIAware();
    Win32OpenGLGetWGLFunctions();

    WNDCLASSEXW WindowClass =
    {
        sizeof(WindowClass),
        0,
        Win32WindowProc,
        0, 0,
        Instance,
        LoadIcon(NULL, IDI_APPLICATION),
        LoadCursor(NULL, IDC_ARROW),
        0,
        0,
        L"opengl_window_class",
        0
    };
    ATOM Atom = RegisterClassExW(&WindowClass);
    Assert(Atom && "Failed to register window class");

    RECT WindowRectangle = { 0, 0, WindowWidth, WindowHeight };
    AdjustWindowRect(&WindowRectangle, WS_OVERLAPPED, FALSE);

    HWND Window = CreateWindowExW(
        WS_EX_APPWINDOW, 
        WindowClass.lpszClassName, 
        L"OpenGL Window", 
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        WindowRectangle.right  - WindowRectangle.left, 
        WindowRectangle.bottom - WindowRectangle.top,
        NULL, NULL, WindowClass.hInstance, NULL);
    Assert(Window && "Failed to create window");

    HDC DeviceContext = GetDC(Window);
    Assert(DeviceContext && "Failed to window device context");

    HGLRC RenderContext = Win32InitializeOpenGLContext(DeviceContext);

    // NOTE(Oskar): sRGB Framebuffer
    GLuint Framebuffer = 0;
    {
        glEnable(GL_FRAMEBUFFER_SRGB);
        glEnable(GL_BLEND);
        glBlendFunc(GL_CONSTANT_COLOR, GL_ONE_MINUS_SRC_COLOR);

        GLuint FrameTexture = 0;
        glGenTextures(1, &FrameTexture);
        glBindTexture(GL_TEXTURE_2D, FrameTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8, WindowWidth, WindowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);

        glGenFramebuffers(1, &Framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, Framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, FrameTexture, 0);
        GLenum Status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        Assert(Status == GL_FRAMEBUFFER_COMPLETE);
    }

    // NOTE(Oskar): Shaders
    GLuint Pipeline;
    {
        VShader = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &VS);
        FShader = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &FS);

        GLint Linked;
        glGetProgramiv(VShader, GL_LINK_STATUS, &Linked);
        if (!Linked)
        {
            char Message[1024];
            glGetProgramInfoLog(VShader, sizeof(Message), NULL, Message);
            OutputDebugStringA(Message);
            Assert(!"Failed to create vertex shader!");
        }

        glGetProgramiv(FShader, GL_LINK_STATUS, &Linked);
        if (!Linked)
        {
            char Message[1024];
            glGetProgramInfoLog(FShader, sizeof(Message), NULL, Message);
            OutputDebugStringA(Message);
            Assert(!"Failed to create fragment shader!");
        }

        glGenProgramPipelines(1, &Pipeline);
        glUseProgramStages(Pipeline, GL_VERTEX_SHADER_BIT, VShader);
        glUseProgramStages(Pipeline, GL_FRAGMENT_SHADER_BIT, FShader);

        glBindProgramPipeline(Pipeline);

        PixelToNormalLocation         = glGetUniformLocation(VShader, "pixel_to_normal");
        UniformTextureLocation        = glGetUniformLocation(FShader, "tex");
        UniformMaskValueTableLocation = glGetUniformLocation(FShader, "mask_value_table");

        AttribPosition        = glGetAttribLocation(VShader, "pos");
        AttribTexturePosition = glGetAttribLocation(VShader, "tex_position");
    }

    // NOTE(Oskar): VAO & VBO
    GLuint VAO;
    GLuint VBO;
    {
        glCreateBuffers(1, &VBO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        
        glCreateVertexArrays(1, &VAO);
        glBindVertexArray(VAO);

        glEnableVertexAttribArray(AttribPosition);
        glEnableVertexAttribArray(AttribTexturePosition);
    }

    static wchar_t FontPath[] = L"C:\\Windows\\Fonts\\arial.ttf";
    dwrite_font Font = BakeDWriteFont(FontPath, PointSize, DPI);
    
    BOOL VSYNC = TRUE;
    wglSwapIntervalEXT(VSYNC ? 1 : 0);
    ShowWindow(Window, SW_SHOWDEFAULT);
    
    glViewport(0, 0, WindowWidth, WindowHeight);

    for (;;)
    {
        MSG Message = {0};
        while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, Framebuffer);

        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        RenderString(Font, "Example Text.", 100, 100, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);

        RenderString(Font, "Example Text.", 100, 160, 1.0f, 1.0f, 1.0f, 1.0f, 0.7f);

        RenderString(Font, "Example Text.", 100, 220, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f);

        RenderString(Font, "Example Text.", 100, 280, 1.0f, 1.0f, 1.0f, 1.0f, 0.3f);   

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, Framebuffer);
        glBlitFramebuffer(0, 0, WindowWidth, WindowHeight, 0, 0, WindowWidth, WindowHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        SwapBuffers(DeviceContext);
    }

    return 0;
}