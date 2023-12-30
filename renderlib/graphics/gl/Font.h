#pragma once

#include <glad/glad.h> // for gl types

#include "stb/stb_truetype.h"

#include <string>

class Font
{
public:
  Font();
  ~Font();

  void load(const char* filename);
  void unload();

  // void drawText(const char* text, float x, float y, float scale, float color[4]);

  // x and y will be updated to the next position to draw the next character.
  bool getBakedQuad(char char_index, float* x, float* y, stbtt_aligned_quad* q);

  float getStringWidth(std::string text);

  GLuint getTextureID() const { return m_texID; }

private:
  static constexpr int m_firstChar = 32;
  static constexpr int m_numChars = 96;
  stbtt_bakedchar m_cdata[m_numChars]; // ASCII 32..126 is 95 glyphs

  GLuint m_texID;
};