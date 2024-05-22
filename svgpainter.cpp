#include "svgpainter.h"
#include "svgstyleparser.h"


// perhaps this should be a static method passed a Painter* ?

void SvgPainter::drawNode(const SvgNode* node, const Rect& dirty)
{
  //if(!node || !node->isVisible()) return;  // draw() checks isVisible()
  p->save();
  initPainter();
  initialTransform = p->getTransform();
  initialTransformInv = initialTransform.inverse();  // only for SvgGui - remove if we find a better soln
  if(node->parent())  // apply parent style before setting dirty rect so dirty rect valid == drawing
    applyParentStyle(node);
  dirtyRect = initialTransformInv.mapRect(p->deviceRect);
  if(dirty.isValid())
    dirtyRect.rectIntersect(dirty);

  // TODO: this unnecessarily saves and restores state
  draw(node);

  extraStates.clear();
  p->restore();
}

Rect SvgPainter::nodeBounds(const SvgNode* node)
{
  ASSERT((p->createFlags & Painter::PAINT_MASK) == Painter::PAINT_NULL && "Cannot use same SvgPainter for drawing and bounds calc!");
  return bounds(node, true);
  //if(!node->isPaintable()) return;  -- just needed for <symbol> - reenable when problem appears again
  /*p->save();
  // sets default style on the painter if we are top level; might be better to let caller call initPainter()
  initPainter();
  p->setTransform(Transform2D());
  applyParentStyle(node);

  // TODO: this unecessarily saves and restores state
  Rect b = bounds(node);

  extraStates.clear();
  p->restore();
  return b;*/
}

std::vector<Rect> SvgPainter::glyphPositions(const SvgText* node)
{
  p->save();
  p->reset();
  initPainter();
  applyParentStyle(node, true);
  applyStyle(node, true);
  real lineh = 0;
  Rect r;
  std::vector<Rect> positions;
  drawTextTspans(node, Point(0,0), &lineh, &r, &positions);
  extraStates.clear();
  p->restore();
  return positions;
}

// really, we should either reset Painter except for transform and clip, or not set anything
void SvgPainter::initPainter()
{
  //p->reset();
  p->setMiterLimit(4);
  p->setFillBrush(Color::BLACK);
  p->setStrokeBrush(Color::NONE);
  // SVG spec says default font family can be choosen by user agent, so we will let Painter choose,
  //  which by default is the first font loaded; default size is medium = 12 pt
  p->setFontSize(12);
  p->setAntiAlias(true);

  extraStates.reserve(32);
  extraStates.emplace_back();
}

// dirty rect for changes to paint servers (gradients, patterns) and <use> targets: renderedBounds for these
//  are united with bounds of each referencing node, so a change will cause dirty rect to include all
//  referencing nodes; we assume that target appears before any references in SVG document tree; also note
//  that this doesn't work for <use> referencing external doc, but that's not a big deal

Rect SvgPainter::calcDirtyRect(const SvgNode* node)
{
  Rect dirty;
  if(node->type() == SvgNode::CUSTOM)
    dirty = node->ext()->dirtyRect();
  else if(node->m_dirty > SvgNode::CHILD_DIRTY)
    dirty = node->isVisible() ? node->bounds().rectUnion(node->m_renderedBounds) : node->m_renderedBounds;

  const SvgContainerNode* container = node->asContainerNode();
  if(container && node->m_dirty == SvgNode::CHILD_DIRTY && !dirty.isValid()) {
    dirty = container->m_removedBounds;
    // we don't descend into pattern node
    if(container->type() == SvgNode::PATTERN)
      dirty.rectUnion(node->m_renderedBounds);
    else {
      for(SvgNode* child : container->children()) {
        if(child->m_dirty != SvgNode::NOT_DIRTY && child->displayMode() != SvgNode::AbsoluteMode)  //&& child->isPaintable()
          dirty.rectUnion(calcDirtyRect(child));
      }
      if(container->type() == SvgNode::DOC)
        dirty.rectIntersect(static_cast<const SvgDocument*>(container)->bounds());
    }
  }
  // renderedBounds is now updated in draw() since, if, e.g., parent transform changes, children won't be
  //  marked dirty, but their renderedBounds can change; also m_dirty is cleared in second pass (clearDirty())
  return dirty;
}

void SvgPainter::clearDirty(const SvgNode* node)
{
  if(node->m_dirty != SvgNode::NOT_DIRTY) {
    node->m_dirty = SvgNode::NOT_DIRTY;
    const SvgContainerNode* container = node->asContainerNode();
    if(container) {
      container->m_removedBounds = Rect();
      for(SvgNode* child : container->children())
        clearDirty(child);
    }
  }
}

// this is only used for <defs> and <symbol> (outside <defs>)
static void clearRenderedBounds(const SvgNode* node)
{
  node->m_renderedBounds = Rect();
  const SvgContainerNode* container = node->asContainerNode();
  // don't descend into <pattern>; technically, we also don't have to descend into a subtree w/ no named nodes
  //  since nothing inside can be referenced, but not worth the hassle of trying to detect this case
  if(container && container->type() != SvgNode::PATTERN) {
    for(const SvgNode* child : container->children())
      clearRenderedBounds(child);
  }
}

// I don't like saving three different state structs for every node (SvgPainter::ExtraState, Painter::State,
//  and nanovg state), but I've at least made sure all have only scalar values.
// I think the best soln would be to pass flag to Painter::save() to skip saving Painter::State - we'd than
//  have to change how endPath() works (setCurrPath(), fillCurrPath(), strokeCurrPath()?) - but this would
//  probably be a small improvement vs. caching flattened nanovg paths
// Something like oldFill, oldStroke, etc. could easily be slower than memcpy of a few hundred bytes
// Also ... can we come up w/ a better naming convention than draw() / _draw()?
void SvgPainter::draw(const SvgNode* node)
{
  Rect bbox;
  if(!insideUse) {
    bbox = node->bounds();
    if(!dirtyRect.intersects(bbox))
      return;
  }
  else if(dirtyRect.isValid() && !dirtyRect.intersects(bounds(node)))
    return;

  p->save();
  extraStates.push_back(extraStates.back());
  applyStyle(node);

  // the most reasonable alternative to this would probably be double dispatch:
  //  virtual SvgNode::process(SvgProcessor*); virtual SvgProcessor::process(SvgPath*), etc.;
  // one advantage of current approach is that the control flow is entirely contained in one file and
  //  it's also closer to how things would work in plain C
  switch(node->type()) {
    case SvgNode::RECT:   //[[fallthrough]]
    case SvgNode::PATH:   _draw(static_cast<const SvgPath*>(node));  break;
    // <symbol> is excluded by isVisible() check in drawChildren; this is only hit from _draw(SvgUse*)
    case SvgNode::SYMBOL: //[[fallthrough]]
    // this is done solely to make stack traces shorter when debugging
    case SvgNode::G:      drawChildren(static_cast<const SvgContainerNode*>(node));  break;  //_draw(static_cast<const SvgG*>(node));
    case SvgNode::IMAGE:  _draw(static_cast<const SvgImage*>(node));  break;
    case SvgNode::USE:    _draw(static_cast<const SvgUse*>(node));  break;
    case SvgNode::TEXT:   _draw(static_cast<const SvgText*>(node));   break;
    case SvgNode::DOC:    _draw(static_cast<const SvgDocument*>(node));  break;
    case SvgNode::CUSTOM: _draw(static_cast<const SvgCustomNode*>(node));  break;
    default: break;
  }

  //p->setTransform(initialTransform);  p->fillRect(node->bounds(), Color(255,0,0,64));
  extraStates.pop_back();
  p->restore();
  if(!insideUse)
    node->m_renderedBounds = bbox;
}

void SvgPainter::drawChildren(const SvgContainerNode* node)
{
  for(const SvgNode* child : node->children()) {
    // moved here from draw() so that _draw(SvgUse*) works for, e.g,., <symbol>
    if(!child->isVisible()) {
      // should m_renderedBounds be updated in clearDirty() instead?
      // note that we don't need to clear renderedBounds for children of invisible node, since renderedBounds
      //  for children won't be accessed until after node is made visible and rendered again
      if(child->type() == SvgNode::DEFS || child->type() == SvgNode::SYMBOL)
        clearRenderedBounds(child);
      else
        child->m_renderedBounds = Rect();
    }
    else if(child->displayMode() != SvgNode::AbsoluteMode)
      draw(child);
  }
}

Rect SvgPainter::bounds(const SvgNode* node, bool parentstyle)
{
#ifndef DEBUG_CACHED_BOUNDS
  if(node->m_cachedBounds.isValid())
    return node->m_cachedBounds;
#endif
  p->save();
  if(parentstyle) {
    p->reset();  //p->setTransform(Transform2D());
    initPainter();
    applyParentStyle(node, true);
  }
  extraStates.push_back(extraStates.back());
  applyStyle(node, true);

  //item->selectFirst(".title-text")->node->setAttr("__debug", 1);
  //if(node->getIntAttr("__debug", 0) > 0) {
  //  Transform2D tf = p->getTransform();
  //  auto& paintstate = p->currState();
  //  auto fontsize = p->fontSize();
  //}

  Rect b;
  switch(node->type()) {
    case SvgNode::RECT:   //[[fallthrough]]
    case SvgNode::PATH:   b = _bounds(static_cast<const SvgPath*>(node));  break;
    // <symbol> is excluded by isVisible() check in childrenBounds; this is only hit from _bounds(SvgUse*)
    case SvgNode::SYMBOL: //[[fallthrough]]   // childrenBounds called directly just for shorter stack traces
    case SvgNode::G:      b = childrenBounds(static_cast<const SvgContainerNode*>(node));  break;
    case SvgNode::IMAGE:  b = _bounds(static_cast<const SvgImage*>(node));  break;
    case SvgNode::USE:    b = _bounds(static_cast<const SvgUse*>(node));  break;
    case SvgNode::TEXT:   b = _bounds(static_cast<const SvgText*>(node));   break;
    case SvgNode::DOC:    b = _bounds(static_cast<const SvgDocument*>(node));  break;
    case SvgNode::CUSTOM: b = _bounds(static_cast<const SvgCustomNode*>(node));  break;
    default: break;
  }

  extraStates.pop_back();
  p->restore();
  if(parentstyle)
    extraStates.pop_back();

#ifdef DEBUG_CACHED_BOUNDS
  if(node->m_cachedBounds.isValid() && b != node->m_cachedBounds) {
    PLATFORM_LOG("Cached bounds are wrong for node: %s!\n", SvgNode::nodePath(node).c_str());
    ASSERT(0 && "Cached bounds are wrong!\n");
  }
#endif
  node->m_cachedBounds = b;
  return b;
}

Rect SvgPainter::childrenBounds(const SvgContainerNode* node)
{
  Rect b;
  for(SvgNode* child : node->children()) {
    // nodes with display="none" are excluded from bounds
    if(child->isVisible() && child->displayMode() != SvgNode::AbsoluteMode)
      b.rectUnion(bounds(child));
  }
  return b;
}

void SvgPainter::applyParentStyle(const SvgNode* node, bool forBounds)
{
  std::vector<SvgNode*> parentApplyStack;
  SvgNode* parent = node->parent();
  // <use> content bounds() are relative to the <use> node, as they can be included in
  //  multiple places in document, each with different bounds relative to document
  while(parent) {  //&& parent->type() != SvgNode::USE) {
    parentApplyStack.push_back(parent);
    parent = parent->parent();
  }

  for(auto it = parentApplyStack.rbegin(); it != parentApplyStack.rend(); ++it)
    applyStyle(*it, forBounds);
}

void SvgPainter::applyStyle(const SvgNode* node, bool forBounds)
{
  ExtraState& state = extraState();
  // transform is not a presentation attribute
  if(node->hasTransform())
    p->transform(node->getTransform());

  if(node->type() == SvgNode::DOC)
    p->transform(static_cast<const SvgDocument*>(node)->viewBoxTransform());
  // transform content bounds always in local units; for drawing, patternTransform will be reapplied after
  //  possible object bbox transform
  else if(node->type() == SvgNode::PATTERN)
    p->setTransform(initialTransform);

  bool fillCurrColor = false, strokeCurrColor = false;
  for(const SvgAttr& attr : node->attrs) {
    // originally, we just had a bunch of if/else w/ string compares, but that was showing up in profiling
    // we could group together StdAttr values that don't affect bounds and skip attr based on forBounds
    switch(attr.stdAttr()) {
    case SvgAttr::COLOR:  state.currentColor = attr.colorVal();  break;
    case SvgAttr::COMP_OP:  p->setCompOp(Painter::CompOp(attr.intVal()));  break;
    case SvgAttr::FILL:
      if(attr.valueIs(SvgAttr::ColorVal))
        p->setFillBrush(attr.colorVal());
      else if(attr.valueIs(SvgAttr::StringVal)) {
        state.fillServer = node->getRefTarget(attr.stringVal());
        if(forBounds)
          p->setFillBrush(Color::RED);  // not really necessary since fill doesn't affect bounds
        else if(state.fillServer && state.fillServer->type() == SvgNode::GRADIENT)
          p->setFillBrush(gradientBrush(static_cast<const SvgGradient*>(state.fillServer), node));
      }
      fillCurrColor = attr.valueIs(SvgAttr::IntVal) && attr.intVal() == SvgStyle::currentColor;
      break;
    case SvgAttr::FILL_OPACITY:  state.fillOpacity = attr.floatVal();  break;
    case SvgAttr::FILL_RULE:  state.fillRule = Path2D::FillRule(attr.intVal());  break;
    case SvgAttr::FONT_FAMILY:  state.fontFamily = &attr;  resolveFont(node->document());  break;
    case SvgAttr::FONT_SIZE:  p->setFontSize(attr.floatVal());  break;
    case SvgAttr::FONT_STYLE:
      p->setFontStyle(Painter::FontStyle(attr.intVal()));
      resolveFont(node->document());
      break;
    case SvgAttr::FONT_VARIANT:  p->setCapitalization(Painter::FontCapitalization(attr.intVal()));  break;
    case SvgAttr::FONT_WEIGHT:
      if(attr.intVal() == SvgStyle::BolderFont)
        p->setFontWeight(std::min(p->fontWeight() + 100, 900));
      else if(attr.intVal() == SvgStyle::LighterFont)
        p->setFontWeight(std::max(p->fontWeight() - 100, 100));
      else
        p->setFontWeight(attr.intVal());
      resolveFont(node->document());
      break;
    case SvgAttr::OPACITY:  p->setOpacity(attr.floatVal() * p->opacity());  break;
    case SvgAttr::SHAPE_RENDERING:  p->setAntiAlias(attr.intVal() != SvgStyle::NoAntialias);  break;
    case SvgAttr::STROKE:
      if(attr.valueIs(SvgAttr::ColorVal))
        p->setStrokeBrush(attr.colorVal());
      else if(attr.valueIs(SvgAttr::StringVal)) {
        state.strokeServer = node->getRefTarget(attr.stringVal());
        if(forBounds)
          p->setStrokeBrush(Color::RED);
        else if(state.strokeServer && state.strokeServer->type() == SvgNode::GRADIENT)
          p->setStrokeBrush(gradientBrush(static_cast<const SvgGradient*>(state.strokeServer), node));
      }
      strokeCurrColor = attr.valueIs(SvgAttr::IntVal) && attr.intVal() == SvgStyle::currentColor;
      break;
    case SvgAttr::STROKE_LINECAP:  p->setStrokeCap(Painter::CapStyle(attr.intVal()));  break;
    case SvgAttr::STROKE_LINEJOIN:  p->setStrokeJoin(Painter::JoinStyle(attr.intVal()));  break;
    case SvgAttr::STROKE_MITERLIMIT:  p->setMiterLimit(attr.floatVal());  break;
    case SvgAttr::STROKE_OPACITY:  state.strokeOpacity = attr.floatVal();  break;
    case SvgAttr::STROKE_WIDTH:  p->setStrokeWidth(attr.floatVal());  break;
    case SvgAttr::STROKE_DASHARRAY:  state.dashServer = &attr;  break;
    case SvgAttr::STROKE_DASHOFFSET:  p->setDashOffset(attr.floatVal());  break;
    case SvgAttr::TEXT_ANCHOR:  state.textAnchor = attr.intVal();  break;
    case SvgAttr::VECTOR_EFFECT:  p->setVectorEffect(Painter::VectorEffect(attr.intVal()));  break;
    case SvgAttr::LETTER_SPACING:  p->setLetterSpacing(attr.floatVal());  break;
    default: break;
    }
  }
  // to support using and setting color on same node
  if(fillCurrColor)
    p->setFillBrush(state.currentColor);
  if(strokeCurrColor)
    p->setStrokeBrush(state.currentColor);

  if(node->hasExt())
    node->ext()->applyStyle(this);
}

Brush SvgPainter::gradientBrush(const SvgGradient* gradnode, const SvgNode* dest)
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
}

void SvgPainter::resolveFont(SvgDocument* doc)  //, const char* families)
{
  const char* families = extraState().fontFamily ? extraState().fontFamily->stringVal() : "";
  std::vector<StringRef> fams = splitStringRef(families, ",");
  for(StringRef fam : fams) {
    StringRef t = fam.trimmed();
    if(t.startsWith("'") || t.startsWith("\""))
      t = t.substr(1, t.size() - 2);
    std::string family = t.toString();

    SvgFont* svgfont = doc ? doc->svgFont(family.c_str(), p->fontWeight(), p->fontStyle()) : NULL;
    if(svgfont)
      extraState().svgFont = svgfont;
    else if(!p->setFontFamily(family.c_str()))
      continue;
    return;
  }
  // fallback to default or leave unchanged?
  //p->setFontFamily(p->defaultFontFamily.c_str());
}

void SvgPainter::drawPattern(const SvgPattern* pattern, const Path2D* path)
{
  Rect dest = path->boundingRect();
  real destw = dest.width();
  real desth = dest.height();
  Rect cell = pattern->m_cell;

  p->save();
  Transform2D tfin = p->getTransform();
  Rect clipin = p->getClipBounds();
  // pattern gets styling from its parents, not the host node (i.e. not the node using the pattern)
  p->reset();
  initPainter();  // NOTE: this will push fresh extraStates
  p->setTransform(tfin);  // this is a hack so that write-scale-down works
  applyParentStyle(pattern);
  applyStyle(pattern);  // this resets transform
  p->setTransform(tfin);
  p->setClipRect(clipin);
  // finally ready!
  if(pattern->m_patternUnits == SvgPattern::objectBoundingBox)
    cell = Rect::ltwh(cell.left*destw, cell.top*desth, cell.width()*destw, cell.height()*desth);
  if(pattern->m_patternContentUnits == SvgPattern::objectBoundingBox) {
    p->translate(-dest.left, -dest.top);
    p->scale(1/destw, 1/desth);
  }
  if(pattern->hasTransform())
    p->transform(pattern->getTransform());

  Rect localDirty = (p->getTransform().inverse() * initialTransform).mapRect(dirtyRect);
  real w = cell.width();
  real h = cell.height();
  real x0 = fmod(cell.left, w);
  real y0 = fmod(cell.top, h);
  x0 -= x0 > 0 ? w : 0;
  y0 -= y0 > 0 ? h : 0;
  x0 += w*std::max(0,int((localDirty.left - x0)/w));
  y0 += h*std::max(0,int((localDirty.top - y0)/h));
  real xmax = std::min(dest.right, localDirty.right);
  real ymax = std::min(dest.bottom, localDirty.bottom);

  Rect oldDirty = dirtyRect;
  // would be better to disable dirtyRect checking when drawing pattern
  dirtyRect = pattern->m_cell.toSize();
  // clip ruling to dest
  p->clipRect(dest);
  for(real xoffset = x0; xoffset < xmax; xoffset += w) {
    for(real yoffset = y0; yoffset < ymax; yoffset += h) {
      p->translate(xoffset, yoffset);
      // standard specifies that pattern contents be clipped to unit cell
      ///p->clipRect(cell.toSize());
      drawChildren(pattern);
      p->translate(-xoffset, -yoffset);
    }
  }
  dirtyRect = oldDirty;
  extraStates.pop_back();
  p->restore();
}

// this is the draw() that will be called for a nested <svg> node
void SvgPainter::_draw(const SvgDocument* doc)
{
  Rect oldDirty = dirtyRect;
  // note the use of _bounds() since applyStyle() already called by draw()
  if(dirtyRect.isValid())
    dirtyRect.rectIntersect(insideUse ? _bounds(doc) : doc->bounds());

#ifdef DEBUG_CLIPPING
  // draw clip rect instead of actually clipping ... should this be in uvg instead?  or just delete?
  p->save();
  p->setStrokeBrush(Color::RED);
  p->setStrokeWidth(4.0);
  p->setFillBrush(Color::NONE);
  p->drawRect(doc->m_viewBox.isValid() ? doc->m_viewBox : docViewport(doc));
  p->restore();
#else
  p->clipRect(doc->m_viewBox.isValid() ? doc->m_viewBox : doc->viewportRect());
#endif

  if(dirtyRect.isValid() || insideUse)
    drawChildren(doc);
  dirtyRect = oldDirty;
}

//void SvgPainter::_draw(const SvgG* node) { drawChildren(node); }

void SvgPainter::_draw(const SvgImage* node)
{
  p->drawImage(node->viewport(), node->m_image, node->srcRect);
}

void SvgPainter::_draw(const SvgPath* node)
{
  const Path2D& m_path = node->m_path;
  if(m_path.empty())
    return;
  ExtraState& state = extraState();
  const_cast<Path2D&>(m_path).setFillRule(state.fillRule);
  real oldOpacity = p->opacity();
  // we cannot use array stored in SvgAttr directly since it may not be aligned on 4-byte boundary (crashes on
  //  some platforms) - this will obviously be inefficient if dasharray is set on a <g> with many paths, but
  //  we expect this to be rare (we are doing this here instead of storing array in extraState because ... ?
  //  extra cost of having vector in extraStates?)
  std::vector<float> dashArray;
  if(state.dashServer) {
    dashArray.resize(state.dashServer->stringLen()/sizeof(float));
    memcpy(dashArray.data(), state.dashServer->stringVal(), state.dashServer->stringLen());
    p->setDashArray(dashArray.data());
  }
  bool hasPen = p->strokeWidth() > 0 && !p->strokeBrush().isNone();
  bool hasBrush = !p->fillBrush().isNone();
  bool hasFillPattern = state.fillServer && state.fillServer->type() == SvgNode::PATTERN;
  bool hasStrokePattern = state.strokeServer && state.strokeServer->type() == SvgNode::PATTERN;
  if(hasFillPattern || hasStrokePattern || (state.fillOpacity != state.strokeOpacity && hasPen && hasBrush)) {
    p->setOpacity(oldOpacity * state.fillOpacity);
    if(hasFillPattern)
      drawPattern(static_cast<const SvgPattern*>(state.fillServer), &m_path);
    else {
      Brush oldPen = p->strokeBrush();
      p->setStrokeBrush(Color::NONE);
      p->drawPath(m_path);
      p->setStrokeBrush(oldPen);
    }
    p->setOpacity(oldOpacity * state.strokeOpacity);
    if(hasStrokePattern)  // TODO: need to convert stroke to path or something
      drawPattern(static_cast<const SvgPattern*>(state.strokeServer), &m_path);
    else {
      Brush oldBrush = p->fillBrush();
      p->setFillBrush(Color::NONE);
      p->drawPath(m_path);
      p->setFillBrush(oldBrush);
    }
  }
  else if(hasPen || hasBrush) {
    p->setOpacity(oldOpacity * (hasPen ? state.strokeOpacity : state.fillOpacity));
    p->drawPath(m_path);
  }
  p->setOpacity(oldOpacity);  // I don't think we need this since p->restore() is called right after we return

  // TODO: just use bounds() (but w/ DEBUG_CACHED_BOUNDS disabled)?
  if(state.fillServer)
    state.fillServer->m_renderedBounds.rectUnion(node->cachedBounds());
  if(state.strokeServer)
    state.strokeServer->m_renderedBounds.rectUnion(node->cachedBounds());
}

void SvgPainter::_draw(const SvgUse* node)
{
  const SvgNode* target = node->target();
  if(!target)  // should not normally happen since node will have invalid bounds
    return;
  Rect m_bounds = node->viewport(); //m_bounds;
  Transform2D oldtf = p->getTransform();
  p->translate(m_bounds.origin());
  // previously, we were getting content bounds and scaling to fit m_bounds ... that was incorrect
  // SVG spec says <use> width/height are transferred to target if <svg> and otherwise are ignored ... in
  //  which case content will not be clipped to the bounds (although we could still consider clipping)!
  if(target->type() == SvgNode::DOC)
    ((SvgDocument*)target)->setUseSize(m_bounds.width(), m_bounds.height());
  // transform dirty rect into local coords since <use> contents bounds are in local coords (since parent = 0)
  Rect oldDirty = dirtyRect;

  Rect nodebounds = node->cachedBounds();
  if(insideUse++ == 0) {  // we only transform dirtyRect for the top-most <use>, so keep track!
    // you're just begging for agonizing bugs here ... should just always clear dirtyRect
    if(dirtyRect.contains(nodebounds) || nodebounds.width()*nodebounds.height() < 10000)
      dirtyRect = Rect();
    else {
      target->invalidateBounds(true);
      dirtyRect = initialTransform.mapRect(dirtyRect);
    }
  }
  draw(target);
  if(--insideUse == 0 && dirtyRect.isValid())
    target->invalidateBounds(true);

  dirtyRect = oldDirty;
  p->setTransform(oldtf);
  if(target->type() == SvgNode::DOC)
    ((SvgDocument*)target)->setUseSize(0, 0);
  target->m_renderedBounds.rectUnion(nodebounds);  //node->cachedBounds());
}

void SvgPainter::_draw(const SvgText* node)
{
  real lineh = 0;
  p->setOpacity(p->opacity() * extraState().fillOpacity);
  drawTextTspans(node, Point(0,0), &lineh);
}

void SvgPainter::_draw(const SvgCustomNode* node)
{
  if(node->hasExt())
    node->ext()->draw(this);
}

// bounds
// - tried adding a "boundsMode" to painter so we could just use draw() fns for bounds, but too complicated
//  since <svg> and <use> require special handling, among other reasons

Rect SvgPainter::_bounds(const SvgDocument* doc)
{
  // should we consider always calling childrenBounds() so that bounds of each child don't need to be
  //  recalculated separately?
  if(doc->m_viewBox.isValid())
    return p->getTransform().mapRect(doc->m_viewBox);
  // return content bounds if width or height is % but canvasRect is invalid
  if((doc->width().isPercent() || doc->height().isPercent()) && !doc->canvasRect().isValid())
    return childrenBounds(doc);

  return p->getTransform().mapRect(doc->viewportRect());
}

Rect SvgPainter::_bounds(const SvgImage* node)
{
  return p->getTransform().mapRect(node->viewport());
}

Rect SvgPainter::_bounds(const SvgPath* node)
{
  real strokewidth = p->strokeBrush().isNone() ? 0 : p->strokeWidth();
  //if(strokewidth > 0 && p->vectorEffect())
  //  strokewidth /= p->getAvgScale();
  // commented code is correct if stroke w/ non-uniform scaling is drawn properly ... but it's not currently!
  Transform2D tf = p->getTransform();
  if(!p->vectorEffect())
    strokewidth *= tf.avgScale();

  // no path is set for rect with zero width or height (to suppress drawing) but we still want bounds
  if(node->pathType() == SvgNode::RECT)
    return p->getTransform().mapRect(Rect(static_cast<const SvgRect*>(node)->m_rect).pad(strokewidth/2));
  if(node->m_path.empty())
    return Rect();
  // I think we can just map the bounding rect if there is no rotation ... probably should add some tests!
  Rect b = !tf.isRotating() ? tf.mapRect(node->m_path.boundingRect())
      : Path2D(node->m_path).transform(tf).boundingRect();
  //return b.pad(tf.xscale() * strokewidth/2, tf.yscale() * strokewidth/2);
  return b.pad(strokewidth/2);
}

Rect SvgPainter::_bounds(const SvgUse* node)
{
  const SvgNode* target = node->target();
  if(!target)
    return Rect();
  Rect m_bounds = node->viewport();  //m_bounds;
  if(target->type() == SvgNode::DOC)
    ((SvgDocument*)target)->setUseSize(m_bounds.width(), m_bounds.height());
  p->translate(m_bounds.origin());
  // cached bounds on contents can only be in local coords
  target->invalidateBounds(true);
  Rect b = bounds(target);
  target->invalidateBounds(true);
  p->translate(-m_bounds.origin());
  if(target->type() == SvgNode::DOC)
    ((SvgDocument*)target)->setUseSize(0, 0);
  return b;
}

Rect SvgPainter::_bounds(const SvgCustomNode* node)
{
  return node->hasExt() ? node->ext()->bounds(this) : Rect();
}

Rect SvgPainter::_bounds(const SvgText* node)
{
  real lineh = 0;
  Rect r;
  drawTextTspans(node, Point(0,0), &lineh, &r);
  return r;
}

// Our goal is not to support the full complexity of the svg text layout spec (SVG2 spec gives the full
//  algorithm), but try to support the most common cases (but if we do move text layout out of nanovg-2,
//  we could consider something closer to algorithm from SVG spec - filling out array of glyph positions)
// - we support individual glyph positioning (but not together with text-anchor in general), in anticipation
//  of possible use for (hidden) OCR text in Write docs (and generally keeping text together in a single
//  <text> even w/ complex positioning)
// - we support output of dvisvgm for LaTeX docs (using svg fonts)
// Note: in, e.g. <text y="0">A<tspan y="10">BC</tspan>D</text>, 'D' is placed at y=10, not y=0 - absolute
//  coordinates set a new current text position, which is not affected by end of <tspan>!

Point SvgPainter::drawTextText(const SvgTspan* node, Point pos, real* lineh, Rect* boundsOut, std::vector<Rect>* glyphPos)
{
  if(node->m_text.empty())
    return pos;

  Transform2D tf0 = p->getTransform();
  bool drawing = !boundsOut && !glyphPos;
  Painter::TextAlign anchor = extraState().textAnchor;
  std::string text = node->displayText();
  if(extraState().svgFont) {
    const SvgFont* font = extraState().svgFont;
    std::vector<const SvgGlyph*> glyphs = font->glyphsForText(text.c_str());

    real scale = p->fontSize()/font->m_unitsPerEm;
    // don't scale stroke-width
    real oldStrokeWidth = p->strokeWidth();
    p->setStrokeWidth(oldStrokeWidth/scale);
    // TODO: multiple x,y values won't be applied correctly if glyphs don't map one-to-one to chars
    for(size_t ii = 0; ii < glyphs.size(); ++ii) {
      if(!textX.empty()) { pos.x = textX.back();  textX.pop_back(); }
      if(!textY.empty()) { pos.y = textY.back();  textY.pop_back(); }

      if(!(anchor & Painter::AlignLeft)) {
        real w = font->horizAdvX(glyphs[ii]);
        // see if this is the first char of the final chunk
        if(textX.empty() && textY.empty()) {
          for(size_t jj = ii+1; jj < glyphs.size(); ++jj)
            w += font->horizAdvX(glyphs[jj]);
        }
        if(anchor & Painter::AlignHCenter)
          pos.x -= scale*w/2;
        else if(anchor & Painter::AlignRight)
          pos.x -= scale*w;
      }
      real kerning = ii+1 < glyphs.size() ? font->kerningForPair(glyphs[ii], glyphs[ii+1]) : 0;
      real dx = scale*(kerning + font->horizAdvX(glyphs[ii]) + p->letterSpacing());
      p->translate(pos);
      p->scale(scale, -scale);
      if(drawing)
        p->drawPath(glyphs[ii]->m_path);
      if(glyphPos)
        glyphPos->push_back(Rect::ltrb(pos.x, pos.y, pos.x+dx, pos.y));
      if(boundsOut && !glyphs[ii]->m_path.empty())
        boundsOut->rectUnion(p->getTransform().mapRect(glyphs[ii]->m_path.controlPointRect()));
      p->setTransform(tf0);
      pos.x += dx;
    }
    p->setStrokeWidth(oldStrokeWidth);  // restore stroke width
    if(lineh)
      *lineh = std::max(*lineh, 1.1 * p->fontSize());
  }
  else {
    p->setTextAlign(anchor);
    Rect tempBounds;
    uint32_t codepoint, utf8_state = UTF8_ACCEPT;
    const char* str = text.c_str();
    // ii counts codepoints
    for(size_t ii = 0; *str; ++ii) {
      if(!textX.empty()) { pos.x = textX.back();  textX.pop_back(); }
      if(!textY.empty()) { pos.y = textY.back();  textY.pop_back(); }

      if(textX.empty() && textY.empty() && textPath.empty()) {
        if(drawing)
          pos.x = p->drawText(pos.x, pos.y, str);  // draw the rest of the text
        if(glyphPos)  // boundsOut assumed to always be present if glyphPos is, so don't touch pos.x
          p->textGlyphPositions(pos.x, pos.y, str, NULL, glyphPos);
        if(boundsOut)  // += is OK because boundsOut excludes drawing
          pos.x += p->textBounds(pos.x, pos.y, str, NULL, &tempBounds);
        break;
      }
      // advance by one codepoint
      const char* start = str;
      while(*str && decode_utf8(&utf8_state, &codepoint, *str++) != UTF8_ACCEPT) {}

      if(!textPath.empty()) {
        Point normal;
        Point pt = textPath.positionAlongPath(pos.x - textPathOffset, &normal);
        if(pt.isNaN())
          continue;  // if position is not on path, glyph is not rendered
        p->translate(pt);
        p->rotate(atan2(-normal.x, normal.y));
        p->translate(-pos.x, 0);  // remove the x translation passed to drawText
      }

      if(drawing)
        pos.x = p->drawText(pos.x, pos.y, start, str);
      if(glyphPos)  // boundsOut assumed to always be present if glyphPos is, so don't touch pos.x
        p->textGlyphPositions(pos.x, pos.y, start, str, glyphPos);
      if(boundsOut)  // += is OK because boundsOut excludes drawing
        pos.x += p->textBounds(pos.x, pos.y, start, str, &tempBounds);

      pos.x += p->letterSpacing();  // letter spacing is not included if drawing glyphs separately
      if(!textPath.empty())
        p->setTransform(tf0);  // restore transform
    }
    if(lineh)
      *lineh = std::max(*lineh, p->textLineHeight());
    if(boundsOut && tempBounds.isValid())
      boundsOut->rectUnion(tempBounds);  //boundsOut->rectUnion(p->getTransform().mapRect(tempBounds));
  }
  return pos;
}

static std::vector<real> appendTextCoords(std::vector<real>& dest, const std::vector<real>& src)  //, size_t nchars)
{
  std::vector<real> olddest = dest;
  size_t n = src.size();  //std::min(nchars, src.size());
  if(dest.size() < n)
    dest.resize(n);
  size_t m = dest.size() - 1;
  for(size_t ii = 0; ii < n; ++ii)
    dest[m - ii] = src[ii];
  return olddest;
}

static bool resetsAnchor(const SvgTspan* node)
{
  return node->isLineBreak() || !node->m_x.empty() || !node->m_y.empty();
}

Point SvgPainter::drawTextTspans(const SvgTspan* node, Point pos, real* lineh, Rect* boundsOut, std::vector<Rect>* glyphPos)
{
  // <textPath> can be contained in <text> but not <tspan> (so can't be nested, since <text> can't be nested)
  if(node->type() == SvgNode::TEXTPATH) {
    const SvgTextPath* tpnode = static_cast<const SvgTextPath*>(node);
    SvgNode* target = tpnode->document()->namedNode(tpnode->href());
    if(!target || target->type() != SvgNode::PATH)
      return pos;
    Path2D* path = static_cast<SvgPath*>(target)->path();
    // note that we have to transform before flattening
    textPath = target->hasTransform() ? Path2D(*path).transform(target->getTransform()).toFlat() : path->toFlat();
    //textPath.transform(p->getTransform() * target->getTransform());
    textPathOffset = tpnode->startOffset();
  }

  // between displayText() and, e.g., (our incorrect handling of) svg font ligatures, too complicated to get
  //  correct number of coords that will be consumed, so just save and restore coords
  std::vector<real> textX0 = appendTextCoords(textX, node->m_x);
  std::vector<real> textY0 = appendTextCoords(textY, node->m_y);

  if(node->tspans().empty())
    pos = drawTextText(node, pos, lineh, boundsOut, glyphPos);
  else {
    ExtraState& states = extraState();
    Painter::TextAlign oldTextAnchor = states.textAnchor;
    const std::vector<SvgTspan*>& m_tspans = node->tspans();
    real x0 = textX.empty() ? pos.x : textX.back();
    for(size_t ii = 0; ii < m_tspans.size(); ++ii) {
      if(resetsAnchor(m_tspans[ii]))
        states.textAnchor = oldTextAnchor;
      if(m_tspans[ii]->isLineBreak()) {
        // ensure that glyphPos is same length as text
        if(glyphPos)
          glyphPos->push_back(Rect::centerwh(pos, 0, 0));
        pos.x = x0;
        pos.y += *lineh > 0 ? *lineh : p->textLineHeight();
        *lineh = 0;
      }
      else if(!m_tspans[ii]->isVisible())
        continue;
      else {
        // if this line contains multiple tspans and is not left aligned, we have to determine its
        //  width before we can draw
        if(!(states.textAnchor & Painter::AlignLeft)
            && m_tspans.size() > ii+1 && !resetsAnchor(m_tspans[ii+1])
            // any additional text coords will reset text position - we basically don't handle combinations
            //  of glyph coords and textAnchor != AlignLeft
            && textX.size() <= 1 && textY.size() <= 1) {
          if(m_tspans[ii]->text().empty())  // prevent empty tspan from consuming position
            continue;
          if(!textX.empty()) { pos.x = textX.back();  textX.pop_back(); }
          if(!textY.empty()) { pos.y = textY.back();  textY.pop_back(); }
          // bounds are always calculated with AlignLeft, then translated as needed
          states.textAnchor = (oldTextAnchor & ~Painter::HorzAlignMask) | Painter::AlignLeft;
          Rect dummy;  // needed to suppress drawing
          Point temppos = pos;
          for(size_t jj = ii; jj < m_tspans.size() && !resetsAnchor(m_tspans[jj]); ++jj)
            temppos = drawTextTspans(m_tspans[jj], temppos, NULL, &dummy, NULL);

          if(oldTextAnchor & Painter::AlignHCenter)
            pos.x = x0 - (temppos.x - x0)/2;
          else if(oldTextAnchor & Painter::AlignRight)
            pos.x = x0 - (temppos.x - x0);
        }
        const SvgTspan* tspan = m_tspans[ii];
        p->save();
        extraStates.push_back(extraStates.back());
        applyStyle(tspan);
        //extraState().tspanCharCount = 0;

        pos = drawTextTspans(tspan, pos, lineh, boundsOut, glyphPos);

        //int n = extraState().tspanCharCount;
        extraStates.pop_back();
        //extraState().tspanCharCount += n;
        p->restore();
      }
    }
  }

  // make sure textX,Y are cleared if done with <text>
  textX0.resize(node->type() == SvgNode::TEXT ? 0 : std::min(textX0.size(), textX.size()));
  textX.swap(textX0);
  textY0.resize(node->type() == SvgNode::TEXT ? 0 : std::min(textY0.size(), textY.size()));
  textY.swap(textY0);
  if(node->type() == SvgNode::TEXTPATH)
    textPath.clear();
  return pos;
}

#include <locale>         // std::wstring_convert
#include <codecvt>        // std::codecvt_utf8

// return text of node broken into lines of length <= maxWidth
// note that we don't use Painter::textBreakLines to support SVG fonts
std::string SvgPainter::breakText(const SvgText* node, real maxWidth)
{
  SvgDocument* root = node->rootDocument();
  SvgPainter* bounder = root && root->boundsCalculator ? root->boundsCalculator : SvgDocument::sharedBoundsCalc;
  std::vector<Rect> glyphpos = bounder->glyphPositions(node);
  std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> cv;
  std::string s8 = node->text();
  std::u32string s = cv.from_bytes(s8);
  if(s.size() != glyphpos.size())
    return s8;  // something went wrong
  std::u32string res;
  size_t ii = 0, lastSpace = 0, lineStart = 0;
  for(; ii < glyphpos.size(); ++ii) {
    if(s[ii] == '\n') {
      res.append(&s[lineStart], &s[ii + 1]);
      lineStart = ii + 1;
      lastSpace = lineStart;
    }
    else if(isspace(s[ii])) {
      lastSpace = ii;
    }
    else if(glyphpos[ii].right - glyphpos[lineStart].left > maxWidth) {
      res.append(&s[lineStart], &s[lastSpace]);  // exclude last space
      while(isspace(res.back())) res.pop_back();   // remove any additional spaces
      res.push_back('\n');
      lineStart = lastSpace + 1;  // glyphpos[origin].left;
      lastSpace = lineStart;
    }
  }
  if(lineStart < ii)
    res.append(&s[lineStart], &s[ii]);
  while(isspace(res.back())) res.pop_back();  // handles possible trailing newline
  return cv.to_bytes(res);
}

// replace text of textnode with truncated and ellipsized version w/ length < maxWidth
void SvgPainter::elideText(SvgText* textnode, real maxWidth)
{
  SvgDocument* root = textnode->rootDocument();
  SvgPainter* bounder = root && root->boundsCalculator ? root->boundsCalculator : SvgDocument::sharedBoundsCalc;
  std::string s = textnode->text();
  if(strchr(s.c_str(), '\n')) {
    std::replace(s.begin(), s.end(), '\n', ' ');
    textnode->setText(s.c_str());
  }
  textnode->addText("...");
  std::vector<Rect> glyphpos = bounder->glyphPositions(textnode);
  textnode->clearText();
  if(glyphpos.size() < 4 || glyphpos[glyphpos.size() - 4].right < maxWidth)
    textnode->addText(s.c_str());
  else {
    size_t jj = glyphpos.size() - 3;
    real overflow = glyphpos.back().right - maxWidth;
    real elided = glyphpos[jj-1].right - overflow;
    while(--jj > 0 && glyphpos[jj-1].right > elided) {}
    // we want to include first jj glyphs of s
    uint32_t codepoint, utf8_state = UTF8_ACCEPT;
    size_t kk = 0;
    for(; kk < s.size() && jj > 0; ++kk) {
      if(decode_utf8(&utf8_state, &codepoint, s[kk]) == UTF8_ACCEPT)
        --jj;
    }
    textnode->addText(s.substr(0, kk).append("...").c_str());
  }
}
