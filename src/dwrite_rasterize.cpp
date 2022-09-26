#define UNICODE
#include <Windows.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <intrin.h>

#include <gl/gl.h>
#include "../ext/wglext.h"
#include "../ext/glext.h"

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
    "   color = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
    "}\n"
    "\n";

float vertices[] = {
    -0.5f, -0.5f,
     0.5f, -0.5f,
     0.0f,  0.5f,
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

    HWND Window = CreateWindowExW(
        WS_EX_APPWINDOW, 
        WindowClass.lpszClassName, 
        L"OpenGL Window", 
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 
        CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, WindowClass.hInstance, NULL);
    Assert(Window && "Failed to create window");

    HDC DeviceContext = GetDC(Window);
    Assert(DeviceContext && "Failed to window device context");

    HGLRC RenderContext = Win32InitializeOpenGLContext(DeviceContext);

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

        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        // NOTE(Oskar): Use shaders
        glBindProgramPipeline(Pipeline);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);   

        SwapBuffers(DeviceContext);
    }

    return 0;
}