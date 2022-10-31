#include "svgstyleparser.h"
#include "svgparser.h"

static real clamp(real val, real min, real max) { return std::max(min, std::min(val, max)); }

std::string idFromPaintUrl(StringRef id)
{
  id += 3;  // skip "url"
  // we only support refs to ids in the same doc; note that single quotes might be present - url('#xxx')
  while(!id.isEmpty() && *id != '#')
    ++id;
  while(!id.isEmpty() && !isAlpha(id.back()) && !isDigit(id.back()) && !strchr("-_:.", id.back()))
    id.chop(1);
  return id.toString();
}

static SvgAttr parsePaint(const char* name, const char* valuestr)
{
  StringRef value(valuestr);
  if(value.startsWith("url"))
    return SvgAttr(name, idFromPaintUrl(valuestr).c_str());
  if(value == "currentColor")
    return SvgAttr(name, SvgStyle::currentColor);
  if(value == "none")
    return SvgAttr(name, Color::NONE);
  return SvgAttr(name, parseColor(value).color);
}

static real parseFontSize(const char* value)
{
  static constexpr SvgEnumVal fontSize[] = {{"xx-small", 0}, {"x-small", 1},
    {"small", 2}, {"medium", 3}, {"large", 4}, {"x-large", 4}, {"xx-large", 5}};
  static const real sizeTable[] = { 6.9, 8.3, 10.0, 12.0, 14.4, 17.3, 20.7 };

  // to support relative sizes, we should store font-size w/ units!
  SvgLength size = parseLength(value, NaN);
  if(!std::isnan(size.value))
    return size.px();

  int idx = parseEnum(value, fontSize);
  return idx >= 0 ? sizeTable[idx] : 0;
}

static SvgAttr processStdAttribute(SvgAttr::StdAttr stdattr, const char* value)
{
  switch(stdattr) {
  case SvgAttr::COLOR:
    return SvgAttr("color", parseColor(value).color);
  case SvgAttr::COMP_OP:
    return SvgAttr("comp-op", parseEnum(value, SvgStyle::compOp));
  case SvgAttr::DISPLAY:
    return SvgAttr("display", StringRef(value) == "none" ? SvgNode::NoneMode : SvgNode::BlockMode);
  case SvgAttr::FILL:
    return parsePaint("fill", value);
  case SvgAttr::FILL_RULE:
    return SvgAttr("fill-rule", parseEnum(value, SvgStyle::fillRule));
  case SvgAttr::FILL_OPACITY:
    return SvgAttr("fill-opacity", clamp(toReal(value, 1), 0.0, 1.0));
  case SvgAttr::FONT_FAMILY:
    // Don't think there's much point resolving to SvgFont* unless we can also resolve regular fonts (note
    //  that font-family can be comma separated list of names, so we'd also have to preserve that somehow)
    return SvgAttr("font-family", value);
  case SvgAttr::FONT_SIZE:
    return SvgAttr("font-size", parseFontSize(value));
  case SvgAttr::FONT_STYLE:
    return SvgAttr("font-style", parseEnum(value, SvgStyle::fontStyle));
  case SvgAttr::FONT_VARIANT:
    return SvgAttr("font-variant", parseEnum(value, SvgStyle::fontVariant));
  case SvgAttr::FONT_WEIGHT:
  {
    real weightNum = toReal(value, NaN);
    if(!std::isnan(weightNum))
      return SvgAttr("font-weight", int(weightNum));
    else
      return SvgAttr("font-weight", parseEnum(value, SvgStyle::fontWeight));
  }
  case SvgAttr::OFFSET:
  {
    SvgLength len = parseLength(value, 0);
    real offset = len.units == SvgLength::PERCENT ? len.value/100 : len.value;
    return SvgAttr("offset", clamp(offset, 0.0, 1.0));
  }
  case SvgAttr::OPACITY:
    return SvgAttr("opacity", clamp(toReal(value, 1), 0.0, 1.0));
  case SvgAttr::SHAPE_RENDERING:
    return SvgAttr("shape-rendering", parseEnum(value, SvgStyle::shapeRendering, SvgStyle::Antialias));
  case SvgAttr::STOP_COLOR:
    return parsePaint("stop-color", value);  // this will incorrectly accept URLs
  case SvgAttr::STOP_OPACITY:
    return SvgAttr("stop-opacity", clamp(toReal(value, 1), 0.0, 1.0));
  case SvgAttr::STROKE:
    return parsePaint("stroke", value);
  case SvgAttr::STROKE_DASHARRAY:
  {
    std::vector<real> dashes = parseNumbersList(value, 8);
    dashes.push_back(-1);  // terminate w/ negative number
    std::vector<float> dashesf(dashes.begin(), dashes.end());
    return SvgAttr("stroke-dasharray", dashesf.data(), dashesf.size()*sizeof(float));
  }
  case SvgAttr::STROKE_DASHOFFSET:
    return SvgAttr("stroke-dashoffset", toReal(value, 0));
  case SvgAttr::STROKE_LINECAP:
    return SvgAttr("stroke-linecap", parseEnum(value, SvgStyle::lineCap));
  case SvgAttr::STROKE_LINEJOIN:
    return SvgAttr("stroke-linejoin", parseEnum(value, SvgStyle::lineJoin));
  case SvgAttr::STROKE_MITERLIMIT:
    return SvgAttr("stroke-miterlimit", toReal(value, 0));
  case SvgAttr::STROKE_OPACITY:
    return SvgAttr("stroke-opacity", clamp(toReal(value, 1), 0.0, 1.0));
  case SvgAttr::STROKE_WIDTH:
    return SvgAttr("stroke-width", toReal(value, 0));
  case SvgAttr::TEXT_ANCHOR:
    return SvgAttr("text-anchor", parseEnum(value, SvgStyle::textAnchor));
  case SvgAttr::VECTOR_EFFECT:
    return SvgAttr("vector-effect", parseEnum(value, SvgStyle::vectorEffect));
  case SvgAttr::VISIBILITY:
    return SvgAttr("visibility", parseEnum(value, SvgStyle::visibility, 1));
  case SvgAttr::LETTER_SPACING:
    return SvgAttr("letter-spacing", toReal(value, 0));
  default:
    return SvgAttr("", "");  // should never happen
  }
}

SvgAttr processAttribute(SvgAttr::Src src, const char* name, const char* value)
{
  // should we reject attributes w/ invalid values (INT_MIN, NAN, "", ...)?
  SvgAttr::StdAttr stdattr = SvgAttr::nameToStdAttr(name);
  if(stdattr == SvgAttr::UNKNOWN)
    return SvgAttr(name, value, src);
  return processStdAttribute(stdattr, value).setFlags(src | stdattr);
}

bool processAttribute(SvgNode* node, SvgAttr::Src src, const char* name, const char* value)
{
  if(!name || !name[0] || StringRef(value) == "inherit")
    return false;
  //node->ext()->parseAttribute(name, value, src)
  // should we reject attributes w/ invalid values (INT_MIN, NAN, "", ...)?
  node->setAttr(processAttribute(src, name, value));
  return true;
}

void processStyleString(SvgNode* node, const char* style)
{
  if(!style || !style[0])
    return;
  // style string is easy to parse - we don't need CSS parser!
  std::vector<StringRef> cssAttrs = splitStringRef(style, ';', true);
  node->attrs.reserve(node->attrs.size() + cssAttrs.size());
  for(const StringRef& attr : cssAttrs) {
    std::vector<StringRef> name_val = splitStringRef(attr, ':', false);
    // this could be done much more efficiently - use splitStrInPlace then just strchr for ':'
    // ... we'd also need trimInPlace
    if(name_val.size() == 2)
      processAttribute(node, SvgAttr::InlineStyleSrc,
          name_val[0].trimmed().toString().c_str(), name_val[1].trimmed().toString().c_str());
    else if(!attr.trimmed().isEmpty())
      PLATFORM_LOG("Invalid CSS in style attribute\n");
  }
}

#ifndef NO_CSS

class SvgCssDecls : public css_declarations
{
public:
  bool isTag(void* el, const char* tag) const override
    { return strcmp(tag, SvgNode::nodeNames[static_cast<SvgNode*>(el)->type()]) == 0; }
  bool hasClass(void* el, const char* cls) const override { return static_cast<SvgNode*>(el)->hasClass(cls); }
  bool hasId(void* el, const char* id) const override { return strcmp(id, static_cast<SvgNode*>(el)->xmlId()) == 0; }
  const char* attribute(void* el, const char* name) const override { return NULL; }  // not supported (yet)
  void* parent(void* el) const override { return static_cast<SvgNode*>(el)->parent(); }
  void parseDecl(const char* name, const char* value) override;

  std::vector<SvgAttr> attrs;
};

void SvgCssDecls::parseDecl(const char* name, const char* value)
{
  StringRef valref(value);
  if(valref.startsWith("var") && valref.back() == ')' && valref.slice(3).trimL().front() == '(') {
    valref.slice(1).trimL();  // remove "(\s*"
    valref.chop(1).trimR();  // remove "\s*)"
    attrs.push_back(SvgAttr(name, valref.toString().c_str(), SvgAttr::CSSSrc | SvgAttr::Variable));
  }
  else
    attrs.push_back(processAttribute(SvgAttr::CSSSrc, name, value));
}

css_declarations* SvgCssStylesheet::createCssDecls() { return new SvgCssDecls; }

void SvgCssStylesheet::applyStyle(SvgNode* node) const
{
  std::vector<SvgAttr> varAttrs;
  for(const css_rule& rule : rules()) {
    if(rule.select(node)) {
      for(const SvgAttr& attr : ((const SvgCssDecls*)rule.decls())->attrs) {
        if(attr.getFlags() & SvgAttr::Variable) {
          SvgAttr* curr = const_cast<SvgAttr*>(node->getAttr(attr.name(), SvgAttr::CSSSrc));
          if(!curr || curr->isStale())
            varAttrs.push_back(attr);
          // prevent replacement by a lower priority value
          if(curr)
            curr->setStale(false);
          else
            node->setAttr(attr);
        }
        else
          node->setAttr(attr);
      }
    }
  }
  // now resolve value of all variables - separate pass needed to get correct value for vars set and
  //  referenced on same node; separate varAttrs list used instead of writing unresolved attrs to node
  //  (previous behavior) to prevent unnecessary dirtying of node
  for(const SvgAttr& attr : varAttrs) {
    // allow replacement (or force removal if unresolved)
    SvgAttr* curr = const_cast<SvgAttr*>(node->getAttr(attr.name(), SvgAttr::CSSSrc));
    if(curr)
      curr->setStale(true);
    // find closest ancestor with the variable set
    for(SvgNode* n = node; n; n = n->parent()) {
      const SvgAttr* valattr = n->getAttr(attr.stringVal(), SvgAttr::CSSSrc);
      // !isStale() to prevent use of stale attribute from same node (no ancestors will have stale attrs)
      if(valattr && !valattr->isStale()) {
        // note that we use setAttribute so that the value of the variable is parsed
        if(valattr->valueIs(SvgAttr::StringVal))  // should always be true
          node->setAttribute(attr.name(), valattr->stringVal(), SvgAttr::CSSSrc);
        break;
      }
    }
  }
}

#endif
