// Refs:
// - http://www.vulcanware.com/cpp_pdf/
// - https://github.com/AndreRenaud/PDFGen
// - https://github.com/linhdq/Test/blob/master/hocr2pdf/src/codecs/pdf.cc
// - https://github.com/bluegum/PegDF/blob/master/src/pdfwrite.c
// - https://github.com/sciapp/gr/blob/master/lib/gks/pdf.c
// PDF validators:
// - https://www.pdf-online.com/osa/repair.aspx seemed best

/************************************************************
* PDF page markup operators:
 * b    closepath, fill,and stroke path.
 * B    fill and stroke path.
 * b*   closepath, eofill,and stroke path.
 * B*   eofill and stroke path.
 * BI   begin image.
 * BMC  begin marked content.
 * BT   begin text object.
 * BX   begin section allowing undefined operators.
 * c    curveto.
 * cm   concat. Concatenates the matrix to the current transform.
 * cs   setcolorspace for fill.
 * CS   setcolorspace for stroke.
 * d    setdash.
 * Do   execute the named XObject.
 * DP   mark a place in the content stream, with a dictionary.
 * EI   end image.
 * EMC  end marked content.
 * ET   end text object.
 * EX   end section that allows undefined operators.
 * f    fill path.
 * f*   eofill Even/odd fill path.
 * g    setgray (fill).
 * G    setgray (stroke).
 * gs   set parameters in the extended graphics state.
 * h    closepath.
 * i    setflat.
 * ID   begin image data.
 * j    setlinejoin.
 * J    setlinecap.
 * k    setcmykcolor (fill).
 * K    setcmykcolor (stroke).
 * l    lineto.
 * m    moveto.
 * M    setmiterlimit.
 * n    end path without fill or stroke.
 * q    save graphics state.
 * Q    restore graphics state.
 * re   rectangle.
 * rg   setrgbcolor (fill).
 * RG   setrgbcolor (stroke).
 * s    closepath and stroke path.
 * S    stroke path.
 * sc   setcolor (fill).
 * SC   setcolor (stroke).
 * sh   shfill (shaded fill).
 * Tc   set character spacing.
 * Td   move text current point.
 * TD   move text current point and set leading.
 * Tf   set font name and size.
 * Tj   show text.
 * TJ   show text, allowing individual character positioning.
 * TL   set leading.
 * Tm   set text matrix.
 * Tr   set text rendering mode.
 * Ts   set super/subscripting text rise.
 * Tw   set word spacing.
 * Tz   set horizontal scaling.
 * T*   move to start of next line.
 * v    curveto.
 * w    setlinewidth.
 * W    clip.
 * y    curveto.
 ************************************************************/

#include <iostream>
#include <sstream>
#include "pdfwriter.h"
#include "miniz/miniz.h"
#include "svgstyleparser.h"
#include "ulib/platformutil.h"

// fonts
const char* PdfWriter::FONTS[] =
{
   "Courier",
   "Courier-Bold",
   "Courier-Oblique",
   "Courier-BoldOblique",
   "Helvetica",
   "Helvetica-Bold",
   "Helvetica-Oblique",
   "Helvetica-BoldOblique",
   "Times-Roman",
   "Times-Bold",
   "Times-Italic",
   "Times-BoldItalic",
   "Symbol",
   "ZapfDingbats"
};

const int PdfWriter::N_FONTS = sizeof(PdfWriter::FONTS) / sizeof(PdfWriter::FONTS[0]);

static constexpr SvgEnumVal fontFamily[] =
    {{"sans", PdfWriter::HELVETICA}, {"sans-serif", PdfWriter::HELVETICA},
     {"helvetica", PdfWriter::HELVETICA}, {"arial", PdfWriter::HELVETICA},
     {"serif", PdfWriter::TIMES}, {"times", PdfWriter::TIMES},
     {"monospace", PdfWriter::COURIER}, {"courier", PdfWriter::COURIER}};
static constexpr SvgEnumVal lineCap[] =
    {{"0 J", Painter::FlatCap}, {"1 J", Painter::RoundCap}, {"2 J", Painter::SquareCap}};
static constexpr SvgEnumVal lineJoin[] =
    {{"0 j", Painter::MiterJoin}, {"1 j", Painter::RoundJoin}, {"2 j", Painter::BevelJoin}};


PdfWriter::PdfWriter(int npages)
{
  // object ids should be consecutive to simplify
  idFontsBase = 1;
  idResources = idFontsBase + N_FONTS;
  idCatalog = idResources + 1;
  idPageList = idCatalog + 1;
  idPagesBase = idPageList + 1;
  idImagesBase = idPagesBase + npages;
}

void PdfWriter::write(std::ostream& out)
{
  ASSERT(int(pages.size()) == idImagesBase - idPagesBase);
  std::vector<long> offsets;
  int objId = idFontsBase;

  out << "%PDF-1.4" << "\n\n";  // PDF 1.4 needed for transparency
  // standard fonts ... always have to write these unless we move after page list
  for(int ii = 0; ii < N_FONTS; ++ii) {
    offsets.push_back(out.tellp());
    out << objId++ << " 0 obj"               << "\n" <<
           "<<"                              << "\n" <<
           "   /Type     /Font"              << "\n" <<
           "   /Subtype  /Type1"             << "\n" <<
           "   /BaseFont /" << FONTS[ii]     << "\n" <<
           "   /Encoding /WinAnsiEncoding"   << "\n" <<
           ">>"                              << "\n" <<
           "endobj"                          << "\n\n";
  }

  // Resources object (directory of images, fonts, graphics states)
  offsets.push_back(out.tellp());
  ASSERT(objId == idResources);
  out << objId++ << " 0 obj" << "\n" <<
         "<<"                    << "\n" <<
         "   /Font <<"           << "\n";
  // fonts
  for(int i = 0; i < N_FONTS; i ++)
    out << "      /F" << (i + 1) << " " << (i + idFontsBase) << " 0 R" << "\n";
  out << "   >>"  << "\n";
  // images
  if(!mEntries.empty()) {
    out << "   /XObject <<" << "\n";
    for(ImageEntry& entry : mEntries) {
      out << fstring("      /Img%d %d 0 R\n", entry.mObjectID, entry.mObjectID);
      //out << "      " << mEntries[i].mName << " " << mEntries[i].mObjectID << " 0 R" << "\n";
    }
    out << "   >>"  << "\n";
  }
  // hardcoded graphics states for transparency
  out << "  /ExtGState <<\n";
  for (int i = 0; i < 16; i++) {
    out << fstring("  /GS%d <</ca %f>>\n", i, i/15.0);  // fill alpha
    out << fstring("  /GS%d <</CA %f>>\n", i+16, i/15.0);  // stroke alpha
  }
  out << "  >>\n";
  // end resources
  out << ">>\nendobj\n\n";

  // Catalog (top-level document object)
  offsets.push_back(out.tellp());
  ASSERT(objId == idCatalog);
  out << objId++ << " 0 obj"             << "\n" <<
         "<<"                              << "\n" <<
         "   /Type /Catalog"               << "\n" <<
         "   /Pages " << idPageList << " 0 R" << "\n" <<
         ">>\nendobj\n\n";

  // Pages object
  ASSERT(objId == idPageList);
  offsets.push_back(out.tellp());
  out << objId++ << " 0 obj" << "\n" <<
         "<<"                << "\n" <<
         "   /Type /Pages"   << "\n";  // <<
  //"   /MediaBox [ 0 0 " << mWidth << " " << mHeight << " ]" << "\n";
  out << "   /Count " << pages.size() << "\n" <<
         "   /Kids [";
  for(int ii = 0; ii < int(pages.size()); ++ii)
    out << " " << (idPagesBase + ii) << " 0 R";
  out << " ]\n" << ">>\nendobj\n\n";

  // calculate size dependent object ids
  int idAnnotsBase = idImagesBase + int(mEntries.size());
  int nAnnots = 0;
  for(const Page& page : pages)
    nAnnots += page.annots.size();
  int annotId = idAnnotsBase;
  int idContentsBase = idAnnotsBase + nAnnots;

  // List of pages (separate from actual contents - see below)
  for(int ii = 0; ii < int(pages.size()); ++ii) {
    const Page& page = pages[ii];
    //ASSERT(out.str().size() == out.tellp());
    offsets.push_back(out.tellp());
    ASSERT(objId == idPagesBase + ii);
    out << objId++ << " 0 obj"                        << "\n" <<
           "<<"                                      << "\n" <<
           "   /Type /Page"                          << "\n" <<
           "   /Parent " << idPageList << " 0 R"        << "\n" <<
           "   /MediaBox [ 0 0 " << page.width << " " << page.height << " ]\n" <<
           "   /Contents " << (idContentsBase + ii) << " 0 R" << "\n" <<
           "   /Resources " << idResources << " 0 R" << "\n";
    if(!page.annots.empty()) {
      out << "   /Annots [ ";
      for(int jj = 0; jj < int(page.annots.size()); ++jj)
        out << annotId++ << " 0 R ";
      out << "]\n";
    }
    out << ">>\nendobj\n\n";
  }

  // Image data
  ASSERT(objId == idImagesBase);
  for(ImageEntry& entry : mEntries) {
    offsets.push_back(out.tellp());
    ASSERT(objId == entry.mObjectID);
    out << objId++ << " 0 obj" << "\n"
        << entry.header << "\n"
        << "stream" << "\n";
    out.write((char*)entry.data.data(), entry.data.size());
    out << "\n"  // this newline is not included in length
        << "endstream" << "\n"
        << "endobj" << "\n\n";
  }

  // Annotations (links)
  ASSERT(objId == idAnnotsBase);
  for(const Page& page : pages) {
    for(const std::string& annot : page.annots) {
      offsets.push_back(out.tellp());
      out << objId++ << " 0 obj\n<<\n" << annot << ">>\nendobj\n\n";
    }
  }

  // Page contents
  ASSERT(objId == idContentsBase);
  for(const Page& page : pages) {
    offsets.push_back(out.tellp());
    size_t size = page.content.size();
    out << objId++ << " 0 obj\n";
    if(compressionLevel > 0) {
      size_t dest_len = size + 4096;
      unsigned char* dest = new unsigned char[dest_len];
      mz_compress2(dest, (mz_ulong*)&dest_len, (unsigned char*)page.content.data(), size, compressionLevel);
      out << fstring("<< /Length %d /Filter /FlateDecode >>\nstream\n", dest_len);
      out.write((char*)dest, dest_len);
      delete[] dest;
    }
    else {
      out << fstring("<< /Length %d >>\nstream\n", size);
      out << page.content;
    }
    out << "\nendstream\nendobj\n\n";
  }

  // Xref section (byte offsets of each object in file)
  int xrefOffset = int(out.tellp());
  int theSize    = int(offsets.size() + 1);
  ASSERT(objId == theSize);
  out << "xref" << "\n" <<
         "0 " << theSize << "\n" <<
         "0000000000 65535 f \n";
  for(long offset : offsets)
    out << fstring("%010d 00000 n \n", int(offset));

  out << "trailer" << "\n" <<
         "<<"      << "\n" <<
         "   /Size " << theSize << "\n" <<
         "   /Root " << idCatalog << " 0 R" << "\n" <<
         ">>" << "\n" <<
         "startxref" << "\n" <<
         xrefOffset << "\n" << "%%EOF\n";
}

// note w, h are passed in Dim units, not pts!
void PdfWriter::newPage(real w, real h, real ptsPerDim)
{
  w *= ptsPerDim;  //std::ceil(w * ptsPerDim);
  h *= ptsPerDim;  //std::ceil(h * ptsPerDim);
  pages.emplace_back(w, h, currPage ? currPage->content.size() + 1024 : 1024);
  currPage = &pages.back();
  initialTransform = Transform2D(ptsPerDim, 0, 0, -ptsPerDim, 0, h);  // flip y
}

template<class... Args>
void PdfWriter::writef(const char* fmt, Args&&... args)
{
  constexpr int INITIAL_SIZE = 256;
  std::string buff(INITIAL_SIZE, '\0');
  // Try to snprintf into our buffer.
  // one potential optimization would be to pass length of zero if strlen(fmt) > 255
  int needed = stbsp_snprintf(&buff[0], INITIAL_SIZE, fmt, std::forward<Args>(args)...);
  if(needed >= 0) {
    if(needed > INITIAL_SIZE) {
      buff.resize(size_t(needed)+1);
      stbsp_snprintf(&buff[0], needed+1, fmt, std::forward<Args>(args)...);
    }
    buff.resize(size_t(needed));
  }
  currPage->content += buff;
  currPage->content += "\n";
}

void PdfWriter::drawNode(const SvgNode* node)
{
  if(!node || !node->isVisible())
    return;

  extraStates.reserve(32);
  extraStates.emplace_back();
  reset();
  if(node->parent())  // && !standalone
    applyParentStyle(node);
  draw(node);
  extraStates.clear();
}

// reset graphics state (used for drawing patterns)
void PdfWriter::reset()
{
  setTransform(initialTransform);
  const char* resetgs =
    "0 0 0 rg\n"  // fill=black
    "/GS15 gs\n"  // fill-opacity=1
    "/GS31 gs\n"  // stroke-opacity=1
    "1 w\n"       // stroke-width=1
    "0 J\n"       // flat cap
    "0 j\n";      // miter join
  writef(resetgs);
  extraState() = ExtraState();
  extraState().transform = initialTransform;  // PDF transform was reset to initialTransform, not identity!
}

void PdfWriter::save()
{
  currPage->content += "q\n";
}

void PdfWriter::restore()
{
  currPage->content += "Q\n";
}

void PdfWriter::draw(const SvgNode* node)
{
  save();
  extraStates.push_back(extraStates.back());
  applyStyle(node);
  switch(node->type()) {
    case SvgNode::RECT:   //[[fallthrough]]
    case SvgNode::PATH:   _draw(static_cast<const SvgPath*>(node));  break;
    case SvgNode::SYMBOL: //[[fallthrough]]
    case SvgNode::G:      _draw(static_cast<const SvgG*>(node));  break;
    case SvgNode::IMAGE:  _draw(static_cast<const SvgImage*>(node));  break;
    case SvgNode::USE:    _draw(static_cast<const SvgUse*>(node));  break;
    case SvgNode::TEXT:   _draw(static_cast<const SvgText*>(node));   break;
    case SvgNode::TSPAN:  _draw(static_cast<const SvgTspan*>(node));   break;
    case SvgNode::DOC:    _draw(static_cast<const SvgDocument*>(node)); break;
    //case SvgNode::CUSTOM: _draw(static_cast<const SvgCustomNode*>(node));  break;
    default: break;
  }
  extraStates.pop_back();
  restore();
}

void PdfWriter::setTransform(const Transform2D& tf)
{
  transform(extraState().transform.inverse() * tf);
  extraState().transform = tf;  // limit propagation of numerical errors
}

void PdfWriter::transform(const Transform2D& tf)
{
  if(tf.isIdentity())
    return;
  extraState().transform = extraState().transform * tf;
  writef("%f %f %f %f %f %f cm", tf.m[0], tf.m[1], tf.m[2], tf.m[3], tf.m[4], tf.m[5]);
}

void PdfWriter::translate(real dx, real dy)
{
  transform(Transform2D::translating(dx, dy));
}

void PdfWriter::applyParentStyle(const SvgNode* node)
{
  std::vector<SvgNode*> parentApplyStack;
  SvgNode* parent = node->parent();
  // <use> content bounds() are relative to the <use> node, as they can be included in
  //  multiple places in document, each with different bounds relative to document
  while(parent && parent->type() != SvgNode::USE) {
    parentApplyStack.push_back(parent);
    parent = parent->parent();
  }

  for(auto it = parentApplyStack.rbegin(); it != parentApplyStack.rend(); ++it)
    applyStyle(*it);
}

void PdfWriter::setFillColor(const Color& c)
{
  extraState().hasFill = c.alpha() > 0;
  if(c.alpha() > 0)
    writef("%.3f %.3f %.3f rg", c.red()/255.0f, c.green()/255.0f, c.blue()/255.0f);
}

void PdfWriter::setStrokeColor(const Color& c)
{
  extraState().hasStroke = c.alpha() > 0;
  if(c.alpha() > 0)
    writef("%.3f %.3f %.3f RG", c.red()/255.0f, c.green()/255.0f, c.blue()/255.0f);
}

void PdfWriter::applyStyle(const SvgNode* node)
{
  ExtraState& state = extraState();
  // PDF will include CSS styling, so restyle if needed
  // transform is not a presentation attribute
  if(node->hasTransform())
    transform(node->getTransform());

  if(node->type() == SvgNode::DOC)
    transform(static_cast<const SvgDocument*>(node)->viewBoxTransform());
  // transform content bounds always in local units; for drawing, patternTransform will be reapplied after
  //  possible object bbox transform
  else if(node->type() == SvgNode::PATTERN)
    setTransform(Transform2D());

  for(const SvgAttr& attr : node->attrs) {
    switch(attr.stdAttr()) {
    case SvgAttr::FILL:
      if(attr.valueIs(SvgAttr::ColorVal))
        setFillColor(attr.colorVal());
      else if(attr.valueIs(SvgAttr::StringVal)) {
        SvgNode* fillnode = node->getRefTarget(attr.stringVal());
        if(fillnode && fillnode->type() == SvgNode::PATTERN)
          state.fillPattern = static_cast<SvgPattern*>(fillnode);
        //else if(fillnode->type() == SvgNode::GRADIENT)
        //  p->setFillBrush(gradientBrush(static_cast<SvgGradient*>(fillnode), node));
      }
      else if(attr.valueIs(SvgAttr::IntVal) && attr.intVal() == SvgStyle::currentColor)
        setFillColor(state.currentColor);
      break;
    case SvgAttr::FILL_RULE:  state.fillRule = Path2D::FillRule(attr.intVal());  break;
    case SvgAttr::FILL_OPACITY:  writef("/GS%d gs", int(attr.floatVal()*15 + 0.5f));  break;
    case SvgAttr::STROKE:
      if(attr.valueIs(SvgAttr::ColorVal))
        setStrokeColor(attr.colorVal());
      else if(attr.valueIs(SvgAttr::StringVal)) {
        SvgNode* strokenode = node->getRefTarget(attr.stringVal());
        if(strokenode && strokenode->type() == SvgNode::PATTERN)
          state.strokePattern = static_cast<SvgPattern*>(strokenode);
        //else if(strokenode->type() == SvgNode::GRADIENT)
        //  p->setStrokeBrush(gradientBrush(static_cast<SvgGradient*>(strokenode), node));
      }
      else if(attr.valueIs(SvgAttr::IntVal) && attr.intVal() == SvgStyle::currentColor)
        setStrokeColor(state.currentColor);
      break;
    case SvgAttr::STROKE_OPACITY:  writef("/GS%d gs", 16 + int(attr.floatVal()*15 + 0.5f));  break;
    case SvgAttr::STROKE_WIDTH:  writef("%.3f w", attr.floatVal());  break;
    case SvgAttr::STROKE_LINECAP:  writef(enumToStr(attr.intVal(), lineCap));  break;
    case SvgAttr::STROKE_LINEJOIN:  writef(enumToStr(attr.intVal(), lineJoin));  break;
    case SvgAttr::STROKE_MITERLIMIT:  writef("%.3f M", attr.floatVal());  break;
    //case SvgAttr::VECTOR_EFFECT:
    //else if(attr.name == "vector-effect")
    //  p->setVectorEffect(Painter::VectorEffect(attr.intVal()));
    case SvgAttr::FONT_FAMILY:
      //if(attr.valueIs(SvgAttr::PtrVal))
      //  state.svgFont = static_cast<SvgFont*>(attr.ptrVal());
      {
        std::string fam(toLower(attr.stringVal()));
        StringRef s(fam);
        for(const SvgEnumVal& enumval : fontFamily) {
          if(s.startsWith(enumval.str))
            state.fontFamily = Font(enumval.val);
        }
      }
      break;
    case SvgAttr::FONT_SIZE:  state.fontSize = attr.floatVal();  break;
    case SvgAttr::FONT_STYLE:  state.fontStyle = Painter::FontStyle(attr.intVal());  break;
    //case SvgAttr::FONT_VARIANT:
    case SvgAttr::FONT_WEIGHT:
      if(attr.intVal() == SvgStyle::BolderFont)
        state.fontWeight = std::min(state.fontWeight + 100, 900);
      else if(attr.intVal() == SvgStyle::LighterFont)
        state.fontWeight = std::max(state.fontWeight - 100, 100);
      else
        state.fontWeight = attr.intVal();
      break;
    //case SvgAttr::OPACITY:  p->setOpacity(attr.floatVal() * p->opacity());
    //case SvgAttr::SHAPE_RENDERING:  p->setAntiAlias(attr.intVal() != SvgStyle::NoAntialias);
    //case SvgAttr::TEXT_ANCHOR:  extraState().textAnchor = attr.intVal();
    //case SvgAttr::COMP_OP:  p->setCompositionMode(Painter::CompositionMode(attr.intVal()));
    case SvgAttr::COLOR:  state.currentColor = attr.colorVal();  break;
    default:  break;
    }
  }

  //if(node->m_ext)
  //  node->ext()->applyStyle(this);
}

/*Brush PdfWriter::gradientBrush(const SvgGradient* gradnode, const SvgNode* dest)
{
  Gradient& grad = gradnode->gradient();
  if(grad.coordinateMode() == Gradient::ObjectBoundingMode && dest->cachedBounds().isValid()) {
    Rect localBBox = (p->getTransform().inverse() * initialTransform).mapRect(dest->cachedBounds());
    grad.setObjectBBox(localBBox);
  }
  else
    grad.setObjectBBox(Rect());
  Brush b(&grad);
  if(gradnode->hasTransform())
    b.setMatrix(gradnode->getTransform());
  return b;
}*/

void PdfWriter::drawChildren(const SvgContainerNode* node)
{
  for(const SvgNode* child : node->children()) {
    if(child->isVisible())
      draw(child);
  }
}

void PdfWriter::drawPattern(const SvgPattern* pattern, const Path2D* path)
{
  Rect dest = path->boundingRect();
  real destw = dest.width();
  real desth = dest.height();
  Rect cell = pattern->m_cell;

  Transform2D tfin = extraState().transform;
  save();
  extraStates.push_back(extraStates.back());
  // pattern gets styling from its parents, not the host node (i.e. not the node using the pattern)
  reset();
  applyParentStyle(pattern);
  applyStyle(pattern);  // this resets transform
  setTransform(tfin);
  // finally ready!
  if(pattern->m_patternUnits == SvgPattern::objectBoundingBox)
    cell = Rect::ltwh(cell.left*destw, cell.top*desth, cell.width()*destw, cell.height()*desth);
  if(pattern->m_patternContentUnits == SvgPattern::objectBoundingBox)
    transform(Transform2D(1/destw, 0, 0, 1/desth, -dest.left, -dest.top));
  if(pattern->hasTransform())
    transform(pattern->getTransform());

  real w = cell.width();
  real h = cell.height();
  real x0 = fmod(cell.left, w);
  real y0 = fmod(cell.top, h);
  x0 -= x0 > 0 ? w : 0;
  y0 -= y0 > 0 ? h : 0;
  real xmax = dest.right;
  real ymax = dest.bottom;

  // clip ruling to dest
  //p->clipRect(dest);
  for(real xoffset = x0; xoffset < xmax; xoffset += w) {
    for(real yoffset = y0; yoffset < ymax; yoffset += h) {
      translate(xoffset, yoffset);
      // standard specifies that pattern contents be clipped to unit cell
      ///p->clipRect(cell.toSize());
      drawChildren(pattern);
      translate(-xoffset, -yoffset);
    }
  }
  extraStates.pop_back();
  restore();
}

// this is the draw() that will be called for a nested <svg> node
void PdfWriter::_draw(const SvgDocument* doc)
{
  //p->clipRect(doc->m_viewBox.isValid() ? doc->m_viewBox : docViewport(doc));
  drawChildren(doc);
}

void PdfWriter::_draw(const SvgG* node)
{
  drawChildren(node);

  if(node->groupType == SvgNode::A || anyHref) {
    const char* href = NULL;
    if(!(href = node->getStringAttr("xlink:href", NULL)) && !(href = node->getStringAttr("href", NULL)))
      return;
    std::string astr;
    if(href[0] == '#') {
      if(!resolveLink)
        return;
      int pgnum = pages.size() - 1;  // search is done backwards from this page
      SvgNode* n = resolveLink(href, &pgnum);
      if(!n)
        return;
      // get target position in coordinate system of target page
      real ptsPerDim = initialTransform.m[0];
      real h = n->rootDocument()->height().px() * ptsPerDim;
      Transform2D tf(ptsPerDim, 0, 0, -ptsPerDim, 0, h);
      Point p = tf.map(n->bounds().origin());
      astr = fstring("/A << /Type /Action  /S /GoTo  /D [%d 0 R /XYZ %.3f %.3f 0] >>", pgnum + idPagesBase, p.x, p.y);
    }
    else
      astr = fstring("/A << /Type /Action  /S /URI  /URI (%s) >>", href);

    Rect r = initialTransform.mapRect(node->bounds());  // PDF rectangle: [ left bottom right top ]
    currPage->annots.push_back( fstring(
        "   /Type /Annot\n"
        "   /Subtype /Link\n"
        "   /Rect [%.3f %.3f %.3f %.3f]\n"
        "   /BS << /W 0 >>\n"  // border width - default is 1; set to 0 to show no border
        "   /F 4\n"
        "   %s\n", r.left, r.bottom, r.right, r.top, astr.c_str()) );
  }
}

#include "stb_image_write.h"

// stb_image_write actually only calls this once, passing entire image
static void stbi_write_png_idat(void* context, void* data, int size)
{
  auto v = static_cast<Image::EncodeBuff*>(context);
  auto d = static_cast<unsigned char*>(data);

  int skip = 41;  // skip first 41 bytes
  size -= skip + 16;  // also skip IDAT CRC32 and 12 byte IEND section
  d += skip;
  if(size > 0)
    v->insert(v->end(), d, d + size);
}

void PdfWriter::_draw(const SvgImage* node)
{
  //real scaledw = saveImageScaled * m_bounds.width();
  //real scaledh = saveImageScaled * m_bounds.height();
  // shrink image to save space if sufficient size change (and new size is not tiny)
  //if(saveImageScaled > 0 && (scaledw < 0.75*node->m_image.width || scaledh < 0.75*node->m_image.height)
  //    && m_bounds.width() > 10 && m_bounds.height() > 10) {
  //  node->m_image = node->m_image.scaled(int(scaledw + 0.5), int(scaledh + 0.5));
  //}

  int wpx = node->m_image.width;
  int hpx = node->m_image.height;
  int npx = wpx * hpx;

  int id = idImagesBase + int(mEntries.size());
  mEntries.emplace_back(id);
  ImageEntry& entry = mEntries.back();

  if(node->m_image.encoding == Image::JPEG && !node->m_image.hasTransparency()) {
    // JPEG
    entry.data = node->m_image.encodeJPEG();
    entry.header = fstring(
        "<<\n/Type /XObject\n/Name /Img%d\n"
        "/Subtype /Image\n/ColorSpace /DeviceRGB\n"
        "/Width %d\n/Height %d\n"
        "/BitsPerComponent 8\n/Filter /DCTDecode\n"
        "/Length %d\n>>", id, wpx, hpx, entry.data.size());
  }
  else {
    unsigned char* maskdata = new unsigned char[npx];
    unsigned char* rgbdata = new unsigned char[3*npx];
    unsigned char* rgbp = rgbdata;
    auto bytes = node->m_image.bytesOnce();
    const unsigned int* pixels = (const unsigned int*)bytes;
    bool hasalpha = false;
    for(int ii = 0; ii < npx; ++ii) {
      maskdata[ii] = pixels[ii] >> Color::SHIFT_A;
      hasalpha = hasalpha || maskdata[ii] < 255;
      *rgbp++ = (pixels[ii] >> Color::SHIFT_R) & 0xFF;
      *rgbp++ = (pixels[ii] >> Color::SHIFT_G) & 0xFF;
      *rgbp++ = (pixels[ii] >> Color::SHIFT_B) & 0xFF;
    }
    if(bytes != node->m_image.data) free(bytes);

    std::string smask;
    if(hasalpha) {
      int maskid = idImagesBase + int(mEntries.size());
      mEntries.emplace_back(maskid);
      ImageEntry& maskentry = mEntries.back();

      size_t dest_len = npx + 4096;
      maskentry.data.resize(dest_len);
      mz_compress2(maskentry.data.data(), (mz_ulong*)&dest_len, maskdata, npx, 6);  // level = 6
      maskentry.data.resize(dest_len);  // actual length of compressed data
      maskentry.header = fstring(
          "<<\n/Type /XObject\n/Name /ImgMask%d\n"
          "/Subtype /Image\n/ColorSpace /DeviceGray\n"
          "/Width %d\n/Height %d\n"
          "/BitsPerComponent 8\n/Filter /FlateDecode\n"
          "/Length %d\n>>", maskid, wpx, hpx, maskentry.data.size());

      smask = fstring("/SMask %d 0 R\n", maskid);
    }

    // PDF supports standard 24-bit (NOT 32-bit) PNG encoding w/ DecodeParms set as below; PNG preprocesses
    //  image data to improve compression of most images (esp. continuous tone images), although some images
    //  will actually be slightly bigger than w/ deflate alone
    stbi_write_png_to_func(&stbi_write_png_idat, &entry.data, wpx, hpx, 3, rgbdata, wpx*3);
    //size_t dest_len = 3*npx + 4096;
    //entry.data.resize(dest_len);
    //mz_compress2(entry.data.data(), &dest_len, rgbdata, 3*npx, 6);  // level = 6
    //entry.data.resize(dest_len);  // actual length of compressed data
    entry.header = fstring(
        "<<\n/Type /XObject\n/Name /Img%d\n"
        "/Subtype /Image\n/ColorSpace /DeviceRGB\n"
        "%s"  // SMask if present
        "/Width %d\n/Height %d\n"
        "/BitsPerComponent 8\n/Filter /FlateDecode\n"
        "/DecodeParms << /BitsPerComponent 8 /Predictor 15 /Columns %d /Colors 3 >>\n"
        "/Length %d\n>>", id, smask.c_str(), wpx, hpx, wpx, entry.data.size());

    delete[] maskdata;
    delete[] rgbdata;
  }

  // now add command to show image in content stream
  save();
  // note we need to flip y to undo global y flip
  Rect b = node->viewport();
  writef("%f 0 0 %f %f %f cm", b.width(), -b.height(), b.left, b.bottom);
  writef("/Img%d Do", id);
  restore();
}

void PdfWriter::_draw(const SvgPath* node)
{
  const Path2D& path = node->m_path;
  ExtraState& states = extraState();
  if(path.empty())
    return;

  bool hasStroke = states.hasStroke && !states.strokePattern;
  bool hasFill = states.hasFill && !states.fillPattern;
  if(states.fillPattern)
    drawPattern(states.fillPattern, &path);
  if(states.strokePattern)
    drawPattern(states.strokePattern, &path);
  if(!hasStroke && !hasFill)
    return;

  int start = -1;
  for(int ii = 0; ii < path.size(); ++ii) {
    if(path.command(ii) == Path2D::MoveTo) {
      if(start >= 0 && path.point(start) == path.point(ii-1))
        writef("h");
      writef("%.3f %.3f m", path.point(ii).x, path.point(ii).y);
      start = ii;
    }
    else if(path.command(ii) == Path2D::LineTo)
      writef("%.3f %.3f l", path.point(ii).x, path.point(ii).y);
    else if(path.command(ii) == Path2D::CubicTo) {
      writef("%.3f %.3f %.3f %.3f %.3f %.3f c", path.point(ii).x, path.point(ii).y,
          path.point(ii+1).x, path.point(ii+1).y, path.point(ii+2).x, path.point(ii+2).y);
      ii += 2;  // skip control points
    }
    else if(path.command(ii) == Path2D::QuadTo) {
      ASSERT(ii > 0 && "Path must begin with moveTo!");
      Point c1 = path.point(ii-1) + (2.0/3.0)*(path.point(ii) - path.point(ii-1));
      Point c2 = path.point(ii+1) + (2.0/3.0)*(path.point(ii) - path.point(ii+1));
      writef("%.3f %.3f %.3f %.3f %.3f %.3f c", c1.x, c1.y, c2.x, c2.y, path.point(ii+1).x, path.point(ii+1).y);
      ++ii;  // skip control point
    }
  }
  if(start >= 0 && path.point(start) == path.point(path.size()-1))
    writef("h");

  // clip path: op = (path.fillRule == Path2D::WindingFill) ? "W n\n" : "W* n\n";
  if(hasFill && hasStroke)
    writef(states.fillRule == Path2D::WindingFill ? "B" : "B*");
  else if(hasFill)
    writef(states.fillRule == Path2D::WindingFill ? "f" : "f*");
  else // stroke only
    writef("S");
}

static void setDocSizeForUse(SvgDocument* doc, const Rect& bbox)
{
  SvgLength w(bbox.width());
  SvgLength h(bbox.height());
  bool changed = false;
  if(w.value > 0) {
    changed = w != doc->m_width && (doc->m_width.isPercent() || doc->m_viewBox.isValid());
    doc->m_width = w;
  }
  if(h.value > 0) {
    changed = changed || (h != doc->m_height && (doc->m_height.isPercent() || doc->m_viewBox.isValid()));
    doc->m_height = h;
  }
  //if(changed)
  //  doc->_invalidateBounds(true);  // clear cached bounds but do not set as dirty!
}

void PdfWriter::_draw(const SvgUse* node)
{
  const SvgNode* target = node->target();
  if(!target)
    return;
  Rect m_bounds = node->viewport();
  translate(m_bounds.left, m_bounds.top);
  if(target->type() == SvgNode::DOC)
    setDocSizeForUse((SvgDocument*)target, m_bounds);
  draw(target);
  translate(-m_bounds.left, -m_bounds.top);
}

void PdfWriter::_draw(const SvgText* node)
{
  hasText = true;
  //p->setOpacity(p->opacity() * extraState().fillOpacity);
  writef("BT");
  //writef("BT\n%.3f %.3f Td", node->m_coord.x, node->m_coord.y);
  _draw(static_cast<const SvgTspan*>(node));
  writef("ET");
}

void PdfWriter::_draw(const SvgTspan* node)
{
  ExtraState& state = extraState();
  // for now, we don't support multiple coords for x,y
  if(!node->m_x.empty()) state.textPos.x = node->m_x[0];
  if(!node->m_y.empty()) state.textPos.y = node->m_y[0];

  if(!node->m_text.empty()) {
    // set position - this replaces previous text matrix but is combined w/ overall transform
    writef("1 0 0 -1 %.3f %.3f Tm", state.textPos.x, state.textPos.y);
    // set font
    int font = state.fontFamily;
    if(font <= TIMES) {
      if(state.fontWeight >= 700)
        font += 1;  // bold
      if(state.fontStyle != Painter::StyleNormal)
        font += 2;  // italic/oblique
    }
    writef("/F%d %d Tf", font, int(state.fontSize + 0.5f));

    std::string escText;
    escText.reserve(node->m_text.size());
    for(size_t ii = 0; ii < node->m_text.size(); ++ii) {
      if(node->m_text[ii] == '(' || node->m_text[ii] == ')')
        escText += "\\";
      escText += node->m_text[ii];
    }
    writef("(%s) Tj", escText.c_str());
  }
  else {
    for(SvgTspan* tspan : node->tspans()) {
      if(tspan->isLineBreak())
        writef("(\n) Tj");
      else
        _draw(tspan);
    }
  }
}
