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

#define L_WINDOW_CLASS_NAME L"window-class"

char *VS = "\n"
    "#version 330 core\n"
    "in vec2 pos;\n"
    "\n"
    "void main()\n"
    "{\n"
    "   gl_Position = vec4(pos.x, pos.y, 0.0f, 1.0f);\n"
    "}\n"
    "\n";

char *FS = "\n"
    "#version 330 core\n"
    "out vec4 color;\n"
    "\n"
    "void main()\n"
    "{\n"
    "   color = vec4(0.5f, 0.5f, 0.2f, 1.0f);\n"
    "}\n"
    "\n";

float vertices[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.0f,  0.5f,
};
float PointSize = 12.0f;
float DPI = 96.0f;
int32_t WindowWidth = 800;
int32_t WindowHeight = 600;

struct glyph_metrics
{
    float OffsetX;
    float OffsetY;
    float Advance;
    float XYW;
    float XYH;
    float UVW;
    float UVH;
};

struct dwrite_font
{
    IDWriteFontFace *Font;
    GLuint Texture;
    glyph_metrics *Metrics;
    int32_t GlyphCount;
};

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

    // NOTE(Oskar): VBO
    GLuint VBO;
    {
        glCreateBuffers(1, &VBO);
        glNamedBufferStorage(VBO, sizeof(vertices), vertices, 0);
    }

    // NOTE(Oskar): VAO
    GLuint VAO;
    {
        glCreateVertexArrays(1, &VAO);

        GLint BufferIndex = 0;
        glVertexArrayVertexBuffer(VAO, BufferIndex, VBO, 0, 2 * sizeof(float));

        GLint Pos = 0;
        glVertexArrayAttribFormat(VAO, Pos, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(VAO, Pos, BufferIndex);
        glEnableVertexArrayAttrib(VAO, Pos);
    }

    // NOTE(Oskar): Shaders
    GLuint Pipeline;
    GLuint VShader;
    GLuint FShader;
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
    }

    static wchar_t FontPath[] = L"C:\\Windows\\Fonts\\arial.ttf";
    dwrite_font Font = {};
    {
        COLORREF BackColor = RGB(0, 0, 0);
        COLORREF ForeColor = RGB(255, 255, 255);

        HRESULT Error = 0;

        // Create factory
        IDWriteFactory *Factory = 0;
        Error = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&Factory);
        DeferRelease(Factory);
        CheckPointer(Error, Factory, Assert(!"Factory"));

        // Read font file
        IDWriteFontFile *FontFile = 0;
        Error = Factory->CreateFontFileReference(FontPath, 0, &FontFile);
        DeferRelease(FontFile);
        CheckPointer(Error, FontFile, Assert(!"FontFile"));

        // Create font face
        Error = Factory->CreateFontFace(DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &FontFile, 0, DWRITE_FONT_SIMULATIONS_NONE, &Font.Font);
        CheckPointer(Error, Font.Font, Assert(!"Font Face"));

        // Font rendering params
        IDWriteRenderingParams *DefaultRenderingParams = 0;
        Error = Factory->CreateRenderingParams(&DefaultRenderingParams);
        DeferRelease(DefaultRenderingParams);
        CheckPointer(Error, DefaultRenderingParams, Assert(!"Default Rendering Params"));

        FLOAT Gamma = 1.0f;

        // Custom rendering params
        IDWriteRenderingParams *RenderingParams = 0;
        Error = Factory->CreateCustomRenderingParams(Gamma,
                                                     DefaultRenderingParams->GetEnhancedContrast(),
                                                     DefaultRenderingParams->GetClearTypeLevel(),
                                                     DefaultRenderingParams->GetPixelGeometry(),
                                                     DWRITE_RENDERING_MODE_NATURAL,
                                                     &RenderingParams);
        DeferRelease(RenderingParams);
        CheckPointer(Error, RenderingParams, Assert(!"Rendering Params"));

        // GDI
        IDWriteGdiInterop *DWriteGDIInterop = 0;
        Error = Factory->GetGdiInterop(&DWriteGDIInterop);
        DeferRelease(DWriteGDIInterop);
        CheckPointer(Error, DWriteGDIInterop, Assert(!"GDI Interop"));

        // Get metrics
        DWRITE_FONT_METRICS FontMetrics = {};
        Font.Font->GetMetrics(&FontMetrics);

        float PixelPerEM = PointSize * (1.0f / 72.0f) * DPI;
        float PixelPerDesignUnit = PixelPerEM / ((float)FontMetrics.designUnitsPerEm);

        int32_t RasterTargetWidth  = (int32_t)(8.0f*((float)FontMetrics.capHeight)*PixelPerDesignUnit);
        int32_t RasterTargetHeight = (int32_t)(8.0f*((float)FontMetrics.capHeight)*PixelPerDesignUnit);
        float RasterTargetX = (float)(RasterTargetWidth / 2);
        float RasterTargetY = (float)(RasterTargetHeight / 2);

        Assert((float) ((int)(RasterTargetX)) == RasterTargetX);
        Assert((float) ((int)(RasterTargetY)) == RasterTargetY);

        // Get glyph count
        Font.GlyphCount = Font.Font->GetGlyphCount();

        // Render target
        IDWriteBitmapRenderTarget *RenderTarget = 0;
        Error = DWriteGDIInterop->CreateBitmapRenderTarget(0, RasterTargetWidth, RasterTargetHeight, &RenderTarget);
        HDC DC = RenderTarget->GetMemoryDC();

        // Clear
        {
            HGDIOBJ Original = SelectObject(DC, GetStockObject(DC_PEN));
            SetDCPenColor(DC, BackColor);
            SelectObject(DC, GetStockObject(DC_BRUSH));
            SetDCBrushColor(DC, BackColor);
            Rectangle(DC, 0, 0, RasterTargetWidth, RasterTargetHeight);
            SelectObject(DC, Original);
        }

        // Allocate Atlas
        int32_t AtlasWidth  = 4 * (int32_t)(((float)FontMetrics.capHeight)*PixelPerDesignUnit);
        int32_t AtlasHeight = 4 * (int32_t)(((float)FontMetrics.capHeight)*PixelPerDesignUnit);
        if (AtlasWidth < 16)
        {
            AtlasWidth = 16;
        }
        else
        {
            AtlasWidth = NextPowerOfTwo(AtlasWidth);
        }

        if (AtlasHeight < 256)
        {
            AtlasWidth = 256;
        }
        else
        {
            AtlasHeight = NextPowerOfTwo(AtlasHeight);
        }

        int32_t AtlasCount = (Font.GlyphCount + 3) / 4;
        int32_t AtlasSliceSize = AtlasWidth * AtlasHeight * 3;
        int32_t AtlasMemorySize = AtlasSliceSize * AtlasCount;
        uint8_t *AtlasMemory = (uint8_t *)malloc(AtlasMemorySize);
        memset(AtlasMemory, 0, AtlasMemorySize);

        // Allocate metrics
        Font.Metrics = (glyph_metrics *)malloc(sizeof(glyph_metrics) * Font.GlyphCount);
        memset(Font.Metrics, 0, sizeof(glyph_metrics) * Font.GlyphCount);

        // Populate atlas and metrics
        for (uint16_t GlyphIndex = 0; GlyphIndex < Font.GlyphCount; ++GlyphIndex)
        {
            // Render glyph into target
            DWRITE_GLYPH_RUN GlyphRun = {};
            GlyphRun.fontFace = Font.Font;
            GlyphRun.fontEmSize = PixelPerEM;
            GlyphRun.glyphCount = 1;
            GlyphRun.glyphIndices = &GlyphIndex;
            RECT BoundingBox = {0};
            Error = RenderTarget->DrawGlyphRun(RasterTargetX, RasterTargetY,
                                                DWRITE_MEASURING_MODE_NATURAL, 
                                                &GlyphRun, RenderingParams,
                                                RGB(255, 255, 255), &BoundingBox);
            Check(Error, continue);

            Assert(0 <= BoundingBox.left);
            Assert(0 <= BoundingBox.top);
            Assert(BoundingBox.right <= RasterTargetWidth);
            Assert(BoundingBox.bottom <= RasterTargetHeight);

            // Compute glyph metrics
            DWRITE_GLYPH_METRICS GlyphMetrics = {};
            Error = Font.Font->GetDesignGlyphMetrics(&GlyphIndex, 1, &GlyphMetrics, false);
            Check(Error, continue);

            int32_t TextureWidth = BoundingBox.right - BoundingBox.left;
            int32_t TextureHeight = BoundingBox.bottom - BoundingBox.top;

            Font.Metrics[GlyphIndex].OffsetX = (float)BoundingBox.left - RasterTargetX;
            Font.Metrics[GlyphIndex].OffsetY = (float)BoundingBox.top  - RasterTargetY;
            Font.Metrics[GlyphIndex].Advance = (float)RoundUp(((float)GlyphMetrics.advanceWidth) * PixelPerDesignUnit);
            Font.Metrics[GlyphIndex].XYW     = TextureWidth;
            Font.Metrics[GlyphIndex].XYH     = TextureHeight;
            Font.Metrics[GlyphIndex].UVW     = (float)TextureWidth / (float)AtlasWidth;
            Font.Metrics[GlyphIndex].UVH     = (float)TextureHeight / (float)AtlasHeight;
            
            // Get Bitmap
            HBITMAP Bitmap = (HBITMAP)GetCurrentObject(DC, OBJ_BITMAP);
            DIBSECTION DIB = {};
            GetObject(Bitmap, sizeof(DIB), &DIB);

            // Blit bitmap to atlas
            int32_t XSliceOffset = (3 * AtlasWidth / 2) * (GlyphIndex & 1);
            int32_t YSliceOffset = (3 * AtlasWidth * AtlasHeight / 2) * ((GlyphIndex & 2) >> 1);
            uint8_t *AtlasSlice = AtlasMemory + AtlasSliceSize * (GlyphIndex / 4) + XSliceOffset + YSliceOffset;
            {
                Assert(DIB.dsBm.bmBitsPixel == 32);

                int32_t InPitch = DIB.dsBm.bmWidthBytes;
                int32_t OutPitch = AtlasWidth * 3;
                uint8_t *InLine = (uint8_t *)DIB.dsBm.bmBits + BoundingBox.left * 4 + BoundingBox.top * InPitch;
                uint8_t *OutLine = AtlasSlice;
                for (int32_t Y = 0; Y < TextureHeight; ++Y)
                {
                    uint8_t *InPixel = InLine;
                    uint8_t *OutPixel = OutLine;
                    for (int32_t X = 0; X < TextureWidth; ++X)
                    {
                        OutPixel[0] = InPixel[2];
                        OutPixel[1] = InPixel[1];
                        OutPixel[2] = InPixel[0];
                        InPixel += 4;
                        OutPixel += 3;
                    }
                    InLine += InPitch;
                    OutLine += OutPitch;
                }
            }

            // Clear render target
            {
                HGDIOBJ Original = SelectObject(DC, GetStockObject(DC_PEN));
                SetDCPenColor(DC, BackColor);
                SelectObject(DC, GetStockObject(DC_BRUSH));
                SetDCBrushColor(DC, BackColor);
                Rectangle(DC,
                          BoundingBox.left, BoundingBox.top,
                          BoundingBox.right, BoundingBox.bottom);
                SelectObject(DC, Original);
            }
        }

        // Create GPU atlas out of the generated CPU atlas.
        glGenTextures(1, &Font.Texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, Font.Texture);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB, AtlasWidth, AtlasHeight, AtlasCount, 0, GL_RGB, GL_UNSIGNED_BYTE, AtlasMemory);

        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        free(AtlasMemory);
        AtlasMemory = 0;
    }

    BOOL VSYNC = TRUE;
    wglSwapIntervalEXT(VSYNC ? 1 : 0);
    ShowWindow(Window, SW_SHOWDEFAULT);

    for (;;)
    {
        MSG Message = {0};
        while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, Framebuffer);

        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // NOTE(Oskar): Use shaders
        glBindProgramPipeline(Pipeline);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, Framebuffer);
        glBlitFramebuffer(0, 0, WindowWidth, WindowHeight, 0, 0, WindowWidth, WindowHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        SwapBuffers(DeviceContext);
    }

    return 0;
}