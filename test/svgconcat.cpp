#include "usvg/svgparser.h"
#include "usvg/svgwriter.h"

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


void cleanSvg(SvgNode* node)
{
  for(auto it = node->attrs.begin(); it != node->attrs.end(); ++it) {
    if(!it->stdAttr())
      it->setFlags(it->getFlags() | SvgAttr::NoSerialize);
  }

  SvgContainerNode* cnode = node->asContainerNode();
  if(cnode) {
    auto& children = cnode->children();
    for(auto it = children.begin(); it != children.end();) {
      if((*it)->type() == SvgNode::UNKNOWN || ((*it)->asContainerNode() && !(*it)->asContainerNode()->firstChild()))
        it = children.erase(it);
      else
        cleanSvg(*it++);
    }
  }
}

int main(int argc, char* argv[])
{
  SvgDocument* out = new SvgDocument;
  for(int ii = 1; ii < argc; ++ii) {
    SvgDocument* doc = SvgParser().parseFile(argv[ii]);

    cleanSvg(doc);
    doc->setXmlId(FSPath(argv[ii]).baseName().c_str());

    out->addChild(doc);
  }

  XmlStreamWriter xmlwriter;
  SvgWriter(xmlwriter).serialize(out);
  xmlwriter.saveFile("combined.svg");
  PLATFORM_LOG("Output written to ./combined.svg");
  return 0;
}
