#pragma once

#include "svgnode.h"

#ifndef NO_CSS
#include "cssparser.h"

class SvgCssStylesheet : public css_stylesheet
{
public:
  css_declarations* createCssDecls() override;
  void applyStyle(SvgNode* node) const;
};
#endif

std::string idFromPaintUrl(StringRef id);
void processStyleString(SvgNode* node, const char* style);
bool processAttribute(SvgNode* node, SvgAttr::Src src, const char* name, const char* value);

struct SvgEnumVal { const char* str; int val; };

template<int N>
int parseEnum(const StringRef& value, const SvgEnumVal (&enumvals)[N], int dflt = INT_MIN)
{
  for(const SvgEnumVal& enumval : enumvals) {
    if(enumval.str == value)
      return enumval.val;
  }
  return dflt;
}

template<int N>
const char* enumToStr(int value, const SvgEnumVal (& enumvals)[N])
{
  for(const SvgEnumVal& enumval : enumvals) {
    if(enumval.val == value)
      return enumval.str;
  }
  //PLATFORM_LOG("Invalid value: %d", value);
  return "";
}

namespace SvgStyle
{
  enum RelFontWeight { LighterFont = -1, BolderFont = 1 };
  enum SpecialColor { currentColor = 1 };
  enum ShapeRendering { Antialias = 0, NoAntialias = 1 };

  static constexpr SvgEnumVal fillRule[] =
      {{"evenodd", Path2D::EvenOddFill}, {"nonzero", Path2D::WindingFill}};
  static constexpr SvgEnumVal vectorEffect[] =
      {{"none", Painter::NoVectorEffect}, {"non-scaling-stroke", Painter::NonScalingStroke}};
  static constexpr SvgEnumVal fontStyle[] =
      {{"normal", Painter::StyleNormal}, {"italic", Painter::StyleItalic}, {"oblique", Painter::StyleOblique}};
  static constexpr SvgEnumVal fontWeight[] =
      {{"normal", 400}, {"bold", 700}, {"bolder", BolderFont}, {"lighter", LighterFont}};
  static constexpr SvgEnumVal fontVariant[] =
      {{"normal", Painter::MixedCase}, {"small-caps", Painter::SmallCaps}};
  static constexpr SvgEnumVal lineCap[] =
      {{"butt", Painter::FlatCap}, {"round", Painter::RoundCap}, {"square", Painter::SquareCap}};
  static constexpr SvgEnumVal lineJoin[] =
      {{"miter", Painter::MiterJoin}, {"round", Painter::RoundJoin}, {"bevel", Painter::BevelJoin}};
  static constexpr SvgEnumVal textAnchor[] =
      {{"start", Painter::AlignLeft}, {"middle", Painter::AlignHCenter}, {"end", Painter::AlignRight}};
  static constexpr SvgEnumVal shapeRendering[] =
      {{"auto", Antialias}, {"crispEdges", NoAntialias}, {"optimizeSpeed", NoAntialias}};
  static constexpr SvgEnumVal visibility[] = {{"hidden", 0}, {"collapse", 0}, {"visible", 1}};
  static constexpr SvgEnumVal compOp[] = {
      {"clear", Painter::CompOp_Clear},
      {"src", Painter::CompOp_Src},
      {"dst", Painter::CompOp_Dest},
      {"src-over", Painter::CompOp_SrcOver},
      {"dst-over", Painter::CompOp_DestOver},
      {"src-in", Painter::CompOp_SrcIn},
      {"dst-in", Painter::CompOp_DestIn},
      {"src-out", Painter::CompOp_SrcOut},
      {"dst-out", Painter::CompOp_DestOut},
      {"src-atop", Painter::CompOp_SrcAtop},
      {"dst-atop", Painter::CompOp_DestAtop},
      {"xor", Painter::CompOp_Xor},
      {"plus", Painter::CompOp_Plus},
      {"multiply", Painter::CompOp_Multiply},
      {"screen", Painter::CompOp_Screen},
      {"overlay", Painter::CompOp_Overlay},
      {"darken", Painter::CompOp_Darken},
      {"lighten", Painter::CompOp_Lighten},
      {"color-dodge", Painter::CompOp_ColorDodge},
      {"color-burn", Painter::CompOp_ColorBurn},
      {"hard-light", Painter::CompOp_HardLight},
      {"soft-light", Painter::CompOp_SoftLight},
      {"difference", Painter::CompOp_Difference},
      {"exclusion", Painter::CompOp_Exclusion}};
};
