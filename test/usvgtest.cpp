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

#define NANOVG_SW_IMPLEMENTATION
#include "nanovg.h"
#include "nanovg_sw.h"

// generated with xxd -i Roboto-Regular.ttf
#include "Roboto-Regular.inl"

int main(int argc, char* argv[])
{
  if(argc < 2) {
    PLATFORM_LOG("Usage: usvgtest <testfile.svg>\n");
    return -100;
  }
  const char* svgfile = argv[1];
  std::string refpngfile = std::string(svgfile, strlen(svgfile)-4) + "_ref.png";
  std::string outpngfile = std::string(svgfile, strlen(svgfile)-4) + "_out.png";
  std::string outsvgfile = std::string(svgfile, strlen(svgfile)-4) + "_out.svg";

  int res = 0;
  NVGcontext* nvgContext = nvgswCreate(NVG_AUTOW_DEFAULT | NVG_IMAGE_SRGB);
  Painter::sharedVg = nvgContext;
  Painter::loadFontMem("sans", Roboto_Regular_ttf, Roboto_Regular_ttf_len);

  SvgDocument* doc = SvgParser().parseFile(svgfile);

  Image image(doc->width().value, doc->height().value);
  Painter painter(&image);
  painter.beginFrame();
  painter.fillRect(painter.deviceRect, Color::WHITE);
  SvgPainter(&painter).drawNode(doc);  //, dirty);
  painter.endFrame();

  std::string refpngbuff;
  readFile(&refpngbuff, refpngfile.c_str());
  Image refpng = Image::decodeBuffer((const unsigned char*)refpngbuff.data(), refpngbuff.size());

  if(refpngbuff.empty())
    PLATFORM_LOG("Reference image %s not found\n", refpngfile.c_str());
  else if(refpng != image) {
    --res;
    PLATFORM_LOG("Failed: rendered image does not match %s\n", refpngfile.c_str());
    auto buff = image.encodePNG();
    FileStream pngout(outpngfile.c_str(), "wb");
    pngout.write(buff.data(), buff.size());
    pngout.close();
  }
  else
    PLATFORM_LOG("Rendered image matches %s\n", refpngfile.c_str());

  SvgWriter::DEBUG_CSS_STYLE = true;
  XmlStreamWriter xmlwriter;
  SvgWriter(xmlwriter).serialize(doc);
  xmlwriter.saveFile(outsvgfile.c_str());

  delete doc;
  return res;
}
