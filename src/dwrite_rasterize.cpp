#define UNICODE
#include <Windows.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#include <gl/gl.h>
#include "../ext/wglext.h"
#include "../ext/glext.h"

#define STN_USE_STRING
#include "../ext/stn.h"

#include "dwrite_util.cpp"
#include "dwrite_opengl.cpp"

#define L_WINDOW_CLASS_NAME L"window-class"

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
        SwapBuffers(DeviceContext);
    }

    return 0;
}