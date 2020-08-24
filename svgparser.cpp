#include <fstream>
#include "svgparser.h"


struct SvgNamedColor {
  const char* name;
  color_t color;
  friend bool operator<(const SvgNamedColor& a, const SvgNamedColor& b) { return strcmp(a.name, b.name) < 0; }
};

// added non-standard colors: grey (== gray)
SvgNamedColor svgNamedColors[] = {
  {"aliceblue", 0xF0F8FF}, {"antiquewhite", 0xFAEBD7}, {"aqua", 0x00FFFF}, {"aquamarine", 0x7FFFD4},
  {"azure", 0xF0FFFF}, {"beige", 0xF5F5DC}, {"bisque", 0xFFE4C4}, {"black", 0x000000},
  {"blanchedalmond", 0xFFEBCD}, {"blue", 0x0000FF}, {"blueviolet", 0x8A2BE2}, {"brown", 0xA52A2A},
  {"burlywood", 0xDEB887}, {"cadetblue", 0x5F9EA0}, {"chartreuse", 0x7FFF00}, {"chocolate", 0xD2691E},
  {"coral", 0xFF7F50}, {"cornflowerblue", 0x6495ED}, {"cornsilk", 0xFFF8DC}, {"crimson", 0xDC143C},
  {"cyan", 0x00FFFF}, {"darkblue", 0x00008B}, {"darkcyan", 0x008B8B}, {"darkgoldenrod", 0xB8860B},
  {"darkgray", 0xA9A9A9}, {"darkgreen", 0x006400}, {"darkkhaki", 0xBDB76B}, {"darkmagenta", 0x8B008B},
  {"darkolivegreen", 0x556B2F}, {"darkorange", 0xFF8C00}, {"darkorchid", 0x9932CC}, {"darkred", 0x8B0000},
  {"darksalmon", 0xE9967A}, {"darkseagreen", 0x8FBC8F}, {"darkslateblue", 0x483D8B},
  {"darkslategray", 0x2F4F4F}, {"darkturquoise", 0x00CED1}, {"darkviolet", 0x9400D3}, {"deeppink", 0xFF1493},
  {"deepskyblue", 0x00BFFF}, {"dimgray", 0x696969}, {"dodgerblue", 0x1E90FF}, {"firebrick", 0xB22222},
  {"floralwhite", 0xFFFAF0}, {"forestgreen", 0x228B22}, {"fuchsia", 0xFF00FF}, {"gainsboro", 0xDCDCDC},
  {"ghostwhite", 0xF8F8FF}, {"gold", 0xFFD700}, {"goldenrod", 0xDAA520}, {"gray", 0x808080},
  {"green", 0x008000}, {"greenyellow", 0xADFF2F}, {"grey", 0x808080}, {"honeydew", 0xF0FFF0},
  {"hotpink", 0xFF69B4}, {"indianred", 0xCD5C5C}, {"indigo", 0x4B0082}, {"ivory", 0xFFFFF0},
  {"khaki", 0xF0E68C}, {"lavender", 0xE6E6FA}, {"lavenderblush", 0xFFF0F5}, {"lawngreen", 0x7CFC00},
  {"lemonchiffon", 0xFFFACD}, {"lightblue", 0xADD8E6}, {"lightcoral", 0xF08080}, {"lightcyan", 0xE0FFFF},
  {"lightgoldenrodyellow", 0xFAFAD2}, {"lightgreen", 0x90EE90}, {"lightgrey", 0xD3D3D3},
  {"lightpink", 0xFFB6C1}, {"lightsalmon", 0xFFA07A}, {"lightseagreen", 0x20B2AA}, {"lightskyblue", 0x87CEFA},
  {"lightslategray", 0x778899}, {"lightsteelblue", 0xB0C4DE}, {"lightyellow", 0xFFFFE0}, {"lime", 0x00FF00},
  {"limegreen", 0x32CD32}, {"linen", 0xFAF0E6}, {"magenta", 0xFF00FF}, {"maroon", 0x800000},
  {"mediumaquamarine", 0x66CDAA}, {"mediumblue", 0x0000CD}, {"mediumorchid", 0xBA55D3},
  {"mediumpurple", 0x9370DB}, {"mediumseagreen", 0x3CB371}, {"mediumslateblue", 0x7B68EE},
  {"mediumspringgreen", 0x00FA9A}, {"mediumturquoise", 0x48D1CC}, {"mediumvioletred", 0xC71585},
  {"midnightblue", 0x191970}, {"mintcream", 0xF5FFFA}, {"mistyrose", 0xFFE4E1}, {"moccasin", 0xFFE4B5},
  {"navajowhite", 0xFFDEAD}, {"navy", 0x000080}, {"oldlace", 0xFDF5E6}, {"olive", 0x808000},
  {"olivedrab", 0x6B8E23}, {"orange", 0xFFA500}, {"orangered", 0xFF4500}, {"orchid", 0xDA70D6},
  {"palegoldenrod", 0xEEE8AA}, {"palegreen", 0x98FB98}, {"paleturquoise", 0xAFEEEE},
  {"palevioletred", 0xDB7093}, {"papayawhip", 0xFFEFD5}, {"peachpuff", 0xFFDAB9}, {"peru", 0xCD853F},
  {"pink", 0xFFC0CB}, {"plum", 0xDDA0DD}, {"powderblue", 0xB0E0E6}, {"purple", 0x800080}, {"red", 0xFF0000},
  {"rosybrown", 0xBC8F8F}, {"royalblue", 0x4169E1}, {"saddlebrown", 0x8B4513}, {"salmon", 0xFA8072},
  {"sandybrown", 0xF4A460}, {"seagreen", 0x2E8B57}, {"seashell", 0xFFF5EE}, {"sienna", 0xA0522D},
  {"silver", 0xC0C0C0}, {"skyblue", 0x87CEEB}, {"slateblue", 0x6A5ACD}, {"slategray", 0x708090},
  {"snow", 0xFFFAFA}, {"springgreen", 0x00FF7F}, {"steelblue", 0x4682B4}, {"tan", 0xD2B48C},
  {"teal", 0x008080}, {"thistle", 0xD8BFD8}, {"tomato", 0xFF6347}, {"turquoise", 0x40E0D0},
  {"violet", 0xEE82EE}, {"wheat", 0xF5DEB3}, {"white", 0xFFFFFF}, {"whitesmoke", 0xF5F5F5},
  {"yellow", 0xFFFF00}, {"yellowgreen", 0x9ACD32}
};
#ifndef NDEBUG
// no luck trying to do this with static_assert
static struct svgNamedColors_CHECK_t { svgNamedColors_CHECK_t() {
    ASSERT(std::is_sorted(std::begin(svgNamedColors), std::end(svgNamedColors))); }} svgNamedColors_CHECK;
#endif

real SvgParser::defaultDpi = 96;

// realToStr and strToReal take the most cycles, so could try github.com/miloyip/itoa-benchmark/
// There's also github.com/miloyip/dtoa-benchmark/ for float to str, but I don't think any of these
//  would beat our int approach if fully optimized

real toReal(const StringRef& str, real dflt, int* advance)
{
  const char* s0 = str.data();
  const char* s = s0;
  const char* e = str.end();
  while(s != e && isSpace(*s)) ++s;
  // it's ok to access *str.end() because we assume underlying string is always null terminated
  if(s == e || !(isDigit(*s) || *s == '-' || *s == '+' || *s == '.')) {
    if(advance)
      *advance = 0;
    return dflt;
  }

  char* endptr;
  char c = *e;
  if(c && (isDigit(c) || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E')) {
#ifndef NDEBUG
    PLATFORM_LOG("Creating temporary string in toReal\n");
#endif
    // have to use a copy if strToReal could advance beyond end of StringRef
    std::string temp(s, e - s);
    const char* t = temp.c_str();
    real res = strToReal(t, &endptr);
    if(advance)
      *advance = (endptr - t) + (s - s0);
    return res;
  }

  real res = strToReal(s, &endptr);
  if(advance)
    *advance = endptr - s0;
  return res;
}

// parse list of numbers separated by whitespace or commas
std::vector<real>& parseNumbersList(StringRef& str, std::vector<real>& points)
{
  points.clear();
  if(str.isEmpty())
    return points;
  //points.reserve(32);
  str.trimL();
  //int advance = 0;
  char* endptr;
  while(!str.isEmpty() && (isDigit(*str) || *str == '-' || *str == '+' || *str == '.')) {
    //points.push_back(toDouble(str, NULL, &advance));
    //str += advance;
    const char* s = str.data();
    points.push_back(strToReal(s, &endptr));
    str += endptr - s;
    str.trimL();
    if(*str == ',') {
      ++str;
      str.trimL();
    }
  }
  return points;
}

std::vector<real> parseNumbersList(const StringRef& str, size_t reserve)
{
  StringRef str2(str);
  std::vector<real> points;
  points.reserve(reserve);
  parseNumbersList(str2, points);
  return points;
}

std::vector<real>& SvgParser::parseNumbersList(StringRef& str)
{
  return ::parseNumbersList(str, numberList);
}

static Transform2D parseTransformMatrix(StringRef str)
{
  std::vector<real> args;
  args.reserve(6);
  Transform2D matrix;
  while(!str.isEmpty()) {
    if(isSpace(*str) || *str == ',') { ++str; continue; }
    StringRef cmd(str.data(), 0);
    while(!str.isEmpty() && isAlpha(*str)) { ++str; ++cmd.len; }
    str.trimL();
    if(str.isEmpty() || *str != '(') break;
    ++str;
    parseNumbersList(str, args);  // this will consume trailing spaces
    if(str.isEmpty() || *str != ')') break;
    ++str;
    if(cmd == "matrix" && args.size() == 6)
      matrix = matrix * Transform2D(args[0], args[1], args[2], args[3], args[4], args[5]);
    else if(cmd == "translate" && (args.size() == 1 || args.size() == 2))
      matrix = matrix * Transform2D::translating(args[0], args.size() == 2 ? args[1] : 0);
    else if(cmd == "rotate" && (args.size() == 1 || args.size() == 3))
      matrix = matrix * Transform2D::rotating(args[0]*M_PI/180,
          args.size() == 3 ? Point(args[1], args[2]) : Point(0,0));
    else if(cmd == "scale" && (args.size() == 1 || args.size() == 2))
      matrix = matrix * Transform2D::scaling(args[0], args.size() == 2 ? args[1] : args[0]);
    else if(cmd == "skewX" && args.size() == 1)
      matrix = matrix * Transform2D().shear(std::tan(args[0]*M_PI/180), 0);
    else if(cmd == "skewY" && args.size() == 1)
      matrix = matrix * Transform2D().shear(0, std::tan(args[0]*M_PI/180));
    else
      break;
  }
  return matrix;
}

Color parseColor(StringRef str, const Color& dflt)
{
  str = str.trimmed();
  if(str.isEmpty())
    return dflt;

  if(str[0] == '#') {
    ++str;
    size_t len = str.size();
    if(len != 3 && len != 6 && len != 8)
      return dflt;
    char temp[9];
    str.toBuff(temp);
    unsigned long num = strtoul(temp, NULL, 16);
    if(len == 8)
      return Color::fromArgb(num);
    if(len == 6)
      return Color::fromRgb(num);
    // len == 3
    int r = (num >> 8) & 0x0F, g = (num >> 4) & 0x0F, b = num & 0x0F;
    return Color(r*16 + r, g*16 + g, b*16 + b);
  }
  std::string colorLower = toLower(str.toString());
  str = StringRef(colorLower);
  if(str.startsWith("rgb(") || str.startsWith("rgba(")) {
    StringRef compoStr(str + 4);
    int advance;
    if(*compoStr == '(') ++compoStr;
    real r = toReal(compoStr, 0, &advance);  compoStr += advance;  compoStr.trimL();
    if(*compoStr == '%') { r *= 2.55; ++compoStr; compoStr.trimL(); }
    if(*compoStr == ',') { ++compoStr;  compoStr.trimL(); }
    real g = toReal(compoStr, 0, &advance);  compoStr += advance;  compoStr.trimL();
    if(*compoStr == '%') { g *= 2.55; ++compoStr; compoStr.trimL(); }
    if(*compoStr == ',') { ++compoStr;  compoStr.trimL(); }
    real b = toReal(compoStr, NaN, &advance);  compoStr += advance;  compoStr.trimL();
    if(*compoStr == '%') { b *= 2.55; ++compoStr; compoStr.trimL(); }
    // rgba() uses 0 - 1 for opacity, not 0 - 255!
    if(*compoStr == ',') { ++compoStr;  compoStr.trimL(); }
    real a = toReal(compoStr, 1, &advance);  //compoStr += advance;  compoStr.trimL();
    return std::isnan(b) ? dflt : Color(int(r), int(g), int(b), int(a*255));
  }

  if(colorLower == "none")
    return Color::NONE;
  if(colorLower == "inherit")
    return dflt;
  // try named color - assumes svgNamedColors is sorted!
  SvgNamedColor colorNamed = { colorLower.c_str(), 0 };
  auto it = std::lower_bound(std::begin(svgNamedColors), std::end(svgNamedColors), colorNamed);
  if(it != std::end(svgNamedColors) && it->name == colorLower)
    return Color::fromRgb(it->color);
  return dflt;
}

SvgLength parseLength(const StringRef& str, const SvgLength& dflt)
{
  int advance;
  real val = toReal(str, NaN, &advance);
  if(std::isnan(val))
    return dflt;
  StringRef units = (str + advance).trimL();
  if(units.isEmpty() || units.startsWith("px"))
    return SvgLength(val, SvgLength::PX);

  if(units.startsWith("%"))
    return SvgLength(val, SvgLength::PERCENT);
  if(units.startsWith("pt"))
    return SvgLength(val, SvgLength::PT);
  if(units.startsWith("in"))
    return SvgLength(val*72, SvgLength::PT);
  if(units.startsWith("mm"))
    return SvgLength(val*72/25.4, SvgLength::PT);
  if(units.startsWith("cm"))
    return SvgLength(val*72/2.54, SvgLength::PT);
  if(units.startsWith("pc"))
    return SvgLength(val*12, SvgLength::PT);
  if(units.startsWith("em"))
    return SvgLength(val, SvgLength::EM);
  if(units.startsWith("ex"))
    return SvgLength(val, SvgLength::EX);
  return SvgLength(val, SvgLength::PX);
}

real SvgParser::lengthToPx(SvgLength len)
{
  if(len.units == SvgLength::PX)
    return len.value;
  if(len.units == SvgLength::PT)
    return len.value * dpi()/72;
  if(len.units == SvgLength::EM)
    return len.value * currState().emPx;
  if(len.units == SvgLength::EX)
    return len.value * currState().emPx*0.5;
  if(len.units == SvgLength::PERCENT)
    return len.value * 0.01;  // ... 2nd arg to specify vert or horz?
  return len.value;  // should never happen
}

real SvgParser::lengthToPx(const StringRef& str, real dflt)
{
  return lengthToPx(parseLength(str, dflt));
}

std::string SvgParser::toAbsPath(StringRef relpath)
{
  // treat as relative path from current file location unless prefixed with "file://"
  if(relpath.startsWith("/") || (relpath.size() > 1 && relpath[1] == ':'))
    return relpath.toString();
  if(relpath.startsWith("file://"))
    return relpath.substr(7).toString();
  const std::string& thisfile = fileName();
  size_t pathsep = thisfile.find_last_of("/\\");
  if(pathsep != std::string::npos)
    return thisfile.substr(0, pathsep + 1).append(relpath.data(), relpath.size());
  return relpath.toString();
}

// arc code from XSVG (BSD)
static void pathArc(Path2D& path, real rx, real ry,
    real x_axis_rotation, int large_arc_flag, int sweep_flag, real x, real y, real curx, real cury)
{
  real sin_th, cos_th;
  real a00, a01, a10, a11;
  real x0, y0, x1, y1, xc, yc;
  real d, sfactor, sfactor_sq;
  real th0, th1, th_arc;
  real dx, dy, dx1, dy1, Pr1, Pr2, Px, Py, check;

  rx = std::abs(rx);
  ry = std::abs(ry);

  x_axis_rotation = x_axis_rotation * M_PI/180;
  sin_th = std::sin(x_axis_rotation);
  cos_th = std::cos(x_axis_rotation);

  dx = (curx - x) / 2.0;
  dy = (cury - y) / 2.0;
  dx1 =  cos_th * dx + sin_th * dy;
  dy1 = -sin_th * dx + cos_th * dy;
  Pr1 = rx * rx;
  Pr2 = ry * ry;
  Px = dx1 * dx1;
  Py = dy1 * dy1;
  // check if radii are large enough
  check = Px / Pr1 + Py / Pr2;
  if(check > 1) {
    rx = rx * std::sqrt(check);
    ry = ry * std::sqrt(check);
  }

  a00 =  cos_th / rx;
  a01 =  sin_th / rx;
  a10 = -sin_th / ry;
  a11 =  cos_th / ry;
  x0 = a00 * curx + a01 * cury;
  y0 = a10 * curx + a11 * cury;
  x1 = a00 * x + a01 * y;
  y1 = a10 * x + a11 * y;
  // (x0, y0) is current point in transformed coordinate space.
  // (x1, y1) is new point in transformed coordinate space.
  // The arc fits a unit-radius circle in this space.
  d = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0);
  sfactor_sq = 1.0 / d - 0.25;
  if(sfactor_sq < 0) sfactor_sq = 0;
  sfactor = std::sqrt(sfactor_sq);
  if(sweep_flag == large_arc_flag) sfactor = -sfactor;
  xc = 0.5 * (x0 + x1) - sfactor * (y1 - y0);
  yc = 0.5 * (y0 + y1) + sfactor * (x1 - x0);
  // (xc, yc) is center of the circle.
  th0 = std::atan2(y0 - yc, x0 - xc);
  th1 = std::atan2(y1 - yc, x1 - xc);

  th_arc = th1 - th0;
  if(th_arc < 0 && sweep_flag)
    th_arc += 2 * M_PI;
  else if(th_arc > 0 && !sweep_flag)
    th_arc -= 2 * M_PI;

  // decomposition to Beziers (if necessary) is handled by Path2D
  path.addArc(xc*rx, yc*ry, rx, ry, th0, th_arc, x_axis_rotation);
}

// we take vector to use so we don't need to alloc and free for every path
static bool parsePathData(const StringRef& dataStr, Path2D& path, std::vector<real>& points)
{
  real x0 = 0, y0 = 0;  // initial point
  real x = 0, y = 0;  // current point
  char lastMode = 0;
  Point ctrlPt;
  StringRef str = dataStr;

  while(!str.isEmpty()) {
    str.trimL();
    char pathElem = *str;
    ++str;
    parseNumbersList(str, points);
    if(pathElem == 'z' || pathElem == 'Z')
      points.push_back(0);
    const real* num = points.data();
    int count = points.size();
    // if path's points vector would growth more than once w/ 2x growth factor, reserve space now
    if(str.isEmpty() || count/2 > path.size() + 1)  // +1 to prevent reserve on initial moveTo
      path.reserve(path.size() + count/2);  // number of points >= number of scalars/2
    const real* end = num + count;
    while(num != end) {
      bool rel = pathElem >= 'a';
      real offsetX = rel ? x : 0;
      real offsetY = rel ? y : 0;
      switch(tolower(pathElem)) {
      case 'm':
        if(end - num < 2) { num = end; break; }
        x = x0 = num[0] + offsetX;
        y = y0 = num[1] + offsetY;
        num += 2;
        path.moveTo(x0, y0);
        // additional coord pairs following moveto command are treated as lineto commands
        pathElem = rel ? 'l' : 'L';
        break;
      case 'z':
        x = x0;
        y = y0;
        num++;  // skip placeholder
        path.closeSubpath();
        break;
      case 'l':
        if(end - num < 2) { num = end; break; }
        x = num[0] + offsetX;
        y = num[1] + offsetY;
        num += 2;
        path.lineTo(x, y);
        break;
      case 'h':
        x = num[0] + offsetX;
        num++;
        path.lineTo(x, y);
        break;
      case 'v':
        y = num[0] + offsetY;
        num++;
        path.lineTo(x, y);
        break;
      case 'c': {
        if(end - num < 6) { num = end; break; }
        Point c1(num[0] + offsetX, num[1] + offsetY);
        Point c2(num[2] + offsetX, num[3] + offsetY);
        Point e(num[4] + offsetX, num[5] + offsetY);
        num += 6;
        path.cubicTo(c1, c2, e);
        ctrlPt = c2;
        x = e.x;
        y = e.y;
        break;
      }
      case 's': {
        if(end - num < 4) { num = end; break; }
        Point c1;
        if(lastMode == 'c' || lastMode == 'C' || lastMode == 's' || lastMode == 'S')
          c1 = Point(2*x - ctrlPt.x, 2*y - ctrlPt.y);
        else
          c1 = Point(x, y);
        Point c2(num[0] + offsetX, num[1] + offsetY);
        Point e(num[2] + offsetX, num[3] + offsetY);
        num += 4;
        path.cubicTo(c1, c2, e);
        ctrlPt = c2;
        x = e.x;
        y = e.y;
        break;
      }
      case 'q': {
        if(end - num < 4) { num = end; break; }
        Point c(num[0] + offsetX, num[1] + offsetY);
        Point e(num[2] + offsetX, num[3] + offsetY);
        num += 4;
        path.quadTo(c, e);
        ctrlPt = c;
        x = e.x;
        y = e.y;
        break;
      }
      case 't': {
        if(end - num < 2) { num = end; break; }
        Point e(num[0] + offsetX, num[1] + offsetY);
        num += 2;
        Point c;
        if(lastMode == 'q' || lastMode == 'Q' || lastMode == 't' || lastMode == 'T')
          c = Point(2*x - ctrlPt.x, 2*y - ctrlPt.y);
        else
          c = Point(x, y);
        path.quadTo(c, e);
        ctrlPt = c;
        x = e.x;
        y = e.y;
        break;
      }
      case 'a': {
        if(end - num < 7) { num = end; break; }
        real rx = (*num++);
        real ry = (*num++);
        real xAxisRotation = (*num++);
        real largeArcFlag  = (*num++);
        real sweepFlag = (*num++);
        real ex = (*num++) + offsetX;
        real ey = (*num++) + offsetY;
        pathArc(path, rx, ry, xAxisRotation, int(largeArcFlag), int(sweepFlag), ex, ey, x, y);
        x = ex;
        y = ey;
        break;
      }
      default:
        return false;
      }
      lastMode = pathElem;
    }
  }
  return true;
}

SvgGlyph* SvgParser::createGlyphNode()
{
  const char* unicode = useAttribute("unicode");  // this will be missing (empty) for <missing-glyph> element
  const char* glyphname = useAttribute("glyph-name");
  real hadv = toReal(useAttribute("horiz-adv-x"), -1);
  StringRef pathd = useAttribute("d");

  SvgGlyph* glyph = new SvgGlyph(glyphname, unicode, hadv);
  parsePathData(pathd, glyph->m_path, this->numberList);
  return glyph;
}

SvgNode* SvgParser::createCircleNode()
{
  real cx = lengthToPx(useAttribute("cx"), 0);
  real cy = lengthToPx(useAttribute("cy"), 0);
  real r = lengthToPx(useAttribute("r"), 0);
  return new SvgPath(Path2D().addEllipse(cx, cy, r, r), SvgNode::CIRCLE);
}

SvgNode* SvgParser::createEllipseNode()
{
  real cx = lengthToPx(useAttribute("cx"), 0);
  real cy = lengthToPx(useAttribute("cy"), 0);
  real rx = lengthToPx(useAttribute("rx"), 0);
  real ry = lengthToPx(useAttribute("ry"), 0);
  return new SvgPath(Path2D().addEllipse(cx, cy, rx, ry), SvgNode::ELLIPSE);
}

SvgFont* SvgParser::createFontNode()
{
  real hadv = toReal(useAttribute("horiz-adv-x"), 0);
  SvgFont* font = new SvgFont(hadv);
  //font->setFamilyName(useId());  -- can't find any ref that id should be fallback for font-family
  return font;
}

void SvgParser::parseFontFaceNode(SvgFont* font)
{
  StringRef name = useAttribute("font-family");
  real unitsPerEm = toReal(useAttribute("units-per-em"), 1000);

  if(!name.isEmpty()) {
    font->setFamilyName(name.toString().c_str());
    m_doc->addSvgFont(font);
  }
  font->setUnitsPerEm(unitsPerEm);
}

void SvgParser::parseFontKerning(SvgFont* font)
{
  std::string g1 = useAttribute("g1");
  std::string g2 = useAttribute("g2");
  const char* u1 = useAttribute("u1");
  const char* u2 = useAttribute("u2");
  real k = lengthToPx(useAttribute("k"), 0);
  // svg spec says glyph names are <name>s and says that <name>s cannot contain whitespace, so remove
  // whitespace from g1, g2 to make searching for hit easier
  g1.erase(std::remove(g1.begin(), g1.end(), ' '), g1.end());
  g2.erase(std::remove(g2.begin(), g2.end(), ' '), g2.end());
  font->m_kerning.push_back({g1.c_str(), g2.c_str(), u1, u2, k});
}

SvgNode* SvgParser::createGNode()
{
  return new SvgG();
}

SvgNode* SvgParser::createANode()
{
  return new SvgG(SvgNode::A);
}

SvgNode* SvgParser::createDefsNode()
{
  return new SvgDefs();
}

SvgNode* SvgParser::createImageNode()
{
  const char* target = useHref();
  StringRef targetref = StringRef(target).trimmed();
  if(targetref.size() > 4 && strncasecmp(targetref.end() - 4, ".svg", 4) == 0) {
    nodeAttributes.emplace_back("xlink:href", target);
    return createUseNode();
  }

  real x = lengthToPx(useAttribute("x"), 0);
  real y = lengthToPx(useAttribute("y"), 0);
  real w = lengthToPx(useAttribute("width"), 0);
  real h = lengthToPx(useAttribute("height"), 0);

  Image image(0,0);
  if(targetref.startsWith("data")) {
    while(!targetref.isEmpty() && !targetref.startsWith("base64,"))
      ++targetref;
    if(!targetref.isEmpty()) {
      size_t declen;
      unsigned char* dec64 = base64_decode(targetref.constData()+7, targetref.size()-7, NULL, &declen);
      image = Image::decodeBuffer(dec64, declen);
      free(dec64);
      if(image.width > 0 && image.height > 0)
        target = NULL;
    }
    else
      PLATFORM_LOG("Unrecognized inline image format!\n");
  }
  else if(!targetref.isEmpty()) {
    std::vector<unsigned char> buff;
    if(readFile(&buff, toAbsPath(targetref).c_str()))
      image = Image::decodeBuffer(buff.data(), buff.size());
  }
  return new SvgImage(std::move(image), Rect::ltwh(x, y, w, h), target);
}

SvgNode* SvgParser::createLineNode()
{
  real x1 = lengthToPx(useAttribute("x1"), 0);
  real y1 = lengthToPx(useAttribute("y1"), 0);
  real x2 = lengthToPx(useAttribute("x2"), 0);
  real y2 = lengthToPx(useAttribute("y2"), 0);
  return new SvgPath(Path2D().addLine(Point(x1, y1), Point(x2, y2)), SvgNode::LINE);
}

void SvgParser::parseBaseGradient(SvgGradient* gradNode)
{
  StringRef link = useHref();
  StringRef trans = useAttribute("gradientTransform");
  StringRef spread = useAttribute("spreadMethod");
  StringRef units = useAttribute("gradientUnits");

  Transform2D tf;
  if(!link.isEmpty()) {
    SvgNode* prop = m_doc->namedNode(link.toString().c_str());
    if(prop && prop->type() == SvgNode::GRADIENT) {
      SvgGradient* inherited = static_cast<SvgGradient*>(prop);
      gradNode->setStopLink(inherited);
      // TODO: need to better support deferred resolution of link
      // TODO: all attributes are subject to inheritance, not just stops and matrix
      tf = inherited->getTransform();
    }
  }

  if(!trans.isEmpty()) {
    tf = parseTransformMatrix(trans);
    gradNode->setTransform(tf);
  }
  else if(!tf.isIdentity())
    gradNode->setTransform(tf);

  Gradient& grad = gradNode->m_gradient;
  grad.setSpread(Gradient::Spread(parseEnum(spread, {{"pad", Gradient::PadSpread},
        {"reflect", Gradient::ReflectSpread}, {"repeat", Gradient::RepeatSpread}}, Gradient::PadSpread)));
  grad.setCoordinateMode(
    units == "userSpaceOnUse" ? Gradient::userSpaceOnUseMode : Gradient::ObjectBoundingMode);
}

SvgNode* SvgParser::createStopNode()
{
  return new SvgGradientStop;
}

SvgNode* SvgParser::createLinearGradientNode()
{
  real x1 = lengthToPx(useAttribute("x1"), 0);
  real y1 = lengthToPx(useAttribute("y1"), 0);
  real x2 = lengthToPx(useAttribute("x2"), 1);
  real y2 = lengthToPx(useAttribute("y2"), 0);

  SvgGradient* gradnode = new SvgGradient(Gradient::linear(x1, y1, x2, y2));
  parseBaseGradient(gradnode);
  return gradnode;
}

SvgNode* SvgParser::createRadialGradientNode()
{
  real cx = lengthToPx(useAttribute("cx"), 0.5);
  real cy = lengthToPx(useAttribute("cy"), 0.5);
  real r  = lengthToPx(useAttribute("r"), 0.5);
  real fx = lengthToPx(useAttribute("fx"), cx);
  real fy = lengthToPx(useAttribute("fy"), cy);

  SvgGradient* gradnode = new SvgGradient(Gradient::radial(cx, cy, r, fx, fy));
  parseBaseGradient(gradnode);
  return gradnode;
}

SvgNode* SvgParser::createPatternNode()
{
  real x = lengthToPx(useAttribute("x"), 0);
  real y = lengthToPx(useAttribute("y"), 0);
  real w = lengthToPx(useAttribute("width"), 0);
  real h = lengthToPx(useAttribute("height"), 0);
  StringRef pu = useAttribute("patternUnits");
  StringRef pcu = useAttribute("patternContentUnits");

  auto npu = pu == "userSpaceOnUse" ? SvgPattern::userSpaceOnUse : SvgPattern::objectBoundingBox;
  auto npcu = pcu == "objectBoundingBox" ? SvgPattern::objectBoundingBox : SvgPattern::userSpaceOnUse;
  return new SvgPattern(x, y, w, h, npu, npcu);
}

SvgNode* SvgParser::createPathNode()
{
  StringRef data = useAttribute("d");
  SvgPath* path = new SvgPath();
  parsePathData(data, path->m_path, this->numberList);
  return path;
}

SvgNode* SvgParser::createPolygonNode()
{
  StringRef spoints = useAttribute("points");
  std::vector<real>& points = parseNumbersList(spoints);
  SvgPath* path = new SvgPath(SvgNode::POLYGON);
  path->m_path.reserve(points.size()/2 + 1);
  for(size_t ii = 0; ii+1 < points.size(); ii += 2)
    path->m_path.addPoint(points[ii], points[ii+1]);
  path->m_path.closeSubpath();
  return path;
}

SvgNode* SvgParser::createPolylineNode()
{
  StringRef spoints = useAttribute("points");
  std::vector<real>& points = parseNumbersList(spoints);
  SvgPath* path = new SvgPath(SvgNode::POLYLINE);
  path->m_path.reserve(points.size()/2);
  for(size_t ii = 0; ii+1 < points.size(); ii += 2)
    path->m_path.addPoint(points[ii], points[ii+1]);
  return path;
}

SvgNode* SvgParser::createRectNode()
{
  real x = lengthToPx(useAttribute("x"), 0);
  real y = lengthToPx(useAttribute("y"), 0);
  real w = lengthToPx(useAttribute("width"), 0);
  real h = lengthToPx(useAttribute("height"), 0);

  StringRef srx = useAttribute("rx");
  StringRef sry = useAttribute("ry");
  real rx = lengthToPx(srx, 0);
  real ry = lengthToPx(sry, 0);
  if(sry.isEmpty()) ry = rx;
  else if(srx.isEmpty()) rx = ry;

  return new SvgRect(Rect::ltwh(x, y, w, h), rx, ry);
}

SvgDocument* SvgParser::createSvgDocumentNode()
{
  real x = lengthToPx(useAttribute("x"), 0);
  real y = lengthToPx(useAttribute("y"), 0);
  SvgLength w = parseLength(useAttribute("width"), SvgLength(100, SvgLength::PERCENT));
  SvgLength h = parseLength(useAttribute("height"), SvgLength(100, SvgLength::PERCENT));
  StringRef viewBoxStr = useAttribute("viewBox");
  StringRef par = useAttribute("preserveAspectRatio");
  // consume xmlns attributes
  useAttribute("xmlns");
  useAttribute("xmlns:xlink");

  SvgDocument* doc = new SvgDocument(x, y, w, h);
  std::vector<real>& viewBox = parseNumbersList(viewBoxStr);
  if(viewBox.size() == 4)
    doc->setViewBox(Rect::ltwh(viewBox[0], viewBox[1], viewBox[2], viewBox[3]));
  // preserveAspectRatio has many options, but we only support, effectively, xMidYMid (default) or none
  doc->setPreserveAspectRatio(par != "none");
  return doc;
}

SvgNode* SvgParser::createSymbolNode()
{
  return new SvgSymbol();
}

SvgNode* SvgParser::parseTspanNode(SvgTspan* node)
{
  // usual case is only a single value of x and y
  node->m_x = ::parseNumbersList(useAttribute("x"), 1);
  node->m_y = ::parseNumbersList(useAttribute("y"), 1);
  // allow units in x,y?
  //StringRef xstr(useAttribute("x"));
  //while(!xstr.isEmpty()) {
  //  real x = lengthToPx(parseLength(xstr));  -- change parseLength to advance StringRef
  //  if(!std::isnan(x)) node->m_x.push_back(x);
  //  xstr.trimL();  if(*xstr == ',') { ++xstr; xstr.trimL(); }
  //}
  return node;
}

SvgNode* SvgParser::createTextNode() { return parseTspanNode(new SvgText()); }
SvgNode* SvgParser::createTspanNode() { return parseTspanNode(new SvgTspan(true)); }

SvgNode* SvgParser::createUseNode()
{
  real x = lengthToPx(useAttribute("x"), 0);
  real y = lengthToPx(useAttribute("y"), 0);
  real w = lengthToPx(useAttribute("width"), 0);
  real h = lengthToPx(useAttribute("height"), 0);
  StringRef href = useHref();

  // according to spec, width=0 and/or height=0 disables rendering, but we're using 0 to indicate
  //  attribute not present ... maybe set display=none if width=0 or height=0 specified
  // if link does not start with '#', assume string before '#' (if present) is filename, in which case
  //  document is loaded now
  SvgNode* link = NULL;
  SvgDocument* doc = NULL;
  if(href.size() > 1 && href[0] != '#') {
    // safest way to prevent recursive include would be to enforce a max depth
    std::vector<StringRef> fileAndId = splitStringRef(href, '#');
    doc = SvgParser().parseFile(toAbsPath(fileAndId[0]).c_str());
    if(doc) {
      if(fileAndId.size() == 2 && !fileAndId[1].isEmpty())
        href = fileAndId[1];  //link = doc->namedNode(fileAndId[1].toString().c_str() + 1);
      else
        link = doc;
    }
  }
  // SvgUse takes ownership of doc
  return new SvgUse(Rect::ltwh(x, y, w, h), href.toString().c_str(), link, doc);
}

SvgNode* SvgParser::createNode(StringRef name)
{
  if(name.isEmpty())
    return NULL;

  StringRef ref(name + 1);
  switch(name[0]) {
  case 'a':
    if(ref.isEmpty()) return createANode();
    break;
  case 'c':
    if(ref == "ircle") return createCircleNode();
    break;
  case 'd':
    if(ref == "efs") return createDefsNode();
    break;
  case 'e':
    if(ref == "llipse") return createEllipseNode();
    break;
  case 'f':
    if(ref == "ont") return createFontNode();
    break;
  case 'g':
    if(ref.isEmpty()) return createGNode();
    break;
  case 'i':
    if(ref == "mage") return createImageNode();
    break;
  case 'l':
    if(ref == "ine") return createLineNode();
    if(ref == "inearGradient") return createLinearGradientNode();
    break;
  case 'p':
    if(ref == "ath") return createPathNode();
    if(ref == "attern") return createPatternNode();
    if(ref == "olygon") return createPolygonNode();
    if(ref == "olyline") return createPolylineNode();
    break;
  case 'r':
    if(ref == "ect") return createRectNode();
    if(ref == "adialGradient") return createRadialGradientNode();
    break;
  case 's':
    if(ref == "vg") return createSvgDocumentNode();
    if(ref == "ymbol") return createSymbolNode();
    break;
  case 't':
    if(ref == "ext") return createTextNode();
    if(ref == "span") return createTspanNode();
    break;
  case 'u':
    if(ref == "se") return createUseNode();
    break;
  default:
    break;
  }
  return NULL;
}

bool SvgParser::parseCoreNode(SvgNode* node)
{
  node->setXmlId(useId());
  node->setXmlClass(useAttribute("class"));
  const char* tfstr = useAttribute("transform");
  if(node->type() == SvgNode::GRADIENT) tfstr = useAttribute("gradientTransform");
  else if(node->type() == SvgNode::PATTERN) tfstr = useAttribute("patternTransform");
  if(tfstr && tfstr[0]) {
    Transform2D tf = parseTransformMatrix(tfstr);
    if(!tf.isIdentity())
      node->setTransform(tf);
  }
  return true;
}

const char* SvgParser::useAttribute(const char* name)
{
  // previously, we were moving the last attribute into the slot for the used attribute, but it is
  //  ridiculous to be reordering attributes due to an silly implementation detail
  for(auto& a : nodeAttributes) {
    if(a.name && strcmp(name, a.name) == 0) {
      a.name = NULL;
      numUsedAttributes++;
      return a.value;
    }
  }
  return "";
}

const char* SvgParser::useId()
{
  const char* id = useAttribute("id");
  const char* xmlid = useAttribute("xml:id");
  return (id && id[0]) ? id : xmlid;
}

const char* SvgParser::useHref()
{
  const char* href = useAttribute("href");
  const char* xmlhref = useAttribute("xlink:href");
  return (href && href[0]) ? href : xmlhref;
}

bool SvgParser::startElement(StringRef nodeName, const XmlStreamAttributes& attributes)
{
  nodeAttributes.clear();
  numUsedAttributes = 0;
  for(XmlStreamAttribute xmlattr = attributes.firstAttribute(); xmlattr; xmlattr = xmlattr.next())
    nodeAttributes.emplace_back(xmlattr.name(), xmlattr.value());

  m_states.emplace_back(m_states.back());
  // this is pretty hacky ... core issue is that lengths should be stored as lengths and resolved when drawn
  real fontsize = lengthToPx(attributes.value("font-size"), 0);  // do not consume attribute
  if(fontsize > 0)
    currState().emPx = fontsize;

  SvgNode* node = NULL;
  SvgNode* parent = m_nodes.empty() ? NULL : m_nodes.back();
  if(nodeName == "style") {
    StringRef styletype(useAttribute("type"));
    bool iscss = styletype.isEmpty() || styletype == "text/css";
    if(iscss)
      m_inStyle = true;
    return iscss;
  }
  else if(!m_doc) {
    if(nodeName != "svg")
      return false;
    m_doc = createSvgDocumentNode();
    node = m_doc;
  }
  else if(parent->type() == SvgNode::TEXT || parent->type() == SvgNode::TSPAN) {
    SvgTspan* textnode = static_cast<SvgTspan*>(parent);
    if(nodeName == "tbreak") {
      textnode->addText("\n");
      m_nodes.push_back(textnode);  // hack to handle m_nodes.pop() in endElement()
      return true;
    }
    else if(nodeName == "tspan") {
      node = createTspanNode();
      if(node)
        textnode->addTspan(static_cast<SvgTspan*>(node));
    }
  }
  else if(parent->type() == SvgNode::GRADIENT) {
    if(nodeName == "stop") {
      node = createStopNode();
      if(node)
        static_cast<SvgGradient*>(parent)->addStop(static_cast<SvgGradientStop*>(node));
    }
  }
  else if(parent->type() == SvgNode::FONT) {
    if(nodeName == "glyph") node = createGlyphNode();
    else if(nodeName == "missing-glyph") node = createGlyphNode();
    else if(nodeName == "font-face") parseFontFaceNode(static_cast<SvgFont*>(parent));
    else if(nodeName == "hkern") parseFontKerning(static_cast<SvgFont*>(parent));
    // for font-face, we return false (since node == NULL) so it's consumed (not saved) as unrecognized node
    if(node)
      static_cast<SvgFont*>(parent)->addGlyph(static_cast<SvgGlyph*>(node));
  }
  else if(parent->asContainerNode()) {
    node = createNode(nodeName);
    if(node)
      parent->asContainerNode()->addChild(node);
  }

  if(!node)
    return false;

  parseCoreNode(node);
  const char* stylestr = useAttribute("style");
  node->attrs.reserve(nodeAttributes.size() - numUsedAttributes);
  for(auto& a : nodeAttributes)
    processAttribute(node, SvgAttr::XMLSrc, a.name, a.value);  // processAttrbute checks !name
  // style string has higher precedence than CSS, but this is handled by SvgNode::setAttr()
  processStyleString(node, stylestr);
  //processCssStyle(node, m_stylesheet.get());
  m_nodes.push_back(node);
  return true;
}

bool SvgParser::endElement(StringRef nodeName)
{
  m_states.pop_back();
  if(m_inStyle) {
    if(nodeName == "style")
      m_inStyle = false;
    return true;
  }
  else
    m_nodes.pop_back();
  return true;
}

bool SvgParser::cdata(const char* str)
{
#ifndef NO_CSS
  if(m_inStyle) {
    m_stylesheet->parse_stylesheet(str);
    return true;
  }
#endif
  if(!m_nodes.empty()) {
    SvgNode* top = m_nodes.back();
    if(top->type() == SvgNode::TEXT || top->type() == SvgNode::TSPAN)
      static_cast<SvgTspan*>(top)->addText(str);
  }
  return true;
}

void SvgParser::parse(XmlStreamReader* const xml)
{
  bool done = false;
  while(!xml->atEnd() && !done) {
    // support XmlStreamReader already at start element (if not, no problem, will advance)
    switch(xml->tokenType()) {
    case XmlStreamReader::StartDocument:
      // this handles the case of the reader already opened on the <svg> node (instead of its parent)
      if(StringRef(xml->name()) != "svg")
        break;
    case XmlStreamReader::StartElement:
      if(!startElement(xml->name(), xml->attributes())) {
        if(!m_doc)
          return;
        m_states.pop_back();
        if(m_nodes.back()->asContainerNode())
          m_nodes.back()->asContainerNode()->addChild(new SvgXmlFragment(xml->readNodeAsFragment()));
        else
          delete xml->readNodeAsFragment();  // read node to skip even if we can't add it to doc
      }
      break;
    // EndDocument means atEnd() returns true, so this never runs - maybe move below loop?
    //case XmlStreamReader::EndDocument:
    //    if(strcmp(xml->name(), "svg") != 0)
    //        break;
    case XmlStreamReader::EndElement:
      done = m_nodes.empty();
      if(!done) {
        endElement(xml->name());
        // save a copy of style node as fragment to preserve it
        if(StringRef(xml->name()) == "style" && m_nodes.back()->asContainerNode())
          m_nodes.back()->asContainerNode()->addChild(new SvgXmlFragment(xml->readNodeAsFragment()));
      }
      break;
    case XmlStreamReader::CData:
      cdata(xml->text());
      break;
    case XmlStreamReader::ProcessingInstruction:
    case XmlStreamReader::Comment:
      // should we support <?xml-stylesheet type="text/css" href="style.css"?> ?
      if(m_nodes.back()->asContainerNode())
        m_nodes.back()->asContainerNode()->addChild(new SvgXmlFragment(xml->readNodeAsFragment()));
      break;
    default:
      break;
    }
    if(!done)
      xml->readNext();
  }
#ifndef NO_DYNAMIC_STYLE
  if(m_doc && !m_stylesheet->rules().empty()) {
    m_stylesheet->sort_rules();
    m_doc->setStylesheet(m_stylesheet.release());
    m_doc->restyle();
  }
#endif
  m_hasErrors = xml->parseStatus() != 0;
}

SvgDocument* SvgParser::parseXml(XmlStreamReader* reader)
{
  m_states.emplace_back();
  parse(reader);
  m_states.clear();
  return m_doc;
}

// parse a document fragment
SvgDocument* SvgParser::parseXmlFragment(XmlStreamReader* reader)
{
  m_states.emplace_back();
  startElement("svg", XmlStreamAttributes());
  parse(reader);
  endElement("svg");
  m_states.clear();
  return m_doc;
}

std::function<std::istream*(const char*)> SvgParser::openStream;

SvgDocument* SvgParser::parseFile(const char* filename, unsigned int opts)
{
  ASSERT(filename && filename[0] && "filename cannot be empty!");
  std::unique_ptr<std::istream> ifs(openStream ? openStream(filename) : new std::ifstream(PLATFORM_STR(filename)));
  if(!*ifs) {
    PLATFORM_LOG("Cannot open file '%s'\n", filename);
    return NULL;
  }
  m_fileName = filename;
  return parseStream(*ifs, opts);
}

SvgDocument* SvgParser::parseStream(std::istream& strm, unsigned int opts)
{
  XmlStreamReader xml(strm, opts);
  return parseXml(&xml);
}

SvgDocument* SvgParser::parseString(const char* data, int len, unsigned int opts)
{
  XmlStreamReader xml(data, len > 0 ? len : strlen(data), opts);
  return parseXml(&xml);
}

SvgDocument* SvgParser::parseFragment(const char* data, int len, unsigned int opts)
{
  XmlStreamReader xml(data, len > 0 ? len : strlen(data), opts);
  return parseXmlFragment(&xml);
}

SvgParser::SvgParser()
{
  m_nodes.reserve(32);
  m_states.reserve(32);
  numberList.reserve(4096);
  m_stylesheet.reset(new SvgCssStylesheet);
}
