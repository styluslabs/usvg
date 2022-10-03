#pragma once

#include <functional>
#include "svgnode.h"


class PdfWriter
{
public:
  struct Page
  {
    int width;
    int height;
    std::string content;
    std::list<std::string> annots;

    Page(int w, int h, size_t mincontent = 1024) : width(w), height(h) { content.reserve(mincontent); }
  };

  struct ImageEntry
  {
    int mObjectID;
    std::string header;
    Image::EncodeBuff data;

    ImageEntry(int id) : mObjectID(id) {}
    ImageEntry(const ImageEntry&) = delete;  // no copying
  };

  enum Font
  {
     NONE = 0,
     COURIER,
     COURIER_BOLD,
     COURIER_OBLIQUE,
     COURIER_BOLD_OBLIQUE,
     HELVETICA,
     HELVETICA_BOLD,
     HELVETICA_OBLIQUE,
     HELVETICA_BOLD_OBLIQUE,
     TIMES,
     TIMES_BOLD,
     TIMES_ITALIC,
     TIMES_BOLD_ITALIC,
     SYMBOL,
     ZAPF_DINGBATS
  };

  struct ExtraState
  {
    float fillOpacity = 1.0;
    float strokeOpacity = 1.0;
    SvgFont* svgFont = NULL;
    Painter::TextAlign textAnchor = Painter::AlignLeft;
    Path2D::FillRule fillRule = Path2D::WindingFill;
    //float strokeDashOffset = 0;
    SvgPattern* fillPattern = NULL;
    SvgPattern* strokePattern = NULL;
    Color currentColor = Color::NONE;

    Transform2D transform;
    Point textPos;
    Font fontFamily = HELVETICA;
    float fontSize = 12;
    int fontWeight = 400;
    Painter::FontStyle fontStyle = Painter::StyleNormal;
    bool hasFill = true;
    bool hasStroke = false;
  };

  std::vector<ExtraState> extraStates;
  ExtraState& extraState() { return extraStates.back(); }
  int compressionLevel = 2;
  bool anyHref = false;

  PdfWriter(int npages);
  void drawNode(const SvgNode* node);
  void newPage(real w, real h, real ptsPerDim);
  void write(std::ostream& out);

  std::function<SvgNode*(const char*, int*)> resolveLink;

  //enum { DEFAULT_WIDTH = 612, DEFAULT_HEIGHT = 792 };  8.5x11in
  static const char* FONTS[];
  static const int N_FONTS;

private:
  template<class... Args> void writef(const char* fmt, Args&&... args);

  void reset();
  void save();
  void restore();
  void setTransform(const Transform2D& tf);
  void transform(const Transform2D& tf);
  void translate(real dx, real dy);

  void applyParentStyle(const SvgNode* node);
  void setFillColor(const Color& c);
  void setStrokeColor(const Color& c);
  void applyStyle(const SvgNode* node);

  void draw(const SvgNode* node);
  void _draw(const SvgDocument* node);
  void _draw(const SvgG* node);
  void _draw(const SvgPath* node);
  void _draw(const SvgImage* node);
  void _draw(const SvgUse* node);
  void _draw(const SvgText* node);
  void _draw(const SvgTspan* node);
  //void _draw(const SvgCustomNode* node);
  void drawChildren(const SvgContainerNode* node);
  void drawPattern(const SvgPattern* pattern, const Path2D* path);
  void drawTspan(const SvgTspan* node);

  std::vector<Page> pages;
  Page* currPage = NULL;
  std::list<ImageEntry> mEntries;
  Transform2D initialTransform;
  bool hasText = false;
  int idFontsBase, idResources, idCatalog, idPageList, idPagesBase, idImagesBase;
};
