#pragma once
#include "svgnode.h"

class XmlStreamWriter;
struct IOStream;

class SvgWriter
{
public:
  XmlStreamWriter& xml;
  float saveImageScaled = DEFAULT_SAVE_IMAGE_SCALED;
  bool pathDataRel = DEFAULT_PATH_DATA_REL;
  std::vector<SvgNode*> tempNodes;

  SvgWriter(XmlStreamWriter& _xml) : xml(_xml) {}
  ~SvgWriter();
  void serialize(SvgNode* node);

  static bool DEBUG_CSS_STYLE;
  static bool DEFAULT_PATH_DATA_REL;
  static float DEFAULT_SAVE_IMAGE_SCALED;
  static int SVG_FLOAT_PRECISION;

  static void save(SvgNode* node, IOStream& strm, const char* indent = "  ");

  static char* serializeColor(char* buff, const Color& color);
  // TODO: make these non-static and use xml.defaultFloatPrecision instead
  static char* serializeLength(char* buff, const SvgLength& len, bool writePx = false, int prec = SVG_FLOAT_PRECISION);
  static int writeNumbersList(char* str, const std::vector<real>& vals, char sep = ' ', int prec = SVG_FLOAT_PRECISION);
  static char* serializeTransform(char* buff, const Transform2D& tf, int prec = SVG_FLOAT_PRECISION);

private:
  void writePaint(const SvgAttr& attr);
  void writeAttribute(const SvgAttr& attr);
  void serializeNodeAttr(SvgNode* node);
  void serializeChildren(SvgContainerNode* node);
  void serializeTspan(SvgTspan* node);

  void _serialize(SvgDocument* node);
  void _serialize(SvgG* node);
  void _serialize(SvgDefs* node);
  void _serialize(SvgPath* node);
  void _serialize(SvgRect* node);
  void _serialize(SvgImage* node);
  void _serialize(SvgUse* node);
  void _serialize(SvgTspan* node);
  void _serialize(SvgTextPath* node);
  void _serialize(SvgFont* node);
  void _serialize(SvgSymbol* node);
  void _serialize(SvgPattern* node);
  void _serialize(SvgGradient* node);
  void _serialize(SvgXmlFragment* node);
  void _serialize(SvgCustomNode* node);
};
