#include "main.h"
#include "play.h"

// #define MODE_FUNCTION(name) void name(State* state)
// typedef UPDATE_AND_RENDER(ModeFunction);

// MODE_FUNCTION(runNormalMode);
// MODE_FUNCTION(runInsertMode);
// MODE_FUNCTION(runExMode);

// static
// MODE_FUNCTION(runNormalMode)
// {
// }

// static
// MODE_FUNCTION(runInsertMode)
// {

// }

EntireFile
readEntireFile(char *path)
{
    EntireFile result = {};

    FILE *in = fopen(path, "rb");
    if (in)
    {
        fseek(in, 0, SEEK_END);
        result.contentsSize = ftell(in);
        fseek(in, 0, SEEK_SET);
        result.contents = malloc(result.contentsSize);
        fread(result.contents, result.contentsSize, 1, in);
        fclose(in);
    }
    else
    {
        printf("ERROR: Cannot open file %s\n", path);
    }

    return result;
}


static Font*
loadFont(char *fontFile, uint32 codePointCount)
{
    Font* font = (Font*) malloc(sizeof(Font));

    EntireFile ttfFile = readEntireFile(fontFile);
    if (ttfFile.contents != 0)
    {
        stbtt_InitFont(&font->handle, (uint8*)ttfFile.contents,
                    stbtt_GetFontOffsetForIndex((uint8*)ttfFile.contents, 0));
        font->contents = ttfFile.contents;

        stbtt_GetFontVMetrics(&font->handle, &font->ascent, &font->descent, &font->lineGap);
        font->codePointCount = codePointCount;
        // font->bitmapIDs = (BitmapId*) malloc(sizeof(BitmapId) * codePointCount);
    }

    return font;
}

extern "C" UPDATE_AND_RENDER(UpdateAndRender)
{
  assert(sizeof(State) <= memory->permanentStorageSize);

  State *gameState = (State *)memory->permanentStorage;

  if(!gameState->isInitialized)
  {
    gameState->isInitialized = true;
  }

  //NOTE(casey): Transient initialization
  assert(sizeof(TransientState) <= memory->transientStorageSize);
  TransientState *transientState = (TransientState *)memory->transientStorage;
  if (!transientState->isInitialized)
  {
    initializeArena(&transientState->arena,
                    memory->transientStorageSize - sizeof(TransientState),
                    (uint8*) memory->transientStorage + sizeof(TransientState));

    transientState->isInitialized = true;
  }

  if (input->executableReloaded)
  {
    printf("RELOAD\n");
  }

  Bitmap drawBuffer = {};
  drawBuffer.width  = safeTruncateToUint16(backBuffer->width);
  drawBuffer.height = safeTruncateToUint16(backBuffer->height);
  drawBuffer.pitch  = safeTruncateToInt16(backBuffer->pitch);
  drawBuffer.memory = backBuffer->memory;

  v4 color = {0, 1, 0, 0};

  uint32 color32 = ((roundReal32ToUInt32(color.a * 255.0f) << 24) |
                    (roundReal32ToUInt32(color.r * 255.0f) << 16) |
                    (roundReal32ToUInt32(color.g * 255.0f) << 8)  |
                    (roundReal32ToUInt32(color.b * 255.0f) << 0));

  uint8* row = (uint8*) backBuffer->memory;

  for (int y = 0;
       y < drawBuffer.height;
       ++y)
  {
    uint32* pixel = (uint32*) row;
    for (int x = 0;
         x < drawBuffer.width;
         ++x)
    {
      *pixel = color32;
      ++pixel;
    }

    row += drawBuffer.pitch;
  }

    Font *debugFont = loadFont("/usr/share/fonts/TTF/Vera.ttf", 'z' - '!');


    int w = 20;
    int h = 20;

    int codepoint = 'a';
    uint8* bitmap = stbtt_GetCodepointBitmap(&debugFont->handle, 0,stbtt_ScaleForPixelHeight(&debugFont->handle, drawBuffer.height), codepoint, &w, &h, 0,0);

   for (int j=0; j < h; ++j)
   {
      for (int i=0; i < w; ++i)
      {
        ((uint8*)drawBuffer.memory)[j*w+i] = bitmap[j*w+i];
      }
   }

    // debugFont->handle

  // runNormalMode(gameState);
}
