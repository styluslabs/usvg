#pragma once

#include <functional>
#include "svgnode.h"
#include "svgstyleparser.h"

class SvgParser
{
public:
  SvgParser();
  SvgDocument* parseFile(const char* filename, unsigned int opts = XmlStreamReader::ParseDefault);
  SvgDocument* parseStream(std::istream& strm, unsigned int opts = XmlStreamReader::ParseDefault);
  SvgDocument* parseString(const char* data, int len = 0, unsigned int opts = XmlStreamReader::ParseDefault);
  SvgDocument* parseFragment(const char* data, int len = 0, unsigned int opts = XmlStreamReader::ParseDefault);
  SvgDocument* parseXml(XmlStreamReader* reader);
  SvgDocument* parseXmlFragment(XmlStreamReader* reader);

  SvgDocument* document() const { return m_doc; }
  bool hasErrors() const { return !m_doc || m_hasErrors; }
  const std::string& fileName() const { return m_fileName; }
  SvgParser& setFileName(const char* s) { m_fileName = s;  return *this; }  // for loading fragments

  // common parsing functions made available for extensions
  std::vector<real>& parseNumbersList(StringRef& str);
  real lengthToPx(const StringRef& str, real dflt);
  real lengthToPx(SvgLength len);

  real dpi() const { return m_dpi; }
  void setDpi(real dpi) { m_dpi = dpi; }
  static real defaultDpi;

  // optional handler to return stream for a file name - to support, e.g., embedded resources
  static std::function<std::istream*(const char*)> openStream;

private:
  struct State {
    real emPx = 12;
    //real exPts = 8;
  };
  SvgDocument* m_doc = NULL;
  std::vector<SvgNode*> m_nodes;
  std::vector<State> m_states;
  State& currState() { return m_states.back(); }

  real m_dpi = defaultDpi;

  struct NodeAttribute {
    const char* name;
    const char* value;
    NodeAttribute(const char* n, const char* v) : name(n), value(v) {}
  };
  std::vector<NodeAttribute> nodeAttributes;
  int numUsedAttributes = 0;
  std::vector<real> numberList;

  bool m_inStyle = false;
  std::unique_ptr<SvgCssStylesheet> m_stylesheet;

  void parse(XmlStreamReader* const xml);
  const char* useAttribute(const char* name);
  bool startElement(StringRef localName, const XmlStreamAttributes& attributes);
  bool endElement(StringRef localName);
  bool cdata(const char* str);

  SvgNode* createNode(StringRef name);
  SvgDocument* createSvgDocumentNode();
  SvgNode* createANode();
  SvgNode* createCircleNode();
  SvgNode* createDefsNode();
  SvgNode* createEllipseNode();
  SvgNode* createGNode();
  SvgNode* createImageNode();
  SvgNode* createLineNode();
  SvgNode* createLinearGradientNode();
  SvgNode* createPathNode();
  SvgNode* createPatternNode();
  SvgNode* createPolygonNode();
  SvgNode* createPolylineNode();
  SvgNode* createRectNode();
  SvgNode* createRadialGradientNode();
  SvgNode* createSymbolNode();
  SvgNode* createTextNode();
  SvgNode* createTspanNode();
  SvgNode* createUseNode();
  SvgNode* createStopNode();
  SvgFont* createFontNode();
  SvgGlyph* createGlyphNode();

  SvgNode* parseTspanNode(SvgTspan* node);
  void parseFontFaceNode(SvgFont* font);
  void parseFontKerning(SvgFont* font);
  void parseBaseGradient(SvgGradient* gradNode);

  bool parseCoreNode(SvgNode* node);
  const char* useId();
  const char* useHref();
  std::string toAbsPath(StringRef relpath);

  std::string m_fileName;
  bool m_hasErrors = false;
};

inline real strToReal(const char* p, char** endptr) { return strToReal<real>(p, endptr); }

real toReal(const StringRef& str, real dflt = NaN, int* advance = NULL);
std::vector<real>& parseNumbersList(StringRef& str, std::vector<real>& points);
std::vector<real> parseNumbersList(const StringRef& str, size_t reserve);
SvgLength parseLength(const StringRef& str, const SvgLength& dflt = SvgLength());
Color parseColor(StringRef str, const Color& dflt = Color::INVALID_COLOR);
