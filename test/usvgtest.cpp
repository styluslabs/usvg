#include "svgparser.h"
#include "svgpainter.h"
#include "svgwriter.h"

#define PLATFORMUTIL_IMPLEMENTATION
#include "ulib/platformutil.h"

#define STRINGUTIL_IMPLEMENTATION
#include "ulib/stringutil.h"

#define FILEUTIL_IMPLEMENTATION
#include "ulib/fileutil.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define FONTSTASH_IMPLEMENTATION
#include "fontstash.h"

#define NANOVG_SW_IMPLEMENTATION
#include "nanovg.h"
#include "nanovg_sw.h"

// generated with xxd -i Roboto-Regular.ttf
#include "Roboto-Regular.inl"

static Image paintDoc(SvgDocument* doc, int paintflags, const std::string& outpngfile)
{
  Image image(doc->width().value, doc->height().value);
  Painter painter(paintflags, &image);  //SRGB_AWARE  //| Painter::CACHE_IMAGES
  painter.beginFrame();
  painter.fillRect(painter.deviceRect, Color::WHITE);
  SvgPainter(&painter).drawNode(doc);  //, dirty);
  painter.endFrame();

  auto buff = image.encodePNG();
  FileStream pngout(outpngfile.c_str(), "wb");
  pngout.write(buff.data(), buff.size());
  pngout.close();
  return image;
}

int main(int argc, char* argv[])
{
  if(argc < 2) {
    PLATFORM_LOG("Usage: usvgtest <testfile.svg>\n");
    return -100;
  }
  const char* svgfile = argv[1];
  std::string filebase(svgfile, strlen(svgfile)-4);
  std::string refpngfile = filebase + "_ref.png";
  std::string outpngfile = filebase + "_out.png";
  std::string outsvgfile = filebase + "_out.svg";

  int res = 0;
  Painter::initFontStash(FONS_SUMMED);  //FONS_SDF
  Painter::loadFontMem("sans", Roboto_Regular_ttf, Roboto_Regular_ttf_len);

  SvgDocument* doc = SvgParser().parseFile(svgfile);

  Painter boundsPaint(Painter::PAINT_NULL);
  SvgPainter boundsCalc(&boundsPaint);
  doc->boundsCalculator = &boundsCalc;

  paintDoc(doc, Painter::PAINT_SW, filebase + "_xc_out.png");
  Image image = paintDoc(doc, Painter::PAINT_SW | Painter::SW_NO_XC, outpngfile);

  std::string refpngbuff;
  readFile(&refpngbuff, refpngfile.c_str());
  Image refpng = Image::decodeBuffer((const unsigned char*)refpngbuff.data(), refpngbuff.size());

  if(refpngbuff.empty())
    PLATFORM_LOG("Reference image %s not found\n", refpngfile.c_str());
  else if(refpng != image)
    PLATFORM_LOG("Failed: rendered image does not match %s\n", refpngfile.c_str());
  else
    PLATFORM_LOG("Rendered image matches %s\n", refpngfile.c_str());

  SvgWriter::DEBUG_CSS_STYLE = true;
  XmlStreamWriter xmlwriter;
  SvgWriter(xmlwriter).serialize(doc);
  xmlwriter.saveFile(outsvgfile.c_str());

  delete doc;
  return res;
}
