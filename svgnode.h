#pragma once

#include <string>
#include <memory>
#include <list>
#include <unordered_map>
#include "ulib/path2d.h"
#include "ulib/image.h"
#include "ulib/painter.h"  // for Color and Gradient
#include "svgxml.h"

#ifndef NDEBUG
#define DEBUG_CACHED_BOUNDS 1
#endif

class SvgNode;
class SvgContainerNode;
class SvgDocument;
class SvgPainter;
class SvgWriter;

class SvgAttr
{
public:
  enum StdAttr { UNKNOWN = 0, COLOR, COMP_OP, DISPLAY, FILL, FILL_OPACITY, FILL_RULE, FONT_FAMILY, FONT_SIZE,
    FONT_STYLE, FONT_VARIANT, FONT_WEIGHT, OFFSET, OPACITY, SHAPE_RENDERING, STOP_COLOR, STOP_OPACITY, STROKE,
    STROKE_DASHARRAY, STROKE_DASHOFFSET, STROKE_LINECAP, STROKE_LINEJOIN, STROKE_MITERLIMIT, STROKE_OPACITY,
    STROKE_WIDTH, TEXT_ANCHOR, VECTOR_EFFECT, VISIBILITY, LETTER_SPACING };

  static StdAttr nameToStdAttr(const char* name);

  enum { Stale = 0x10000, NoSerialize = 0x20000, Variable = 0x40000 };
  bool isStale() const { return flags & Stale; }
  void setStale(bool stale) { if(stale != isStale()) flags ^= Stale; }

  enum Src { XMLSrc = 0x1000, CSSSrc = 0x2000, InlineStyleSrc = 0x4000, AnySrc = 0xF000 };
  Src src() const { return Src(flags & 0xF000); }
  StdAttr stdAttr() const { return StdAttr(flags & 0xFF); }
  // note this only sets src and StdAttr
  SvgAttr& setFlags(unsigned int f) { flags = f | valueType(); return *this; }
  unsigned int getFlags() const { return flags; }

  const char* name() const { return str.c_str(); }
  bool nameIs(const char* s) const { return strcmp(name(), s) == 0; }
  bool nameIs(StdAttr std) const { return stdAttr() == std; }

  enum ValueType { IntVal = 0x100, ColorVal = 0x200, FloatVal = 0x300, StringVal = 0x400 };
  ValueType valueType() const { return ValueType(flags & 0x0F00); }
  bool valueIs(ValueType type) const { return valueType() == type; }
  friend bool operator==(const SvgAttr& a, const SvgAttr& b);

  int intVal() const { return value.intVal; }
  color_t colorVal() const { return value.colorVal; }
  float floatVal() const { return value.floatVal; }
  const char* stringVal() const { return str.data() + value.strOffset; }
  size_t stringLen() const { return str.size() - value.strOffset; }

  SvgAttr(const char* n, int v, int f = XMLSrc) : str(n), flags(f | IntVal) { value.intVal = v; }
  SvgAttr(const char* n, color_t v, int f = XMLSrc) : str(n), flags(f | ColorVal) { value.colorVal = v; }
  SvgAttr(const char* n, float v, int f = XMLSrc) : str(n), flags(f | FloatVal) { value.floatVal = v; }
  SvgAttr(const char* n, double v, int f = XMLSrc) : str(n), flags(f | FloatVal) { value.floatVal = v; }
  //SvgAttr(const char* n, void* v, int f = XMLSrc) : str(n), flags(f | PtrVal) { value.ptrVal = v; }
  SvgAttr(const char* n, const char* v, int f = XMLSrc) : str(n), flags(f | StringVal)
    { str += '\0';  value.strOffset = str.size();  str += v; }
  // Previously, we made hack of storing arbitrary data in str official but this is dangerous because we
  //  can't guarantee proper alignment - so we'll force the only use case, stroke-dasharray, to use
  //  stringVal to make 1-byte alignment explicit
  SvgAttr(const char* n, const void* v, size_t len, int f = XMLSrc) : str(n), flags(f | StringVal)
    { str += '\0';  value.strOffset = str.size();  str.append((const char*)v, len); }

private:
  // str stores name and, if present, string value; std::string is 24-32 bytes, char* is 8 bytes, so not much
  //  benefit to deduping name; for Write, path data completely dominates svg memory use
  std::string str;
  union {
    int intVal;
    color_t colorVal;
    float floatVal;
    unsigned int strOffset;
  } value;
  unsigned int flags;  // source (XML, CSS, style=), value type, standard attribute id
};

class SvgLength {
public:
  real value;
  enum Units { PX = 0, PT, EM, EX, PERCENT } units;

  SvgLength(real v = NAN, Units u = PX) : value(v), units(u) {}
  bool isValid() const { return !std::isnan(value); }
  bool isPercent() const { return units == PERCENT; }
  real px(real dpi = defaultDpi) const { return units == PT ? value*dpi/72 : value; }

  friend bool operator==(const SvgLength& a, const SvgLength& b) { return a.value == b.value && a.units == b.units; }
  friend bool operator!=(const SvgLength& a, const SvgLength& b) { return !(a == b); }

  static const char* unitNames[];
  static real defaultDpi;
};

// extension class interface
class SvgNodeExtension
{
public:
  SvgNodeExtension(SvgNode* n);
  virtual ~SvgNodeExtension() {}
  virtual SvgNodeExtension* clone() const = 0;
  virtual SvgNodeExtension* createExt(SvgNode* n) const = 0;

  virtual void applyStyle(SvgPainter* painter) const {}
  virtual void onAttrChange(const char* name) {}
  virtual void serializeAttr(SvgWriter* writer) {}
  //virtual bool parseAttribute(const char* name, const char* value, SvgAttr::Src src) { return false; }

  // these are called only for SvgCustomNode
  virtual void draw(SvgPainter* svgp) const {}
  virtual Rect bounds(SvgPainter* svgp) const { return Rect(); }
  virtual void serialize(SvgWriter* writer) const {}
  virtual Rect dirtyRect() const;

  SvgNode* node;

protected:
  SvgNodeExtension(const SvgNodeExtension&) = default;
};


class SvgNode
{
public:
  enum Type { DOC = 0, G, A, DEFS, SYMBOL, PATTERN, GRADIENT, STOP, FONT, FONTFACE, GLYPH, ARC, CIRCLE,
      ELLIPSE, IMAGE, LINE, PATH, POLYGON, POLYLINE, RECT, TEXT, TSPAN, TEXTPATH, USE, UNKNOWN, CUSTOM };

  static const int NUM_NODE_TYPES = CUSTOM+1;
  static const char* nodeNames[];
  static Transform2D identityTransform;

  static std::string nodePath(const SvgNode* node);  // for debugging - should probably be non-static

  enum DisplayMode { NoneMode, BlockMode, AbsoluteMode };
  enum DirtyFlag {NOT_DIRTY=0, CHILD_DIRTY, PIXELS_DIRTY, BOUNDS_DIRTY};

  SvgNode() {}
  virtual ~SvgNode() {}
  void deleteFromExt();

  SvgNode* parent() const { return m_parent; }
  void setParent(SvgNode* parent) { m_parent = parent; }
  SvgDocument* document() const;
  SvgDocument* rootDocument() const;

  virtual Type type() const = 0;
  virtual SvgNode* clone() const = 0;
  virtual SvgContainerNode* asContainerNode() { return NULL; }
  virtual const SvgContainerNode* asContainerNode() const { return NULL; }
  bool isPaintable() const;

  Rect bounds() const;
  Rect cachedBounds() const { return m_cachedBounds; }

  bool hasTransform() const { return bool(transform); }
  const Transform2D& getTransform() const { return transform ? *transform : identityTransform; }
  void setTransform(const Transform2D& tf);
  Transform2D totalTransform() const;

  void invalidate(bool children) const;
  virtual void invalidateBounds(bool inclChildren, bool inclParents = true) const;
  virtual bool restyle();
  void setDirty(DirtyFlag type) const;

  void setDisplayMode(DisplayMode display);
  DisplayMode displayMode() const;
  bool isVisible() const;

  const char* xmlId() const { return m_id.c_str(); }
  void setXmlId(const char* id);

  const char* xmlClass() const { return m_class.c_str(); }
  void setXmlClass(const char* str);
  bool hasClass(const char* s) const;
  void addClass(const char* s);
  void removeClass(const char* s);

  // attributes
  void setAttribute(const char* name, const char* value, SvgAttr::Src src = SvgAttr::XMLSrc);
  void setAttr(const SvgAttr& attr);
  template<typename T>
  void setAttr(const char* name, T val, SvgAttr::Src src = SvgAttr::XMLSrc)
    { setAttr(SvgAttr( name, val, src | SvgAttr::nameToStdAttr(name) )); }
  void removeAttr(const char* name, int src = SvgAttr::AnySrc);

  const SvgAttr* getAttr(const char* name, int src = SvgAttr::AnySrc) const;
  int getIntAttr(const char* name, int dflt = INT_MIN) const;
  Color getColorAttr(const char* name, color_t dflt = Color::INVALID_COLOR) const;
  float getFloatAttr(const char* name, float dflt = NAN) const;
  const char* getStringAttr(const char* name, const char* dflt = NULL) const;
  SvgNode* getRefTarget(const char* id) const;
  void cssToInlineStyle();

  SvgNodeExtension* ext(bool create = true) const;
  void setExt(SvgNodeExtension*);
  void createExt();
  bool hasExt() const { return bool(m_ext); }

protected:
  SvgNode(const SvgNode&);

  bool setAttrHelper(const SvgAttr& attr);
  void onAttrChange(const char* name, SvgAttr::StdAttr stdattr);

public:
  std::vector<SvgAttr> attrs;
  std::unique_ptr<Transform2D> transform;  // prior to SVG 2, transform is not a presentation attribute

//private:
  mutable Rect m_cachedBounds;
  mutable Rect m_renderedBounds;
  mutable DirtyFlag m_dirty = NOT_DIRTY;

  SvgNode* m_parent = NULL;
  std::unique_ptr<SvgNodeExtension> m_ext;
  bool m_visible = true;
  std::string m_id;
  std::string m_class;
};

class SvgXmlFragment : public SvgNode
{
public:
  std::unique_ptr<XmlFragment> fragment;
  SvgXmlFragment(XmlFragment* frag) : fragment(frag) {}
  SvgXmlFragment(const SvgXmlFragment& other) : SvgNode(other), fragment(other.fragment->clone()) {}
  Type type() const override { return UNKNOWN; }
  SvgXmlFragment* clone() const override { return new SvgXmlFragment(*this); }
};

// Should we rename this to SvgForeignNode (type FOREIGN; serialize as <foreignObject>)?
// or should we support using ext()->draw() for any node type?  would we ever want to support children for custom node?
class SvgCustomNode : public SvgNode
{
public:
  Type type() const override { return CUSTOM; }
  SvgCustomNode* clone() const override { return new SvgCustomNode(*this); }
};

// this eliminates redundant, error-prone code w/o forcing us to unwrap smart pointers every place we iterate
template<typename T>
struct cloning_container
{
  T c;
  cloning_container() {}
  cloning_container(const cloning_container& other) = delete;
  cloning_container(SvgNode* newparent, const cloning_container& other)
  {
    //c.reserve(other.c.size()); -- not avail for list
    for(auto& ptr : other.c) {
      c.push_back(ptr->clone());
      c.back()->setParent(newparent);
    }
  }
  ~cloning_container() { for(auto& ptr : c) delete ptr; }
  T& get() { return c; }
  const T& get() const { return c; }
};

// consider shorter name ... SvgGroupNode?
class SvgContainerNode : public SvgNode
{
public:
  SvgContainerNode() {}
  SvgContainerNode(const SvgContainerNode& other) : SvgNode(other), m_children(this, other.m_children) {}
  SvgContainerNode* clone() const override = 0;  // this is needed to clone container nodes w/o casting result
  SvgContainerNode* asContainerNode() override { return this; }
  const SvgContainerNode* asContainerNode() const override { return this; }
  void invalidateBounds(bool inclChildren, bool inclParents = true) const override;
  bool restyle() override;

  void addChild(SvgNode* child, SvgNode* next = NULL);
  SvgNode* removeChild(SvgNode* child);
  std::list<SvgNode*>& children() { return m_children.get(); }
  const std::list<SvgNode*>& children() const { return m_children.get(); }
  SvgNode* firstChild() const { return children().empty() ? NULL : children().front(); }
  SvgNode* nodeAt(const Point& p, bool visual_only = true) const;
  std::vector<SvgNode*> select(const char* selector, size_t nhits = SIZE_MAX) const;
  SvgNode* selectFirst(const char* selector) const;

//protected:
  cloning_container< std::list<SvgNode*> > m_children;
  mutable Rect m_removedBounds;
};

class SvgG : public SvgContainerNode
{
public:
  SvgG(Type grouptype = G) : groupType(grouptype) {}
  Type type() const override { return G; }
  SvgG* clone() const override { return new SvgG(*this); }
//private:
  Type groupType;
};

class SvgDefs : public SvgContainerNode
{
public:
  Type type() const override { return DEFS; }
  SvgDefs* clone() const override { return new SvgDefs(*this); }
};

class SvgSymbol : public SvgContainerNode
{
public:
  Type type() const override { return SYMBOL; }
  SvgSymbol* clone() const override { return new SvgSymbol(*this); }
};

class SvgPattern : public SvgContainerNode
{
public:
  enum Units_t {userSpaceOnUse, objectBoundingBox};

  SvgPattern(real x, real y, real w, real h, Units_t pu, Units_t pcu);
  Type type() const override { return PATTERN; }
  SvgPattern* clone() const override { return new SvgPattern(*this); }

//private:
  Rect m_cell;
  Units_t m_patternUnits;
  Units_t m_patternContentUnits;
};

class SvgGradientStop : public SvgNode
{
public:
  Type type() const override { return STOP; }
  SvgGradientStop* clone() const override { return new SvgGradientStop(*this); }
  // stop-color, stop-opacity, and offset are all SvgAttrs
};

class SvgGradient : public SvgNode
{
public:
  SvgGradient(const Gradient& grad) : m_gradient(grad) {}
  SvgGradient(const SvgGradient& other) : SvgNode(other), m_gradient(other.m_gradient), m_stops(this, other.m_stops) {}
  Type type() const override { return GRADIENT; }
  SvgGradient* clone() const override { return new SvgGradient(*this); }
  bool restyle() override;

  void addStop(SvgGradientStop* stop);
  void setStopLink(SvgGradient* link) { m_link = link; }
  //const char* stopLink() const { return m_link.c_str(); }
  std::vector<SvgGradientStop*>& stops() { return m_stops.get(); }
  const std::vector<SvgGradientStop*>& stops() const { return m_stops.get(); }
  Gradient& gradient() const;

//private:
  mutable Gradient m_gradient;
  cloning_container< std::vector<SvgGradientStop*> > m_stops;
  SvgGradient* m_link = NULL;
  //std::string m_linkStr;
};

#ifndef NO_DYNAMIC_STYLE
class SvgCssStylesheet;
#endif

class SvgFont;

class SvgDocument : public SvgContainerNode
{
public:
  SvgDocument(real x = 0, real y = 0,
      SvgLength w = SvgLength(100, SvgLength::PERCENT), SvgLength h = SvgLength(100, SvgLength::PERCENT));
  Type type() const override { return DOC; }
  SvgDocument* clone() const override;

  SvgLength width() const { return m_useWidth > 0 ? SvgLength(m_useWidth) : m_width; }
  SvgLength height() const { return m_useHeight > 0 ? SvgLength(m_useHeight) : m_height; }
  void setWidth(const SvgLength& w);
  void setHeight(const SvgLength& h);

  void setPreserveAspectRatio(bool v) { m_preserveAspectRatio = v; }
  bool preserveAspectRatio() const { return m_preserveAspectRatio; }
#ifndef NO_DYNAMIC_STYLE
  ~SvgDocument() override;
  void setStylesheet(SvgCssStylesheet* ss);
  SvgCssStylesheet* stylesheet() { return m_stylesheet; }
#endif

  Rect viewBox() const { return m_viewBox; }
  void setViewBox(const Rect& r) { if(r != m_viewBox) invalidate(true); m_viewBox = r; }
  // canvas rect is only used for top-level doc w/ percentage for width and/or height
  Rect canvasRect() const { return m_canvasRect; }
  void setCanvasRect(const Rect& r) { if(r != m_canvasRect) invalidate(true); m_canvasRect = r; }
  void setUseSize(real w, real h);

  Rect viewportRect() const;
  Transform2D viewBoxTransform() const;

  void addSvgFont(SvgFont*);
  SvgFont* svgFont(const char* family, int weight = 400, Painter::FontStyle = Painter::StyleNormal) const;
  void addNamedNode(SvgNode* node);
  void removeNamedNode(SvgNode* node);
  SvgNode* namedNode(const char* id) const;
  void restyleNode(SvgNode* node);
  bool canRestyle();
  void replaceIds(SvgDocument* dest = NULL);

//private:
  real m_x = 0, m_y = 0;
  SvgLength m_width, m_height;
  real m_useWidth = 0, m_useHeight = 0;
  Rect m_viewBox;
  bool m_preserveAspectRatio = true;
  Rect m_canvasRect;

  std::unordered_multimap<std::string, SvgFont*> m_fonts;  // key is family name, can have >1 style per family
  std::unordered_map<std::string, SvgNode*> m_namedNodes;
#ifndef NO_DYNAMIC_STYLE
  // if we use unique_ptr, have to create boilerplate copy constructor
  SvgCssStylesheet* m_stylesheet = NULL;
#endif
};

class SvgImage : public SvgNode
{
public:
  SvgImage(Image image, const Rect& bounds, const char* linkStr = NULL);
  SvgImage(const SvgImage& other);
  Type type() const override { return IMAGE; }
  SvgImage* clone() const override { return new SvgImage(*this); }
  Image* image() { return &m_image; }
  void setSize(const Rect& r) { m_bounds = r; invalidate(false); }
  Rect viewport() const;

//private:
  Image m_image;
  Rect m_bounds;
  std::string m_linkStr;

  Rect srcRect;
};

class SvgPath : public SvgNode
{
public:
  SvgPath(const Path2D& path, Type pathtype = PATH) : m_path(path), m_pathType(pathtype) {}
  SvgPath(Type pathtype = PATH) : m_pathType(pathtype) {}
  Type type() const override { return PATH; }
  SvgPath* clone() const override { return new SvgPath(*this); }

  Path2D* path() { return &m_path; }
  Type pathType() const { return m_pathType; }

//protected:
  Path2D m_path;
  Type m_pathType;
};

class SvgRect : public SvgPath
{
public:
  SvgRect(const Rect& rect, real rx = 0, real ry = 0);
  Type type() const override { return RECT; }
  SvgPath* clone() const override { return new SvgRect(*this); }
  Rect getRect() const { return m_rect; }
  std::pair<real, real> getRadii() const { return {m_rx, m_ry}; }
  void setRect(const Rect& rect, real rx = 0, real ry = 0);

//private:
  void updatePath();

  Rect m_rect;
  real m_rx;
  real m_ry;
};

class SvgUse : public SvgNode
{
public:
  SvgUse(const Rect& bounds, const char* linkStr, const SvgNode* link, SvgDocument* doc = NULL);
  Type type() const override { return USE; }
  SvgUse* clone() const override { return new SvgUse(*this); }
  void setTarget(const SvgNode* link);
  const SvgNode* target() const;
  const char* href() const { return m_linkStr.c_str(); }
  void setHref(const char* s) { m_linkStr = s;  m_link = NULL;  invalidate(false); }
  // probably should have separate fns for x,y,width,height
  Rect viewport() const { return m_viewport; }
  void setViewport(const Rect& r) { m_viewport = r; }

private:
  const SvgNode* m_link;
  Rect m_viewport;
  std::string m_linkStr;
  std::shared_ptr<SvgDocument> m_doc;
};

class SvgTspan : public SvgNode
{
public:
  // for <text>ABC<tspan>123</tspan>DEF</text>, isTspan = true for 123 and false for ABC and DEF
  // TODO: change to a Type flag, so we can also store <a> (or just inherit from SvgContainerNode?)
  SvgTspan(bool isTspan = true, const char* text = "") : m_isTspan(isTspan), m_text(text) {}
  SvgTspan(const SvgTspan& other) : SvgNode(other), m_isTspan(other.m_isTspan),
      m_x(other.m_x), m_y(other.m_y), m_tspans(this, other.m_tspans), m_text(other.m_text) {}
  Type type() const override { return TSPAN; }
  SvgTspan* clone() const override { return new SvgTspan(*this); }
  bool restyle() override;

  void addTspan(SvgTspan* tspan);
  void addText(const char* text);
  bool isLineBreak() const { return m_text == "\n"; }
  void clearText();
  std::string text() const;
  std::string displayText() const;
  bool isTspan() const { return m_isTspan; }
  std::vector<SvgTspan*>& tspans() { return m_tspans.get(); }
  const std::vector<SvgTspan*>& tspans() const { return m_tspans.get(); }

//private:
  bool m_isTspan;
  std::vector<real> m_x;
  std::vector<real> m_y;
  // only one of m_tspans and m_text can be non-empty
  cloning_container< std::vector<SvgTspan*> > m_tspans;
  std::string m_text;
};

class SvgText : public SvgTspan
{
public:
  SvgText() : SvgTspan(false) {}
  Type type() const override { return TEXT; }
  SvgText* clone() const override { return new SvgText(*this); }
};

class SvgTextPath : public SvgTspan
{
public:
  SvgTextPath(const char* linkStr, real offset) : SvgTspan(true), m_linkStr(linkStr), m_startOffset(offset) {}
  Type type() const override { return TEXTPATH; }
  SvgTextPath* clone() const override { return new SvgTextPath(*this); }
  const char* href() const { return m_linkStr.c_str(); }
  real startOffset() const { return m_startOffset; }

private:
  std::string m_linkStr;
  real m_startOffset;
};

class SvgGlyph : public SvgNode
{
public:
  SvgGlyph(const char* name, const char* unicode, real horizAdvX);
  Type type() const override { return GLYPH; }
  SvgGlyph* clone() const override { return new SvgGlyph(*this); }

  std::string m_name;
  std::string m_unicode;
  Path2D m_path;
  real m_horizAdvX = NaN;
};

class SvgFontFace : public SvgNode
{
public:
  Type type() const override { return FONTFACE; }
  SvgFontFace* clone() const override { return new SvgFontFace(*this); }
};

class SvgFont : public SvgNode
{
public:
  SvgFont(real horizAdvX) : m_horizAdvX(horizAdvX) {}
  // glyphMap of copy is empty - updated in glyphsForText
  SvgFont(const SvgFont& other) : SvgNode(other), m_familyName(other.m_familyName),
      m_unitsPerEm(other.m_unitsPerEm), m_horizAdvX(other.m_horizAdvX), m_maxUnicodeLen(other.m_maxUnicodeLen),
      m_glyphs(this, other.m_glyphs), m_kerning(other.m_kerning), m_fontface(other.m_fontface->clone()) {}
  Type type() const override { return FONT; }
  SvgFont* clone() const override { return new SvgFont(*this); }

  void setFontFaceNode(SvgFontFace* fontface) { m_fontface.reset(fontface); }
  const SvgFontFace* fontFace() const { return m_fontface.get(); }
  void setFamilyName(const char* name) { m_familyName = name; }
  const char* familyName() const { return m_familyName.c_str(); }
  void setUnitsPerEm(real x) { m_unitsPerEm = x; }
  void addGlyph(SvgGlyph* glyph) { m_glyphs.get().push_back(glyph); }
  std::vector<const SvgGlyph*> glyphsForText(const char* str) const;
  real horizAdvX(const SvgGlyph* glyph) const;
  real kerningForPair(const SvgGlyph* g1, const SvgGlyph* g2) const;
  std::vector<SvgGlyph*>& glyphs() { return m_glyphs.get(); }

  struct Kerning { std::string g1,g2,u1,u2; real k; };

//private:
  std::string m_familyName;
  real m_unitsPerEm = 0;
  real m_horizAdvX;
  mutable int m_maxUnicodeLen = 0;
  cloning_container< std::vector<SvgGlyph*> > m_glyphs;
  mutable std::unordered_map<std::string, SvgGlyph*> m_glyphMap;
  std::vector<Kerning> m_kerning;
  std::unique_ptr<SvgFontFace> m_fontface;
};
