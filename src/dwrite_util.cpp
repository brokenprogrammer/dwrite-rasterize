struct win32_interface_releaser
{
    IUnknown *Pointer;

    win32_interface_releaser(IUnknown *Pointer)
    {
        this->Pointer = Pointer;
    }

    ~win32_interface_releaser()
    {
        if (Pointer != 0)
        {
            this->Pointer->Release();
        }
    }
};
#define DeferRelease(Pointer) win32_interface_releaser Pointer##Releaser(Pointer)
#define CheckPointer(Error, Pointer, Result) if ((Pointer) == 0 || (Error) != S_OK) { Error = S_OK; Result; }
#define Check(Error, Result) if ((Error) != S_OK) { Error = S_OK; Result; }

#define Assert(cond) do { if (!(cond)) __debugbreak(); } while (0)

static void FatalError(const char* message)
{
    MessageBoxA(NULL, message, "Error", MB_ICONEXCLAMATION);
    ExitProcess(0);
}

static int StringsAreEqual(const char* src, const char* dst, size_t dstlen)
{
    while (*src && dstlen-- && *dst)
    {
        if (*src++ != *dst++)
        {
            return 0;
        }
    }

    return (dstlen && *src == *dst) || (!dstlen && *src == 0);
}

uint32_t
NextPowerOfTwo(uint32_t X)
{
    if (X == 0)
    {
        return 1;
    }
    else
    {
        X -= 1;
        X |= X >> 1;
        X |= X >> 2;
        X |= X >> 4;
        X |= X >> 8;
        X |= X >> 16;
        X += 1;
        return X;
    }
}

int32_t
RoundUp(float X)
{
    int32_t K = (int32_t)X;
    if ((float)K < X)
    {
        K += 1;
    }

    return K;
}