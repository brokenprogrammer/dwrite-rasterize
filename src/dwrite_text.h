struct glyph_metrics
{
    float OffsetX;
    float OffsetY;
    float Advance;
    float XYW;
    float XYH;
    float UVX;
    float UVY;
    float UVW;
    float UVH;
};

struct dwrite_state
{
    IDWriteFactory *Factory;
    IDWriteFontFile *FontFile;
    IDWriteRenderingParams *DefaultRenderingParams;
    IDWriteRenderingParams *RenderingParams;
    IDWriteGdiInterop *DWriteGDIInterop;
    IDWriteBitmapRenderTarget *RenderTarget;
    HDC DC;

    DWRITE_FONT_METRICS FontMetrics;
    
    float PixelPerEM;
    float PixelPerDesignUnit;

    int32_t RasterTargetWidth;
    int32_t RasterTargetHeight;
    float RasterTargetX;
    float RasterTargetY;
};

struct dwrite_font
{
    IDWriteFontFace *Font;
    GLuint Texture;
    glyph_metrics *Metrics;
    int32_t GlyphCount;
    int32_t TextureWidth;
    int32_t TextureHeight;
};

struct font_atlas
{
    uint8_t *Memory;
    int32_t Width;
    int32_t Height;
    int32_t Count;
};

static COLORREF BackColor = RGB(0, 0, 0);
static COLORREF ForeColor = RGB(255, 255, 255);
