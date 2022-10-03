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

// NOTE(Oskar): This function builds a font atlas in memory. The atlas based on multiple slices where each slice
// can fit 4 glyphs each. There is no rect packing or magic hapening but instead just a chunk of memory
// allocated based on the cap height of the font.
// We proceed by looping through all the glyphs within the font and render them to the RenderTarget we've 
// prepared earlier also calculating the font metrics that we need later for rendering.
// Finally we blit the glyph manually from the rendertarget onto the atlas.
font_atlas
BuildFontAtlas(dwrite_font *Font, dwrite_state *State)
{
    font_atlas Atlas = {};

    // NOTE(Oskar): Allocate Atlas based on slice width and height.
    int32_t GlyphSize = 4 * (int32_t)(((float)State->FontMetrics.capHeight)*State->PixelPerDesignUnit);
    int32_t QuarterCount = Font->GlyphCount / 4;
    Atlas.Width  = 25 * GlyphSize;
    Atlas.Height = 100 * GlyphSize;

    Atlas.Count = Font->GlyphCount;
    uint32_t AtlasMemorySize = (Atlas.Width * Atlas.Height * 3);
    Atlas.Memory = (uint8_t *)malloc(AtlasMemorySize);
    memset(Atlas.Memory, 0, AtlasMemorySize);

    // NOTE(Oskar): Allocate font metrics based on the number of glyphs available.
    Font->Metrics = (glyph_metrics *)malloc(sizeof(glyph_metrics) * Font->GlyphCount);
    memset(Font->Metrics, 0, sizeof(glyph_metrics) * Font->GlyphCount);

    uint32_t Column = 0;
    uint32_t Row = 0;
    for (uint16_t GlyphIndex = 0; GlyphIndex < Font->GlyphCount; ++GlyphIndex)
    {
        // NOTE(Oskar): Render glyph into RenderTarget
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

        // NOTE(Oskar): Compute the glyph metrics and store them.
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
        Font->Metrics[GlyphIndex].UVX     = ((float)(Column * GlyphSize)) / Atlas.Width;
        Font->Metrics[GlyphIndex].UVY     = ((float)(Row * GlyphSize)) / Atlas.Height;
        Font->Metrics[GlyphIndex].UVW     = (float)TextureWidth / Atlas.Width;
        Font->Metrics[GlyphIndex].UVH     = (float)TextureHeight / Atlas.Height;

        // NOTE(Oskar): Get Bitmap from RenderTaget and blit the bitmap to the allocated atlas manually.
        HBITMAP Bitmap = (HBITMAP)GetCurrentObject(State->DC, OBJ_BITMAP);
        DIBSECTION DIB = {};
        GetObject(Bitmap, sizeof(DIB), &DIB);

        int32_t XSliceOffset = (Column);
        int32_t YSliceOffset = (Row);
        uint8_t *AtlasSlice = Atlas.Memory + ((((Row * Atlas.Width) + Column) * GlyphSize) * 3);
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

        Column++;
        if ((Column * GlyphSize) >= Atlas.Width)
        {
            Column = 0;
            Row++;
        }

        // NOTE(Oskar): Clear render target
        ClearDC(State->DC, BackColor, BoundingBox.left, BoundingBox.top, BoundingBox.right, BoundingBox.bottom); 
    }

    return Atlas;
}

dwrite_state
DWriteStateCreate(wchar_t *FontPath, float PointSize, float DPI, dwrite_font *Font)
{
    dwrite_state State = {0};

    HRESULT Error = 0;
    
    // NOTE(Oskar): Initialize factory interface which provdes access to everything needed.
    Error = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)&State.Factory);
    CheckPointer(Error, State.Factory, Assert(!"Factory"));

    // NOTE(Oskar): Set the DWrite font to use a specific file.
    Error = State.Factory->CreateFontFileReference(FontPath, 0, &State.FontFile);
    CheckPointer(Error, State.FontFile, Assert(!"FontFile"));

    // NOTE(Oskar): Create a font face which refers to in-memory data representation of renderable font.
    // This can work from different sources but we use a IDWriteFontFile. This implementation assumes truetype is used
    // for more robust implemetnation this some code needs to check this.
    Error = State.Factory->CreateFontFace(DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &State.FontFile, 0, DWRITE_FONT_SIMULATIONS_NONE, &Font->Font);
    CheckPointer(Error, Font->Font, Assert(!"Font Face"));

    // NOTE(Oskar): In order to draw the individual glyphs we need to set some parameters.
    // The defaults refer to system defaults on the machine whic can be edited from control panel.
    FLOAT Gamma = 1.0f;
    Error = State.Factory->CreateRenderingParams(&State.DefaultRenderingParams);
    CheckPointer(Error, State.DefaultRenderingParams, Assert(!"Default Rendering Params"));
    Error = State.Factory->CreateCustomRenderingParams(Gamma,
                                                       State.DefaultRenderingParams->GetEnhancedContrast(),
                                                       State.DefaultRenderingParams->GetClearTypeLevel(),
                                                       State.DefaultRenderingParams->GetPixelGeometry(),
                                                       DWRITE_RENDERING_MODE_NATURAL,
                                                       &State.RenderingParams);
    CheckPointer(Error, State.RenderingParams, Assert(!"Rendering Params"));

    // NOTE(Oskar): This implemetnation uses GDI in order to draw the glyphs. 
    // Here we are getting an interface that provide some methods that allow us to get DirectWrite
    // to work with GDI. Mainly this is uised to create a bitmap render target.
    State.DWriteGDIInterop = 0;
    Error = State.Factory->GetGdiInterop(&State.DWriteGDIInterop);
    CheckPointer(Error, State.DWriteGDIInterop, Assert(!"GDI Interop"));

    // NOTE(Oskar): Here we get font metrics that contains scaling information for converting
    // design units into pixels as well as line spacing and various dimensions.
    // This API assumes we know our DPI.
    State.FontMetrics = {};
    Font->Font->GetMetrics(&State.FontMetrics);

    // NOTE(Oskar): The font metrics are specified in Design units. This means that we need to convert
    // it into pixel units.
    // Important keywords to understand the process:
    //  Design Unit      - Abstract unit independent of screen or text size and varies in resolution between fonts.
    //  Em               - Unit that scales relative to the visual size of the text.
    //  Point            - Fixed unit of physical length. This is 1 / 72 Inch
    //  Design Unit / Em - The scale of a font's design unit. This exists within the Font Metrics.
    //  Point / Em       - The point size of text. For example 12pt text is it 12 Point / Em.
    //  Inch / Point     - Always 1 / 72
    //  Pixel / Inch     - Also known as DPI. Default is 96 if your application is not DPI aware.
    // IMPORTANT: This is just information relevant to this Microsoft API and it is not guaranteed to be
    // translatable to other APIs.
    State.PixelPerEM = PointSize * (1.0f / 72.0f) * DPI;
    State.PixelPerDesignUnit = State.PixelPerEM / ((float)State.FontMetrics.designUnitsPerEm);

    State.RasterTargetWidth  = (int32_t)(8.0f*((float)State.FontMetrics.capHeight)*State.PixelPerDesignUnit);
    State.RasterTargetHeight = (int32_t)(8.0f*((float)State.FontMetrics.capHeight)*State.PixelPerDesignUnit);
    State.RasterTargetX = (float)(State.RasterTargetWidth / 2);
    State.RasterTargetY = (float)(State.RasterTargetHeight / 2);

    Assert((float) ((int)(State.RasterTargetX)) == State.RasterTargetX);
    Assert((float) ((int)(State.RasterTargetY)) == State.RasterTargetY);

    // NOTE(Oskar): Tells us the total number of glyphs that the font can rasterize for us. 
    Font->GlyphCount = Font->Font->GetGlyphCount();

    // NOTE(Oskar): Here we create an interface to the actual pixel data that draw operations will be 
    // recorded to. 
    // It is on us to make sure that everything we render onto this bitmap will actually fit and after
    // rendering it is not dumb to check the bounding box of the glyph against the target to see that it
    // actually fit.
    Error = State.DWriteGDIInterop->CreateBitmapRenderTarget(0, State.RasterTargetWidth, State.RasterTargetHeight, &State.RenderTarget);
    
    // NOTE(Oskar): This gives us a GDI based HDC that allows us to make GDI calls that render to the
    // IDWriteBitmapRenderTarget buffer.
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

    ClearDC(State.DC, BackColor, 0, 0, State.RasterTargetWidth, State.RasterTargetHeight);
    font_atlas Atlas = BuildFontAtlas(&Font, &State);

    // NOTE(Oskar): Create an OpenGL texture based on the texture atlas.
    glGenTextures(1, &Font.Texture);
    glBindTexture(GL_TEXTURE_2D, Font.Texture);
    // glTexImage3D(GL_TEXTURE_2D, 0, GL_RGB, Atlas.Width, Atlas.Height, Atlas.Count, 0, GL_RGB, GL_UNSIGNED_BYTE, Atlas.Memory);
    glTextureStorage2D(Font.Texture, 1, GL_RGB8, Atlas.Width, Atlas.Height);
    glTextureSubImage2D(Font.Texture, 0, 0, 0, Atlas.Width, Atlas.Height, GL_RGB, GL_UNSIGNED_BYTE, Atlas.Memory);

    glTextureParameteri(Font.Texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(Font.Texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(Font.Texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(Font.Texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    free(Atlas.Memory);
    Atlas.Memory = 0;

    DWriteStateDestroy(&State);

    return Font;
}