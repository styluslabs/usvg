#include "svgwriter.h"
#include "svgstyleparser.h"


// include CSS style when serializing
bool SvgWriter::DEBUG_CSS_STYLE = false;
bool SvgWriter::DEFAULT_PATH_DATA_REL = true;
float SvgWriter::DEFAULT_SAVE_IMAGE_SCALED = 0;
int SvgWriter::SVG_FLOAT_PRECISION = 3;

char* SvgWriter::serializeColor(char* buff, const Color& color)
{
  static const char* hexDigits = "0123456789ABCDEF";

  if(color.color == Color::NONE || color.color == Color::INVALID_COLOR)  // any alpha == 0?
    strcpy(buff, "none");
  else if(color.alpha() < 255) {
    // probably should use fstring, but it isn't actually used anywhere else in SvgWriter
    strcpy(buff, "rgba(");
    int n = writeNumbersList(buff+5,
        {real(color.red()), real(color.green()), real(color.blue()), color.alphaF()}, ',');
    strcpy(buff+n+5, ")");
  }
  else {
    int r = color.red(), g = color.green(), b = color.blue();
    buff[0] = '#';
    buff[1] = hexDigits[(r & 0xF0) >> 4];  buff[2] = hexDigits[(r & 0x0F)];
    buff[3] = hexDigits[(g & 0xF0) >> 4];  buff[4] = hexDigits[(g & 0x0F)];
    buff[5] = hexDigits[(b & 0xF0) >> 4];  buff[6] = hexDigits[(b & 0x0F)];
    buff[7] = '\0';
  }
  return buff;
}

char* SvgWriter::serializeLength(char* buff, const SvgLength& len, bool writePx, int prec)
{
  int ii = realToStr(buff, len.value, prec < 0 ? SVG_FLOAT_PRECISION : prec);
  const char* unitstr = SvgLength::unitNames[int(len.units)];
  if(len.units != SvgLength::PX || writePx) {
    while(*unitstr) buff[ii++] = *unitstr++;
  }
  buff[ii] = '\0';
  return buff;
}

int SvgWriter::writeNumbersList(char* str, const std::vector<real>& vals, char sep, int prec)
{
  int ii = 0;
  for(real val : vals) {
    ii += realToStr(str + ii, val, prec);
    str[ii++] = sep;
  }
  str[--ii] = '\0';  // overwrite trailing separator
  return ii;  // note return value does not include null terminator
}

char* SvgWriter::serializeTransform(char* buff, const Transform2D& tf, int prec)
{
  char* buff0 = buff;
  // can't use %.4g because it will lose precision for large translations
  if(approxEq(tf, Transform2D(), 1E-4))  // 1E-4 choosen because we're using %.3f
    *buff = '\0';
  else if(tf.isTranslate()) {
    const char* src = "translate(";
    while(*src) *buff++ = *src++;
    buff += realToStr(buff, tf.xoffset(), prec);  *buff++ = ',';  buff += realToStr(buff, tf.yoffset(), prec);
    *buff++ = ')';  *buff++ = '\0';
  }
  else {
    const char* src = "matrix(";
    while(*src) *buff++ = *src++;
    buff += writeNumbersList(buff, {tf.m[0], tf.m[1], tf.m[2], tf.m[3], tf.m[4], tf.m[5]});
    *buff++ = ')';  *buff++ = '\0';
  }
  return buff0;
}

SvgWriter::~SvgWriter()
{
  for(SvgNode* n : tempNodes) {
    SvgNode* parent = n->parent();
    if(parent)
      parent->asContainerNode()->removeChild(n);
    delete n;
  }
}

static const char* stdEnumToStr(SvgAttr attr)
{
  switch(attr.stdAttr()) {
  case SvgAttr::FILL_RULE:       return enumToStr(attr.intVal(), SvgStyle::fillRule);
  case SvgAttr::VECTOR_EFFECT:   return enumToStr(attr.intVal(), SvgStyle::vectorEffect);
  case SvgAttr::FONT_STYLE:      return enumToStr(attr.intVal(), SvgStyle::fontStyle);
  case SvgAttr::FONT_VARIANT:    return enumToStr(attr.intVal(), SvgStyle::fontVariant);
  case SvgAttr::FONT_WEIGHT:     return enumToStr(attr.intVal(), SvgStyle::fontWeight);
  case SvgAttr::STROKE_LINECAP:  return enumToStr(attr.intVal(), SvgStyle::lineCap);
  case SvgAttr::STROKE_LINEJOIN: return enumToStr(attr.intVal(), SvgStyle::lineJoin);
  case SvgAttr::TEXT_ANCHOR:     return enumToStr(attr.intVal(), SvgStyle::textAnchor);
  case SvgAttr::SHAPE_RENDERING: return enumToStr(attr.intVal(), SvgStyle::shapeRendering);
  case SvgAttr::COMP_OP:         return enumToStr(attr.intVal(), SvgStyle::compOp);
  case SvgAttr::DISPLAY:         return attr.intVal() == SvgNode::NoneMode ? "none" : "block";
  case SvgAttr::VISIBILITY:      return attr.intVal() ? "visible" : "hidden";
  case SvgAttr::FILL:            // [[fallthrough]]
  case SvgAttr::STROKE:          // [[fallthrough]]
  case SvgAttr::STOP_COLOR:      return attr.intVal() == SvgStyle::currentColor ? "currentColor" : NULL;
  default:                       return NULL;
  }
}

void SvgWriter::writeAttribute(const SvgAttr& attr)
{
  if(attr.getFlags() & SvgAttr::NoSerialize) {}
  else if(attr.valueIs(SvgAttr::IntVal)) {
    const char* s = stdEnumToStr(attr);
    s ? xml.writeAttribute(attr.name(), s) : xml.writeAttribute(attr.name(), attr.intVal());
  }
  else if(attr.valueIs(SvgAttr::ColorVal))
    xml.writeAttribute(attr.name(), serializeColor(xml.getTemp(), attr.colorVal()));
  else if(attr.valueIs(SvgAttr::FloatVal))
    xml.writeAttribute(attr.name(), attr.floatVal());
  else if(attr.valueIs(SvgAttr::StringVal)) {
    if(attr.stringVal()[0] == '#' && (attr.nameIs(SvgAttr::FILL) || attr.nameIs(SvgAttr::STROKE)))
      xml.writeAttribute(attr.name(), "url(" + std::string(attr.stringVal()) + ")");
    else if(attr.nameIs(SvgAttr::STROKE_DASHARRAY)) {
      std::vector<float> dashesf;
      dashesf.resize(attr.stringLen()/sizeof(float));
      // not aligned in general
      memcpy(dashesf.data(), attr.stringVal(), attr.stringLen());
      std::vector<real> dashes(dashesf.begin(), dashesf.end());
      writeNumbersList(xml.getTemp(), dashes, ' ');
      xml.writeAttribute(attr.name(), xml.getTemp());
    }
    else
      xml.writeAttribute(attr.name(), attr.stringVal());
  }
}

void SvgWriter::serialize(SvgNode* node)
{
  switch(node->type()) {
    case SvgNode::PATH:     _serialize(static_cast<SvgPath*>(node));  break;
    case SvgNode::RECT:     _serialize(static_cast<SvgRect*>(node));  break;
    case SvgNode::G:        _serialize(static_cast<SvgG*>(node));  break;
    case SvgNode::DEFS:     _serialize(static_cast<SvgDefs*>(node));  break;
    case SvgNode::IMAGE:    _serialize(static_cast<SvgImage*>(node));  break;
    case SvgNode::USE:      _serialize(static_cast<SvgUse*>(node));  break;
    case SvgNode::TEXT:     _serialize(static_cast<SvgText*>(node));   break;
    case SvgNode::TSPAN:    _serialize(static_cast<SvgTspan*>(node));   break;
    case SvgNode::DOC:      _serialize(static_cast<SvgDocument*>(node)); break;
    case SvgNode::SYMBOL:   _serialize(static_cast<SvgSymbol*>(node));  break;
    case SvgNode::PATTERN:  _serialize(static_cast<SvgPattern*>(node));  break;
    case SvgNode::GRADIENT: _serialize(static_cast<SvgGradient*>(node));  break;
    case SvgNode::FONT:     _serialize(static_cast<SvgFont*>(node));  break;
    case SvgNode::UNKNOWN:  _serialize(static_cast<SvgXmlFragment*>(node));  break;
    case SvgNode::CUSTOM:   _serialize(static_cast<SvgCustomNode*>(node));  break;
    default: ASSERT(0 && "Unknown node type!");
  }
}

// nodes
void SvgWriter::serializeNodeAttr(SvgNode* node)
{
  if(!node->m_id.empty())
    xml.writeAttribute("id", node->m_id);
  if(!node->m_class.empty())
    xml.writeAttribute("class", node->m_class);
  if(node->hasTransform()) {
    char* buff = serializeTransform(xml.getTemp(), node->getTransform());
    const char* tfname = "transform";
    if(node->type() == SvgNode::GRADIENT) tfname = "gradientTransform";
    else if(node->type() == SvgNode::PATTERN) tfname = "patternTransform";
    if(buff[0])
      xml.writeAttribute(tfname, buff);
  }

  // extension can write attributes directly (preferred) or update node attrs before they're written
  if(node->hasExt())
    node->ext()->serializeAttr(this);

  auto it = node->attrs.begin();
  for(; it != node->attrs.end() && it->src() == SvgAttr::XMLSrc; ++it)
    writeAttribute(*it);
  xml.startStyleAttribute();
  for(; it != node->attrs.end() && it->src() == SvgAttr::InlineStyleSrc; ++it)
    writeAttribute(*it);
  xml.endStyleAttribute();
#ifndef NDEBUG
  if(DEBUG_CSS_STYLE) {
    // m_cssStyle is not normally serialized on node of course!
    xml.startStyleAttribute();
    for(; it != node->attrs.end(); ++it)
      writeAttribute(*it);
    xml.endStyleAttribute("debug:css");
  }
#endif
}

void SvgWriter::serializeChildren(SvgContainerNode* node)
{
  for(SvgNode* child : node->children())
    serialize(child);
}

void SvgWriter::_serialize(SvgG* node)
{
  xml.writeStartElement(SvgNode::nodeNames[node->groupType]);
  serializeNodeAttr(node);
  serializeChildren(node);
  xml.writeEndElement();
}

void SvgWriter::_serialize(SvgDefs* node)
{
  xml.writeStartElement("defs");
  serializeNodeAttr(node);
  serializeChildren(node);
  xml.writeEndElement();
}

void SvgWriter::_serialize(SvgSymbol* node)
{
  xml.writeStartElement("symbol");
  serializeNodeAttr(node);
  serializeChildren(node);
  xml.writeEndElement();
}

void SvgWriter::_serialize(SvgPattern* node)
{
  Rect m_cell = node->m_cell;

  xml.writeStartElement("pattern");
  serializeNodeAttr(node);
  xml.writeAttribute("x", m_cell.left);
  xml.writeAttribute("y", m_cell.top);
  xml.writeAttribute("width", m_cell.width());
  xml.writeAttribute("height", m_cell.height());
  if(node->m_patternUnits == SvgPattern::userSpaceOnUse)
    xml.writeAttribute("patternUnits", "userSpaceOnUse");
  if(node->m_patternContentUnits == SvgPattern::objectBoundingBox)
    xml.writeAttribute("patternContentUnits", "objectBoundingBox");
  serializeChildren(node);
  xml.writeEndElement();
}

void SvgWriter::_serialize(SvgGradient* node)
{
  const Gradient& m_gradient = node->gradient();
  xml.writeStartElement(m_gradient.type == Gradient::Linear ? "linearGradient" : "radialGradient");
  serializeNodeAttr(node);
  xml.writeAttribute("gradientUnits",
      m_gradient.coordinateMode() == Gradient::ObjectBoundingMode ? "objectBoundingBox" : "userSpaceOnUse");

  if(m_gradient.type == Gradient::Linear) {
    const Gradient::LinearGradCoords& g = m_gradient.coords.linear;
    xml.writeAttribute("x1", g.x1);
    xml.writeAttribute("y1", g.y1);
    xml.writeAttribute("x2", g.x2);
    xml.writeAttribute("y2", g.y2);
  }
  else if(m_gradient.type == Gradient::Radial) {
    const Gradient::RadialGradCoords& g = m_gradient.coords.radial;
    xml.writeAttribute("cx", g.cx);
    xml.writeAttribute("cy", g.cy);
    xml.writeAttribute("r",  g.radius);
    xml.writeAttribute("fx", g.fx);
    xml.writeAttribute("fy", g.fy);
  }

  for(SvgGradientStop* stop : node->stops()) {
    xml.writeStartElement("stop");
    serializeNodeAttr(stop);  // includes stop-color, stop-opacity, offset
    xml.writeEndElement();
  }
  xml.writeEndElement();
}

void SvgWriter::_serialize(SvgImage* node)
{
  Rect m_bounds = node->m_bounds;
  xml.writeStartElement("image");
  serializeNodeAttr(node);
  xml.writeAttribute("x", m_bounds.left);
  xml.writeAttribute("y", m_bounds.top);
  if(m_bounds.width() > 0) xml.writeAttribute("width", m_bounds.width());
  if(m_bounds.height() > 0) xml.writeAttribute("height", m_bounds.height());
  //xml.writeAttribute("preserveAspectRatio", "none");

  // m_linkStr will be empty iff image successfully loaded from inline base64
  if(node->m_linkStr.empty()) {
    Image cropped(0, 0);
    bool crop = node->srcRect.isValid() && node->srcRect != Rect::wh(node->m_image.width, node->m_image.height);
    if(crop)
      cropped = node->m_image.cropped(node->srcRect);
    const Image& img = crop ? cropped : node->m_image;

    Transform2D tf = node->totalTransform();
    Rect tf_bounds = tf.mapRect(node->viewport());
    int scaledw = int(saveImageScaled*tf_bounds.width() + 0.5);
    int scaledh = int(saveImageScaled*tf_bounds.height() + 0.5);
    // shrink image to save space if sufficient size change (and new size is not tiny)
    bool scaleimg = saveImageScaled > 0 && tf_bounds.width() > 10 && tf_bounds.height() > 10
        && (scaledw < 0.75*img.width || scaledh < 0.75*img.height);
    // compress image
    auto buff = scaleimg ? img.scaled(scaledw, scaledh).encode() : img.encode();

    std::string prefix = std::string("data:image/") + img.formatName() + ";base64,";
    size_t base64len;
    base64_encode(NULL, buff.size(), NULL, &base64len);  // get encoded size
    base64len += 1;  // account for \0 terminator
    char* base64 = new char[base64len + prefix.size()];
    strcpy(base64, prefix.c_str());
    base64_encode((const unsigned char*)buff.data(), buff.size(), base64 + prefix.size(), &base64len);
    xml.writeAttribute("xlink:href", base64);
    delete[] base64;
  }
  else
    xml.writeAttribute("xlink:href", node->m_linkStr.c_str());

  xml.writeEndElement();
}

static inline char* writePathXY(char* ts, real x, real y, int prec)
{
  ts += realToStr(ts, x, prec);  *ts++ = ' ';  ts += realToStr(ts, y, prec);
  return ts;
}

static size_t maxPathDataLen(const Path2D& m_path)
{
  static constexpr int MAX_CHARS_PER_POINT = 32;
  static constexpr int MIN_PATHD_STR_LEN = 256;
  return m_path.size()*MAX_CHARS_PER_POINT + MIN_PATHD_STR_LEN;
}

static const char* serializePathData(char* buff, const Path2D& m_path, int prec, bool relative = true)
{
  char* ts = buff;
  Path2D::PathCommand prevcmd = Path2D::MoveTo;
  int ncmdpts = 0;
  Point prevpt(0,0);
  for(int i = 0; i < m_path.size(); ++i) {
    const Path2D::PathCommand cmd = m_path.command(i);
    if(i != 0) *ts++ = ' ';
    if(cmd != prevcmd || cmd == Path2D::ArcTo || i == 0) {
      // always write initial moveTo as 'M' to support old version of Write; since we always write 'l',
      //  this is OK ('m' implies 'l' if not specified)
      if(cmd == Path2D::MoveTo) *ts++ = (relative && i != 0 ? 'm' : 'M');
      else if(cmd == Path2D::LineTo) *ts++ = (relative ? 'l' : 'L');
      else if(cmd == Path2D::CubicTo) *ts++ = (relative ? 'c' : 'C');
      else if(cmd == Path2D::QuadTo) *ts++ = (relative ? 'q' : 'Q');
      // arc requires special handling
      else if(cmd == Path2D::ArcTo) {
        if(i + 2 >= m_path.size())
          break;
        *ts++ = (relative ? 'a' : 'A');
        const Point& c = m_path.point(i);  // center
        const Point& r = m_path.point(i+1); // radii
        real a0 = m_path.point(i+2).x;  // start angle
        real sweep = m_path.point(i+2).y;  // sweep angle
        real x1 = c.x + r.x * cos(a0 + sweep);  // stop point
        real y1 = c.y + r.y * sin(a0 + sweep);

        // NOT TESTED!
        // radius, x axis rotation
        ts = writePathXY(ts, r.x, r.y, prec);  *ts++ = ' ';  *ts++ = '0';  *ts++ = ' ';
        // large arc flag and sweep flag
        *ts++ = std::abs(sweep) > M_PI ? '1' : '0';  *ts++ = ' ';  *ts++ = sweep > 0 ? '1' : '0';  *ts++ = ' ';
        ts = writePathXY(ts, x1 - prevpt.x, y1 - prevpt.y, prec);

        i += 2;
        if(relative)
          prevpt = Point(x1, y1);
        ncmdpts = 0;
        prevcmd = cmd;
        continue;
      }
    }
    if(relative && ncmdpts == 0) {
      if(cmd == Path2D::MoveTo) ncmdpts = 1;
      else if(cmd == Path2D::LineTo) ncmdpts = 1;
      else if(cmd == Path2D::CubicTo) ncmdpts = 3;
      else if(cmd == Path2D::QuadTo) ncmdpts = 2;
    }
    const Point& pt = m_path.points[i];
    ts = writePathXY(ts, pt.x - prevpt.x, pt.y - prevpt.y, prec);
    if(relative && --ncmdpts == 0)
      prevpt = pt;
    prevcmd = cmd;
  }
  *ts++ = '\0';
  return buff;
}

void SvgWriter::_serialize(SvgPath* node)
{
  const Path2D& m_path = node->m_path;
  if(node->m_pathType == SvgNode::LINE) {
    xml.writeStartElement("line");
    serializeNodeAttr(node);
    xml.writeAttribute("x1", m_path.point(0).x);
    xml.writeAttribute("y1", m_path.point(0).y);
    xml.writeAttribute("x2", m_path.point(1).x);
    xml.writeAttribute("y2", m_path.point(1).y);
    xml.writeEndElement();
    return;
  }
  if(node->m_pathType == SvgNode::CIRCLE) {
    // we assume circle consists of an even number of CubicTo segments
    Point p0 = m_path.point(0);
    Point p1 = m_path.point((m_path.size() - 1)/2);  // point opposite initial position
    xml.writeStartElement("circle");
    serializeNodeAttr(node);
    xml.writeAttribute("cx", (p0.x + p1.x)/2);
    xml.writeAttribute("cy", (p0.y + p1.y)/2);
    xml.writeAttribute("r", p0.dist(p1)/2);
    xml.writeEndElement();
    return;
  }

  xml.writeStartElement("path");
  serializeNodeAttr(node);
  char* buff = xml.getTemp(maxPathDataLen(node->m_path));
  xml.writeAttribute("d", serializePathData(buff, node->m_path, xml.defaultFloatPrecision, pathDataRel));
  xml.writeEndElement();
}

void SvgWriter::_serialize(SvgRect* node)
{
  xml.writeStartElement("rect");
  serializeNodeAttr(node);
  Rect rect = node->m_rect;
  xml.writeAttribute("x", rect.left);
  xml.writeAttribute("y", rect.top);
  xml.writeAttribute("width", rect.width());
  xml.writeAttribute("height", rect.height());
  if(node->m_rx > 0 || node->m_ry > 0) {
    // it would be better if we used, e.g., -1 to indicate rx or ry not specified
    xml.writeAttribute("rx", node->m_rx);
    xml.writeAttribute("ry", node->m_ry);
  }
  xml.writeEndElement();
}

void SvgWriter::_serialize(SvgTspan* node)
{
  xml.writeStartElement(node->type() == SvgNode::TEXT ? "text" : "tspan");
  serializeNodeAttr(node);
  if(!node->m_x.empty()) {
    writeNumbersList(xml.getTemp(), node->m_x);
    xml.writeAttribute("x", xml.getTemp());
  }
  if(!node->m_y.empty()) {
    writeNumbersList(xml.getTemp(), node->m_y);
    xml.writeAttribute("y", xml.getTemp());
  }
  //xml.writeAttribute("xml:space", "preserve");
  if(node->tspans().empty())
    xml.writeCharacters(node->text().c_str());
  for(SvgTspan* tspan : node->tspans()) {
    if(tspan->isLineBreak()) {
      xml.writeStartElement("tbreak");
      xml.writeEndElement();
    }
    else if(tspan->isTspan())
      _serialize(tspan);
    else
      xml.writeCharacters(tspan->text().c_str());
  }
  xml.writeEndElement();
}

void SvgWriter::_serialize(SvgFont* node)
{
  xml.writeStartElement("font");
  serializeNodeAttr(node);
  xml.writeAttribute("horz-adv-x", node->m_horizAdvX);

  xml.writeStartElement("font-face");
  xml.writeAttribute("font-family", node->familyName());
  xml.writeAttribute("units-per-em", node->m_unitsPerEm);
  xml.writeEndElement();

  for(SvgGlyph* glyph : node->glyphs()) {
    xml.writeStartElement(glyph->m_unicode.empty()? "missing-glyph" : "glyph");
    if(!glyph->m_name.empty()) xml.writeAttribute("glyph-name", glyph->m_name);
    if(!glyph->m_unicode.empty()) xml.writeAttribute("unicode", glyph->m_unicode);
    if(glyph->m_horizAdvX >= 0) xml.writeAttribute("horiz-adv-x", glyph->m_horizAdvX);
    serializeNodeAttr(glyph);
    char* buff = xml.getTemp(maxPathDataLen(glyph->m_path));
    xml.writeAttribute("d", serializePathData(buff, glyph->m_path, xml.defaultFloatPrecision, pathDataRel));
    xml.writeEndElement();
  }
  for(const SvgFont::Kerning& k : node->m_kerning) {
    xml.writeStartElement("hkern");
    if(!k.g1.empty()) xml.writeAttribute("g1", k.g1);
    if(!k.g2.empty()) xml.writeAttribute("g2", k.g2);
    if(!k.u1.empty()) xml.writeAttribute("u1", k.u1);
    if(!k.u2.empty()) xml.writeAttribute("u2", k.u2);
    xml.writeAttribute("k", k.k);
    xml.writeEndElement();
  }
  xml.writeEndElement();
}

void SvgWriter::_serialize(SvgUse* node)
{
  Rect m_bounds = node->viewport();  //m_bounds;

  xml.writeStartElement("use");
  serializeNodeAttr(node);
  if(m_bounds.left != 0)
    xml.writeAttribute("x", m_bounds.left);
  if(m_bounds.top != 0)
    xml.writeAttribute("y", m_bounds.top);
  if(m_bounds.width() > 0)
    xml.writeAttribute("width", m_bounds.width());
  if(m_bounds.height() > 0)
    xml.writeAttribute("height", m_bounds.height());
  xml.writeAttribute("xlink:href", node->href());  //m_linkStr);
  xml.writeEndElement();
}

void SvgWriter::_serialize(SvgDocument* doc)
{
  xml.writeStartElement("svg");
  serializeNodeAttr(doc);
  if(doc->m_x != 0 || doc->m_y != 0) {
    xml.writeAttribute("x", doc->m_x);
    xml.writeAttribute("y", doc->m_y);
  }
  if(!doc->width().isPercent() || doc->width().value != 100)
    xml.writeAttribute("width", serializeLength(xml.getTemp(), doc->m_width, true));
  if(!doc->height().isPercent() || doc->height().value != 100)
    xml.writeAttribute("height", serializeLength(xml.getTemp(), doc->m_height, true));
  Rect vb = doc->viewBox();
  if(vb.isValid()) {
    writeNumbersList(xml.getTemp(), {vb.left, vb.top, vb.width(), vb.height()});
    xml.writeAttribute("viewBox", xml.getTemp());
  }
  if(!doc->preserveAspectRatio())
    xml.writeAttribute("preserveAspectRatio", "none");
  if(!doc->parent()) {
    xml.writeAttribute("xmlns", "http://www.w3.org/2000/svg");
    xml.writeAttribute("xmlns:xlink", "http://www.w3.org/1999/xlink");
  }
  serializeChildren(doc);
  xml.writeEndElement();  // </svg>
}

void SvgWriter::_serialize(SvgXmlFragment* node)
{
  xml.writeFragment(*node->fragment.get());
}

void SvgWriter::_serialize(SvgCustomNode* node)
{
  xml.writeStartElement("foreignObject");
  serializeNodeAttr(node);
  if(node->hasExt())
    node->ext()->serialize(this);
  xml.writeEndElement();
}
