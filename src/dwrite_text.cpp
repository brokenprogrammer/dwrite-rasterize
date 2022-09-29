void
ClearDC(HDC DC, COLORREF Color, uint32_t L, uint32_t T, uint32_t R, uint32_t B)
{
    HGDIOBJ Original = SelectObject(DC, GetStockObject(DC_PEN));
    SetDCPenColor(DC, Color);
    SelectObject(DC, GetStockObject(DC_BRUSH));
    SetDCBrushColor(DC, Color);
    Rectangle(DC, L, T, R, B);
    SelectObject(DC, Original);
}

font_atlas
BuildFontAtlas(dwrite_font *Font, dwrite_state *State)
{
    font_atlas Atlas = {};

    // Allocate Atlas
    Atlas.Width  = 4 * (int32_t)(((float)State->FontMetrics.capHeight)*State->PixelPerDesignUnit);
    Atlas.Height = 4 * (int32_t)(((float)State->FontMetrics.capHeight)*State->PixelPerDesignUnit);
    if (Atlas.Width < 16)
    {
        Atlas.Width = 16;
    }
    else
    {
        Atlas.Width = NextPowerOfTwo(Atlas.Width);
    }

    if (Atlas.Height < 256)
    {
        Atlas.Width = 256;
    }
    else
    {
        Atlas.Height = NextPowerOfTwo(Atlas.Height);
    }

    Atlas.Count = (Font->GlyphCount + 3) / 4;
    int32_t AtlasSliceSize = Atlas.Width * Atlas.Height * 3;
    int32_t AtlasMemorySize = AtlasSliceSize * Atlas.Count;
    Atlas.Memory = (uint8_t *)malloc(AtlasMemorySize);
    memset(Atlas.Memory, 0, AtlasMemorySize);

    // Allocate metrics
    Font->Metrics = (glyph_metrics *)malloc(sizeof(glyph_metrics) * Font->GlyphCount);
    memset(Font->Metrics, 0, sizeof(glyph_metrics) * Font->GlyphCount);

    // Populate atlas and metrics
    for (uint16_t GlyphIndex = 0; GlyphIndex < Font->GlyphCount; ++GlyphIndex)
    {
        // Render glyph into target
        DWRITE_GLYPH_RUN GlyphRun = {};
        GlyphRun.fontFace = Font->Font;
        GlyphRun.fontEmSize = State->PixelPerEM;
        GlyphRun.glyphCount = 1;
        GlyphRun.glyphIndices = &GlyphIndex;
        RECT BoundingBox = {0};
        HRESULT Error = State->RenderTarget->DrawGlyphRun(State->RasterTargetX, State->RasterTargetY,
                                                          DWRITE_MEASURING_MODE_NATURAL, 
                                                          &GlyphRun, State->RenderingParams,
                                                          RGB(255, 255, 255), &BoundingBox);
        Check(Error, continue);

        Assert(0 <= BoundingBox.left);
        Assert(0 <= BoundingBox.top);
        Assert(BoundingBox.right  <= State->RasterTargetWidth);
        Assert(BoundingBox.bottom <= State->RasterTargetHeight);

        // Compute glyph metrics
        DWRITE_GLYPH_METRICS GlyphMetrics = {};
        Error = Font->Font->GetDesignGlyphMetrics(&GlyphIndex, 1, &GlyphMetrics, false);
        Check(Error, continue);

        int32_t TextureWidth = BoundingBox.right - BoundingBox.left;
        int32_t TextureHeight = BoundingBox.bottom - BoundingBox.top;

        Font->Metrics[GlyphIndex].OffsetX = (float)BoundingBox.left - State->RasterTargetX;
        Font->Metrics[GlyphIndex].OffsetY = (float)BoundingBox.top  - State->RasterTargetY;
        Font->Metrics[GlyphIndex].Advance = (float)RoundUp(((float)GlyphMetrics.advanceWidth) * State->PixelPerDesignUnit);
        Font->Metrics[GlyphIndex].XYW     = TextureWidth;
        Font->Metrics[GlyphIndex].XYH     = TextureHeight;
        Font->Metrics[GlyphIndex].UVW     = (float)TextureWidth / (float)Atlas.Width;
        Font->Metrics[GlyphIndex].UVH     = (float)TextureHeight / (float)Atlas.Height;
        
        // Get Bitmap
        HBITMAP Bitmap = (HBITMAP)GetCurrentObject(State->DC, OBJ_BITMAP);
        DIBSECTION DIB = {};
        GetObject(Bitmap, sizeof(DIB), &DIB);

        // Blit bitmap to atlas
        int32_t XSliceOffset = (3 * Atlas.Width / 2) * (GlyphIndex & 1);
        int32_t YSliceOffset = (3 * Atlas.Width * Atlas.Height / 2) * ((GlyphIndex & 2) >> 1);
        uint8_t *AtlasSlice = Atlas.Memory + AtlasSliceSize * (GlyphIndex / 4) + XSliceOffset + YSliceOffset;
        {
            Assert(DIB.dsBm.bmBitsPixel == 32);

            int32_t InPitch = DIB.dsBm.bmWidthBytes;
            int32_t OutPitch = Atlas.Width * 3;
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
        ClearDC(State->DC, BackColor, BoundingBox.left, BoundingBox.top, BoundingBox.right, BoundingBox.bottom); 
    }

    return Atlas;
}

dwrite_state
DWriteStateCreate(wchar_t *FontPath, float PointSize, float DPI, dwrite_font *Font)
{
    dwrite_state State = {};

    HRESULT Error = 0;
    // Create factory
    State.Factory = 0;
    Error = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&State.Factory);
    //DeferRelease(Factory);
    CheckPointer(Error, State.Factory, Assert(!"Factory"));

    // Read font file
    State.FontFile = 0;
    Error = State.Factory->CreateFontFileReference(FontPath, 0, &State.FontFile);
    // DeferRelease(State.FontFile);
    CheckPointer(Error, State.FontFile, Assert(!"FontFile"));

    // Create font face
    Error = State.Factory->CreateFontFace(DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &State.FontFile, 0, DWRITE_FONT_SIMULATIONS_NONE, &Font->Font);
    CheckPointer(Error, Font->Font, Assert(!"Font Face"));

    // Font rendering params
    State.DefaultRenderingParams = 0;
    Error = State.Factory->CreateRenderingParams(&State.DefaultRenderingParams);
    // DeferRelease(DefaultRenderingParams);
    CheckPointer(Error, State.DefaultRenderingParams, Assert(!"Default Rendering Params"));

    FLOAT Gamma = 1.0f;

    // Custom rendering params
    State.RenderingParams = 0;
    Error = State.Factory->CreateCustomRenderingParams(Gamma,
                                                       State.DefaultRenderingParams->GetEnhancedContrast(),
                                                       State.DefaultRenderingParams->GetClearTypeLevel(),
                                                       State.DefaultRenderingParams->GetPixelGeometry(),
                                                       DWRITE_RENDERING_MODE_NATURAL,
                                                       &State.RenderingParams);
    // DeferRelease(RenderingParams);
    CheckPointer(Error, State.RenderingParams, Assert(!"Rendering Params"));

    // GDI
    State.DWriteGDIInterop = 0;
    Error = State.Factory->GetGdiInterop(&State.DWriteGDIInterop);
    // DeferRelease(DWriteGDIInterop);
    CheckPointer(Error, State.DWriteGDIInterop, Assert(!"GDI Interop"));

    // Get metrics
    State.FontMetrics = {};
    Font->Font->GetMetrics(&State.FontMetrics);

    State.PixelPerEM = PointSize * (1.0f / 72.0f) * DPI;
    State.PixelPerDesignUnit = State.PixelPerEM / ((float)State.FontMetrics.designUnitsPerEm);

    State.RasterTargetWidth  = (int32_t)(8.0f*((float)State.FontMetrics.capHeight)*State.PixelPerDesignUnit);
    State.RasterTargetHeight = (int32_t)(8.0f*((float)State.FontMetrics.capHeight)*State.PixelPerDesignUnit);
    State.RasterTargetX = (float)(State.RasterTargetWidth / 2);
    State.RasterTargetY = (float)(State.RasterTargetHeight / 2);

    Assert((float) ((int)(State.RasterTargetX)) == State.RasterTargetX);
    Assert((float) ((int)(State.RasterTargetY)) == State.RasterTargetY);

    // Get glyph count
    Font->GlyphCount = Font->Font->GetGlyphCount();

    // Render target
    State.RenderTarget = 0;
    Error = State.DWriteGDIInterop->CreateBitmapRenderTarget(0, State.RasterTargetWidth, State.RasterTargetHeight, &State.RenderTarget);
    State.DC = State.RenderTarget->GetMemoryDC();

    return State;
}

void
DWriteStateDestroy(dwrite_state *State)
{
    State->Factory->Release();
    State->FontFile->Release();
    State->DefaultRenderingParams->Release();
    State->RenderingParams->Release();
    State->DWriteGDIInterop->Release();
}

dwrite_font
BakeDWriteFont(wchar_t *FontPath, float PointSize, float DPI)
{
    dwrite_font Font = {};

    dwrite_state State = DWriteStateCreate(FontPath, PointSize, DPI, &Font);

    // Clear
    ClearDC(State.DC, BackColor, 0, 0, State.RasterTargetWidth, State.RasterTargetHeight);

    font_atlas Atlas = BuildFontAtlas(&Font, &State);

    // Create GPU atlas out of the generated CPU atlas.
    glGenTextures(1, &Font.Texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, Font.Texture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB, Atlas.Width, Atlas.Height, Atlas.Count, 0, GL_RGB, GL_UNSIGNED_BYTE, Atlas.Memory);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    free(Atlas.Memory);
    Atlas.Memory = 0;

    DWriteStateDestroy(&State);

    return Font;
}