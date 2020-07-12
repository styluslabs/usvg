#include "svgnode.h"
#include "svgstyleparser.h"
#include "svgpainter.h"  // only needed for bounds()


const char* SvgLength::unitNames[] = {"px", "pt", "em", "ex", "%"};

// we could add a separate constexpr fn to do this mapping at compile time for things like case labels,
//  e.g. case SVG_ATTR("fill"):  but I'm not sure the extra complication is worthwhile
SvgAttr::StdAttr SvgAttr::nameToStdAttr(const char* name)
{
  static std::unordered_map<std::string, StdAttr> stdAttrMap = {
    {"color", COLOR}, {"comp-op", COMP_OP}, {"display", DISPLAY}, {"fill", FILL}, {"fill-opacity", FILL_OPACITY},
    {"fill-rule", FILL_RULE}, {"font-family", FONT_FAMILY}, {"font-size", FONT_SIZE},
    {"font-style", FONT_STYLE}, {"font-variant", FONT_VARIANT}, {"font-weight", FONT_WEIGHT},
    {"offset", OFFSET}, {"opacity", OPACITY}, {"shape-rendering", SHAPE_RENDERING}, {"stop-color", STOP_COLOR},
    {"stop-opacity", STOP_OPACITY}, {"stroke", STROKE}, {"stroke-dasharray", STROKE_DASHARRAY},
    {"stroke-dashoffset", STROKE_DASHOFFSET}, {"stroke-linecap", STROKE_LINECAP},
    {"stroke-linejoin", STROKE_LINEJOIN}, {"stroke-miterlimit", STROKE_MITERLIMIT},
    {"stroke-opacity", STROKE_OPACITY}, {"stroke-width", STROKE_WIDTH}, {"text-anchor", TEXT_ANCHOR},
    {"vector-effect", VECTOR_EFFECT}, {"visibility", VISIBILITY}
  };

  auto it = stdAttrMap.find(name);
  return it != stdAttrMap.end() ? it->second : UNKNOWN;
}

bool operator==(const SvgAttr& a, const SvgAttr& b)
{
  if(a.flags != b.flags || a.str != b.str)
    return false;
  // In C++ I think memcmp would work here (but not C due to unintialized padding)
  switch(a.valueType()) {
    case SvgAttr::IntVal: return a.value.intVal == b.value.intVal;
    case SvgAttr::ColorVal: return a.value.colorVal == b.value.colorVal;
    case SvgAttr::FloatVal: return a.value.floatVal == b.value.floatVal;
    case SvgAttr::StringVal: return a.value.strOffset == b.value.strOffset;
  }
  return false;
}

// mapping from SvgNode::Type to name
const char* SvgNode::nodeNames[] = {"svg", "g", "a", "defs", "symbol", "pattern", "gradient", "stop", "font",
    "glyph", "arc", "circle", "ellipse", "image", "line", "path", "polygon", "polyline", "rect", "text",
    "tspan", "use", "unknown", "custom" };

Transform2D SvgNode::identityTransform;

// we override default copy constructor to clear parent, and update ext node pointer; we no longer clear id
SvgNode::SvgNode(const SvgNode& n) : attrs(n.attrs), transform(n.transform ? new Transform2D(*n.transform) : NULL),
    m_cachedBounds(), m_renderedBounds(), m_dirty(NOT_DIRTY), m_parent(NULL),
    m_ext(n.m_ext ? n.m_ext->clone() : NULL), m_visible(n.m_visible), m_id(n.m_id), m_class(n.m_class)
{
  if(m_ext)
    m_ext->node = this;
}

void SvgNode::deleteFromExt()
{
  m_ext.release();
  delete this;
}

void SvgNode::createExt()
{
  SvgNode* node = parent();
  // an alternative would be to just do parent()->ext()->createExt(), which would create exts for all parents
  while(node && !node->m_ext)
    node = node->parent();
  ASSERT(node && "Unable to create SvgNodeExtension - no parent with ext set found!");
  if(node)
    node->ext()->createExt(this);  // note that ext constructor calls setExt
}

void SvgNode::setExt(SvgNodeExtension* ext)
{
  ASSERT(!m_ext && "SvgNode extension already set!");
  m_ext.reset(ext);
  //setRestyle();  -- don't know why this was added ... exts can do manually from constructor if needed
}

// const_cast is unfortunate, but I don't think we want to make ext() non-const
SvgNodeExtension* SvgNode::ext(bool create) const
{
  if(!m_ext && create)
    const_cast<SvgNode*>(this)->createExt();
  return m_ext.get();
}

Rect SvgNode::bounds() const
{
#ifndef DEBUG_CACHED_BOUNDS
  if(m_cachedBounds.isValid())
    return m_cachedBounds;
#endif
  Painter p;
  return SvgPainter(&p).nodeBounds(this);
}

void SvgNode::setDirty(DirtyFlag type) const
{
  // isVisible() is false for non-paintable nodes but we want dirty state to propagate for them
  if(!isVisible() && isPaintable())
    return;
  if(m_dirty == NOT_DIRTY && m_parent)
    m_parent->setDirty(CHILD_DIRTY);
  ASSERT(!m_parent || !m_parent->isVisible() || m_parent->m_dirty != NOT_DIRTY);
  if(type > m_dirty)
    m_dirty = type;
}

bool SvgNode::isPaintable() const
{
  auto t = type();
  return t != DEFS && t != SYMBOL && t != PATTERN && t != GRADIENT && t != STOP && t != UNKNOWN;
}

// invalidate bounds and set as BOUNDS_DIRTY
void SvgNode::invalidate(bool children) const
{
  invalidateBounds(children);
  setDirty(BOUNDS_DIRTY);
}

void SvgNode::invalidateBounds(bool children) const
{
  /*if(!m_cachedBounds.isValid()) {
    // what about <svg>, whose bounds may not depend on children???
#ifndef NDEBUG
    ASSERT((!m_parent || !m_parent->m_cachedBounds.isValid()) && "Parent bounds should not be valid!");
#endif
    return;
  }*/

  m_cachedBounds = Rect();
  // minor optimization: if a node's bounds are valid, then all children bounds are valid, thus if our bounds
  //  are already invalid, we could skip this since parent bounds should also be invalidated already
  if(m_parent && isVisible())
    m_parent->invalidateBounds(false);
}

// TODO: clear transform if tf is identity?
void SvgNode::setTransform(const Transform2D& tf)
{
  if(transform)
    *transform = tf;
  else
    transform.reset(new Transform2D(tf));
  invalidate(true);
}

// we now include viewbox transform since we have canvas rect
Transform2D SvgNode::totalTransform() const
{
  Transform2D tf = parent() ? parent()->totalTransform() : Transform2D();
  if(hasTransform())
    tf = tf * getTransform();
  if(type() == SvgNode::DOC)
    tf = tf * static_cast<const SvgDocument*>(this)->viewBoxTransform();
  return tf;
}

SvgDocument* SvgNode::document() const
{
  SvgNode* node = const_cast<SvgNode*>(this);
  while(node && node->type() != SvgNode::DOC)
    node = node->parent();
  return static_cast<SvgDocument*>(node);  // static_cast of NULL always returns NULL
}

SvgDocument* SvgNode::rootDocument() const
{
  SvgNode* node = const_cast<SvgNode*>(this);
  while(node->parent())
    node = node->parent();
  return node->type() == SvgNode::DOC ? static_cast<SvgDocument*>(node) : NULL;
}

void SvgNode::setDisplayMode(DisplayMode mode)
{
  const SvgAttr* attr = getAttr("display", SvgAttr::XMLSrc);
  if(mode != BlockMode) {
    if(!attr || attr->intVal() != mode)
      setAttr("display", mode, SvgAttr::XMLSrc);
  }
  else if(attr)
    removeAttr("display", SvgAttr::XMLSrc);  // we don't want a bunch of unnecessary display=block attrs
}

SvgNode::DisplayMode SvgNode::displayMode() const
{
  const SvgAttr* attr = getAttr("display", SvgAttr::XMLSrc);
  return attr ? DisplayMode(attr->intVal()) : BlockMode;
}

bool SvgNode::isVisible() const
{
  return m_visible && isPaintable();
}

bool SvgNode::hasClass(const char* s) const { return containsWord(m_class.c_str(), s); }

void SvgNode::setXmlClass(const char* str)
{
  if(str != m_class) {
    m_class = str;
    restyle();
  }
}

void SvgNode::addClass(const char* s) { setXmlClass(addWord(m_class, s).c_str()); }
void SvgNode::removeClass(const char* s) { setXmlClass(removeWord(m_class, s).c_str()); }

void SvgNode::setXmlId(const char* id)
{
  if(id == m_id)
    return;
  SvgDocument* doc = m_parent ? m_parent->document() : document();  // handle the case where we are <svg>
  if(doc && !m_id.empty())
    doc->removeNamedNode(this);
  m_id = id;
  if(doc && !m_id.empty())
    doc->addNamedNode(this);
  restyle();
}

// Previously, we set a flag, defering restyle until an attribute was requested.  However, restyle could
//  also change node bounds, so to prevent invalid state, we'd need to clear cached bounds (and thus bounds
//  of all ancestors) ... but there don't seem to be too many cases where deferred restyle would be a
//  significant optimization, so we'll just do it immediately
bool SvgNode::restyle()
{
  //PLATFORM_LOG("setting restyle for id=%s class=%s\n", m_id.c_str(), m_class.c_str());
#ifndef NO_DYNAMIC_STYLE
  SvgDocument* doc = document();
  if(!doc || !doc->canRestyle())
    return false;
  // mark CSS attributes stale
  for(auto it = attrs.rbegin(); it != attrs.rend() && it->src() == SvgAttr::CSSSrc; ++it)
    it->setStale(true);
  doc->restyleNode(this);
  // remove stale attrs that were not replaced; erase() doesn't accept reverse_iterator
  for(auto it = attrs.end(); it != attrs.begin() && (--it)->src() == SvgAttr::CSSSrc;) {
    if(it->isStale()) {
      // must erase before we call onAttrChange!
      std::string name = it->name();
      auto stdattr = it->stdAttr();
      it = attrs.erase(it);
      onAttrChange(name.c_str(), stdattr);
    }
  }
  return true;
#endif
}

// node notifies ext of each attr change via onAttrChange() so it can update any cached values
// - if cached values depend on multiple attributes, makes sense to set flag and recalc just before use
// - if each cached value depends only on a single attribute, they can be recalculated as soon as it changes
void SvgNode::onAttrChange(const char* name, SvgAttr::StdAttr stdattr)
{
  // invalidating bounds of all children can be expensive, so only do so if necessary
  // if we wanted to be fancier, in container node we could keep track of number of total descendants and
  //  number of descendants w/ valid bounds (then we can stop descending if valid count = 0)
  if(type() == STOP) {
    if(m_parent)
      m_parent->setDirty(PIXELS_DIRTY);
  }
  else {
    switch(stdattr) {
    case SvgAttr::FONT_FAMILY:
    case SvgAttr::FONT_SIZE:
    case SvgAttr::FONT_STYLE:
    case SvgAttr::FONT_VARIANT:
    case SvgAttr::FONT_WEIGHT:
    case SvgAttr::STROKE:
    case SvgAttr::STROKE_LINECAP:
    case SvgAttr::STROKE_LINEJOIN:
    case SvgAttr::STROKE_MITERLIMIT:
    case SvgAttr::STROKE_WIDTH:
    case SvgAttr::TEXT_ANCHOR:
      invalidate(true);  // bounds may have changed
      break;
    case SvgAttr::DISPLAY:
    case SvgAttr::VISIBILITY:
    {
      int dispmode = getIntAttr("display", BlockMode);
      // SvgPainter::calcDirtyRect() ignores AbsoluteMode nodes, so if we are switching a node from BlockMode,
      //  we add bounds to parent's removedBounds to get correct dirty rect
      if(stdattr == SvgAttr::DISPLAY && dispmode == AbsoluteMode && m_visible && m_parent->asContainerNode())
        m_parent->asContainerNode()->m_removedBounds.rectUnion(m_renderedBounds);  //bounds());

      bool vis = dispmode != NoneMode && getIntAttr("visibility", 1);
      if(vis != m_visible) {
        // exactly one of these invalidate() calls will be a no-op since visible will be false
        invalidate(false);
        m_visible = vis;
      }
      invalidate(false);  // always have to invalidate because of AbsoluteMode <-> BlockMode case
      break;
    }
    case SvgAttr::UNKNOWN:
      break;
    default:  // all other standard attributes
      setDirty(PIXELS_DIRTY);
      break;
    }
  }
  if(hasExt())
    ext()->onAttrChange(name);
}

static bool replaceAttr(SvgAttr& dest, const SvgAttr& src)
{
  dest.setStale(false);
  if(dest == src)
     return false;
  dest = src;
  return true;
}

bool SvgNode::setAttrHelper(const SvgAttr& attr)
{
  // Attrs are stored in the following order: XMLSrc, InlineStyleSrc, CSSSrc
  // if inline style (style=) src, insert before first CSS attr and remove any CSS w/ same name
  // if CSS src, replace CSS attr w/ same name or insert at end, unless inline style version exists
  // if XML src, insert after last XML attr (or at beginning)
  if(attr.src() == SvgAttr::XMLSrc) {
    for(auto it = attrs.begin(); it != attrs.end(); ++it) {
      if(it->nameIs(attr.name()) && it->src() == SvgAttr::XMLSrc)
        return replaceAttr(*it, attr);
      else if(it->src() != SvgAttr::XMLSrc) {
        attrs.insert(it, attr);
        return true;
      }
    }
    attrs.push_back(attr);
  }
  else if(attr.src() == SvgAttr::CSSSrc) {
    // note reverse order iteration
    for(auto it = attrs.rbegin(); it != attrs.rend() && it->src() != SvgAttr::XMLSrc; ++it) {
      if(it->nameIs(attr.name())) {  // src is CSS or inline style
        // CSS rules are processed from high to low priority, so we only replace stale attributes
        if(it->src() == SvgAttr::CSSSrc && it->isStale())
          return replaceAttr(*it, attr);
        return false;  // discard if overridden by inline style
      }
    }
    attrs.push_back(attr);
  }
  else if(attr.src() == SvgAttr::InlineStyleSrc) {
    auto it = attrs.begin();
    for(; it != attrs.end() && it->src() != SvgAttr::CSSSrc; ++it) {
      if(it->nameIs(attr.name()) && it->src() == SvgAttr::InlineStyleSrc)
        return replaceAttr(*it, attr);  // if already present as inline style, can't be a CSSSrc version
    }
    it = attrs.insert(it, attr);
    // remove CSSSrc attr is present
    for(++it; it != attrs.end(); ++it) {
      if(it->nameIs(attr.name()) && it->src() == SvgAttr::CSSSrc) {
        attrs.erase(it);
        break;
      }
    }
  }
  return true;
}

void SvgNode::setAttr(const SvgAttr& attr)
{
  if(setAttrHelper(attr))
    onAttrChange(attr.name(), attr.stdAttr());
}

void SvgNode::setAttribute(const char* name, const char* value, SvgAttr::Src src)
{
  processAttribute(this, src, name, value);  // this will call setAttr
}

void SvgNode::removeAttr(const char* name, int src)
{
  // arguably, we should restyle if removing an inline style, which may have blocked CSS attributes, but
  //  since we remove all instances of attribute, those CSS attributes would be removed anyway, so we'll
  //  require user manually force restyle if desired
  // arguably, we should also restyle if needed before removing attributes!
  size_t n = attrs.size();
  for(auto it = attrs.begin(); it != attrs.end();)
    it = it->nameIs(name) && (it->src() & src) ? attrs.erase(it) : ++it;
  if(attrs.size() < n)
    onAttrChange(name, SvgAttr::nameToStdAttr(name));  // this is OK for now since removeAttr is rarely used
}

const SvgAttr* SvgNode::getAttr(const char* name, int src) const
{
  for(auto it = attrs.rbegin(); it != attrs.rend(); ++it) {
    if(it->nameIs(name) && (it->src() & src))
      return &*it;
  }
  return NULL;
}

int SvgNode::getIntAttr(const char* name, int dflt) const
{
  const SvgAttr* attr = getAttr(name);
  return attr && attr->valueIs(SvgAttr::IntVal) ? attr->intVal() : dflt;
}

Color SvgNode::getColorAttr(const char* name, color_t dflt) const
{
  const SvgAttr* attr = getAttr(name);
  return attr && attr->valueIs(SvgAttr::ColorVal) ? attr->colorVal() : dflt;
}

float SvgNode::getFloatAttr(const char* name, float dflt) const
{
  const SvgAttr* attr = getAttr(name);
  return attr && attr->valueIs(SvgAttr::FloatVal) ? attr->floatVal() : dflt;
}

const char* SvgNode::getStringAttr(const char* name, const char* dflt) const
{
  const SvgAttr* attr = getAttr(name);
  return attr && attr->valueIs(SvgAttr::StringVal) ? attr->stringVal() : dflt;
}

// convert CSS style attrs to inline style attrs, e.g., to allow for insertion into another document; note that
//  a CSS attr is not added to node if overriding inline style attr is present, so all we have to do is change
//  flags on CSS attrs
void SvgNode::cssToInlineStyle()
{
  for(auto it = attrs.rbegin(); it != attrs.rend() && it->src() == SvgAttr::CSSSrc; ++it)
    it->setFlags(it->getFlags() ^ (SvgAttr::CSSSrc | SvgAttr::InlineStyleSrc));
  if(asContainerNode()) {
    for(SvgNode* child : asContainerNode()->children())
      child->cssToInlineStyle();
  }
}

// convenience fn
SvgNode* SvgNode::getRefTarget(const char* id) const
{
  if(!id) return NULL;
  SvgDocument* doc = document();
  return doc ? doc->namedNode(id) : NULL;
}

// path to node for debugging
std::string SvgNode::nodePath(const SvgNode* node)
{
  std::vector<const SvgNode*> path;
  do { path.push_back(node); } while( (node = node->parent()) );

  std::string pathstr;
  for(auto it = path.rbegin(); it != path.rend(); ++it) {
    pathstr.append(SvgNode::nodeNames[(*it)->type()]);
    const char* xmlid = (*it)->xmlId();
    std::string xmlclass = (*it)->xmlClass();
    if(xmlid && xmlid[0])
      pathstr.append("#").append(xmlid);
    else if(!xmlclass.empty()) {
      std::replace(xmlclass.begin(), xmlclass.end(), ' ', '.');
      pathstr.append(".").append(xmlclass);
    }
    pathstr.append(" > ");
  }
  pathstr.erase(pathstr.size() - 3);
  return pathstr;
}

// SvgNodeExtension

SvgNodeExtension::SvgNodeExtension(SvgNode* n) : node(n)
{
  n->setExt(this);
}

Rect SvgNodeExtension::dirtyRect() const
{
  return node->m_dirty != SvgNode::NOT_DIRTY ? node->bounds().rectUnion(node->m_renderedBounds) : Rect();
}

// SvgContainerNode

static bool isSingleIdent(const char* s)
{
  if(!s || !s[0] || isDigit(*s))
    return false;
  for(; *s; ++s) {
    if(!(isAlpha(*s) || isDigit(*s) || *s == '-' || *s == '_'))
      return false;
  }
  return true;
}

// TODO: this should use BFS not DFS!
SvgNode* SvgContainerNode::selectFirst(const char* selector) const
{
  std::vector<SvgNode*> hits = select(selector, 1);
  return hits.empty() ? NULL : hits.front();
}

// TODO: support complex selectors using cssparser; to support parsed attributes, serialize node
//  (or just style?) if selector uses attributes other than id and class
std::vector<SvgNode*> SvgContainerNode::select(const char* selector, size_t nhits) const
{
  if(!selector || !selector[0])
    return {};
  if(strcmp(selector, "*") == 0)  // or do this if selector is empty?
    return std::vector<SvgNode*>(children().cbegin(), children().cend());
  if(selector[0] == '#' && isSingleIdent(selector+1)) {
    SvgNode* node = getRefTarget(selector);
    for(SvgNode* parent = node; parent; parent = parent->parent()) {
      if(parent == this)
        return {node};
    }
    return {};
  }
  if(selector[0] == '.' && isSingleIdent(selector+1)) {
    std::vector<SvgNode*> hits;
    std::function<void(SvgNode*)> selfn = [&selfn, &selector, &hits, nhits](SvgNode* node){
      if(hits.size() >= nhits)
        return;
      if(node->hasClass(selector+1))
        hits.push_back(node);
      if(node->asContainerNode()) {
        for(SvgNode* child : node->asContainerNode()->children())
          selfn(child);
      }
    };
    selfn(const_cast<SvgContainerNode*>(this));
    return hits;
  }
  if(isSingleIdent(selector)) {
    for(size_t typeId = 0; typeId < SvgNode::NUM_NODE_TYPES; ++typeId) {
      if(strcmp(selector, SvgNode::nodeNames[typeId]) == 0) {
        std::vector<SvgNode*> hits;
        std::function<void(SvgNode*)> selfn = [&selfn, typeId, &hits, nhits](SvgNode* node){
          if(hits.size() >= nhits)
            return;
          // this seems to be the only place we need a hack to deal with <a>, whereas making <a> a separate
          //  class would require additional checks in many places
          if(node->type() == typeId || (typeId == A && node->type() == G && static_cast<SvgG*>(node)->groupType == A))
            hits.push_back(node);
          if(node->asContainerNode()) {
            for(SvgNode* child : node->asContainerNode()->children())
              selfn(child);
          }
        };
        selfn(const_cast<SvgContainerNode*>(this));
        return hits;
      }
    }
  }
  PLATFORM_LOG("Invalid node selector - only simple selectors are supported in select(): %s", selector);
  return {};
}

static void addIds(SvgDocument* doc, SvgNode* node)
{
  if(node->xmlId()[0])
    doc->addNamedNode(node);
  if(node->asContainerNode()) {
    for(SvgNode* child : node->asContainerNode()->children())
      addIds(doc, child);
  }
}

static void removeIds(SvgDocument* doc, SvgNode* node)
{
  if(node->xmlId()[0])
    doc->removeNamedNode(node);
  if(node->asContainerNode()) {
    for(SvgNode* child : node->asContainerNode()->children())
      removeIds(doc, child);
  }
}

void SvgContainerNode::addChild(SvgNode* child, SvgNode* next)
{
  child->invalidateBounds(true);
  child->m_renderedBounds = Rect();
  child->setParent(this);
  child->m_dirty = NOT_DIRTY;  // ensure that parents will be marked CHILD_DIRTY
  child->setDirty(BOUNDS_DIRTY);

  if(next)
    children().insert(std::find(children().begin(), children().end(), next), child);
  else
    children().push_back(child);
  // invalidate bounds if necessary
  if(m_cachedBounds.isValid() && !m_cachedBounds.contains(child->bounds()))
    invalidateBounds(false);

  SvgDocument* doc = document();
  if(doc) {
    child->restyle();
    addIds(doc, child);
  }
}

SvgNode* SvgContainerNode::removeChild(SvgNode* child)
{
  auto it = std::find(children().begin(), children().end(), child);
  if(it == children().end())
    return NULL;

  if(m_renderedBounds.isValid())
    m_removedBounds.rectUnion(child->m_renderedBounds);  //child->bounds());
  setDirty(CHILD_DIRTY);  // or should we add a level above CHILD_DIRTY for removed node?

  // TODO: We need to use fuzzyEq here!
  if(m_cachedBounds.isValid()) {
    Rect bbox = child->bounds();
    if(m_cachedBounds.left >= bbox.left || m_cachedBounds.top >= bbox.top
        || m_cachedBounds.right <= bbox.right || m_cachedBounds.bottom <= bbox.bottom)
      invalidateBounds(false);
  }
  SvgDocument* doc = document();
  if(doc)
    removeIds(doc, child);
  child->setParent(NULL);
  auto next = children().erase(it);
  return next != children().end() ? *next : NULL;
}

// because CSS selectors can select children, we must restyle all children upon attribute change
bool SvgContainerNode::restyle()
{
#ifndef NO_DYNAMIC_STYLE
  if(!SvgNode::restyle())
    return false;
  for(SvgNode* child : children())
    child->restyle();
  return true;
#endif
}

// find top-most visual node under a point; excludes container nodes by default
SvgNode* SvgContainerNode::nodeAt(const Point& p, bool visual_only) const
{
  // process children in top to bottom z-order
  for(auto it = children().crbegin(); it != children().crend(); ++it) {
    SvgNode* node = *it;
    if(node->isVisible() && node->bounds().contains(p)) {
      if(node->asContainerNode()) {
        SvgNode* fromchild = node->asContainerNode()->nodeAt(p, visual_only);
        if(fromchild)
          return fromchild;
      }
      else
        return node;
    }
  }
  return (visual_only || !bounds().contains(p)) ? NULL : const_cast<SvgContainerNode*>(this);
}

void SvgContainerNode::invalidateBounds(bool inclChildren) const
{
  SvgNode::invalidateBounds(inclChildren);
  if(inclChildren) {
    for(SvgNode* node : children())
      node->invalidateBounds(true);
  }
}

SvgPattern::SvgPattern(real x, real y, real w, real h, Units_t pu, Units_t pcu)
    : m_cell(Rect::ltwh(x, y, w, h)), m_patternUnits(pu), m_patternContentUnits(pcu) {}

void SvgGradient::addStop(SvgGradientStop* stop)
{
  stop->setParent(this);
  stops().push_back(stop);
  SvgDocument* doc = document();
  if(doc && strlen(stop->xmlId()) > 0)
    doc->addNamedNode(stop);
}

// because CSS selectors can select children, we must restyle all children upon attribute change
bool SvgGradient::restyle()
{
#ifndef NO_DYNAMIC_STYLE
  if(!SvgNode::restyle())
    return false;
  for(SvgNode* child : stops())
    child->restyle();
  return true;
#endif
}

// if rebuilding gradient everytime is an issue, we could make onAttrChange virtual, and use it in <stop> to
//  set flag in parent gradient to rebuild
Gradient& SvgGradient::gradient() const
{
  // do this every time because we don't know when linked gradient gets restyled
  if(stops().empty() && m_link)
    m_gradient.setStops(m_link->m_gradient.stops());
  else {
    m_gradient.clearStops();
    real offset = 0;
    for(SvgGradientStop* child : stops()) {
      SvgGradientStop* svgstop = static_cast<SvgGradientStop*>(child);
      offset = std::min(std::max((real)svgstop->getFloatAttr("offset", 0), offset), 1.0);
      Color color = svgstop->getColorAttr("stop-color", Color::BLACK);
      // support stop-color w/ alpha < 1
      color.setAlphaF(color.alphaF() * svgstop->getFloatAttr("stop-opacity", 1.0));
      m_gradient.setColorAt(offset, color);
    }
  }
  return m_gradient;
}

// SvgDocument

SvgDocument::SvgDocument(real x, real y, SvgLength w, SvgLength h)
    : m_x(x), m_y(y), m_width(w), m_height(h) {}

SvgDocument* SvgDocument::clone() const
{
  ASSERT(m_fonts.empty() && "Cloning SvgDocument with fonts not yet supported");
#ifndef NO_DYNAMIC_STYLE
  ASSERT(!m_stylesheet && "Cloning SvgDocument with stylesheet not yet supported");
#endif
  SvgDocument* c = new SvgDocument(*this);
  c->m_stylesheet = NULL;
  c->m_fonts.clear();
  if(!c->m_namedNodes.empty()) {
    c->m_namedNodes.clear();
    addIds(c, c);
  }
  return c;
}

// note that newid must include leading '#'
static void replaceId(SvgNode* node, const char* oldid, const char* newid)
{
  const char* fillref = node->getStringAttr("fill");
  if(fillref && strcmp(fillref+1, oldid) == 0)
    node->setAttr("fill", newid);
  const char* strokeref = node->getStringAttr("stroke");
  if(strokeref && strcmp(strokeref+1, oldid) == 0)
    node->setAttr("stroke", newid);
  const char* href = node->getStringAttr("xlink:href");  // we should do "href" too
  if(href && href[0] == '#' && strcmp(href+1, oldid) == 0)
    node->setAttr("xlink:href", newid);
  if(node->asContainerNode()) {
    for(SvgNode* child : node->asContainerNode()->children())
      replaceId(child, oldid, newid);
  }
}

// if dest != NULL, only ids conflicting with dest are replaced; otherwise, all are replaced
void SvgDocument::replaceIds(SvgDocument* dest)
{
  // we expect source to typically have fewer named nodes than dest, so iterate over source
  // must collect ids first because we can't modify namedNodes while iterating
  std::vector<SvgNode*> nodes;
  if(!dest) nodes.reserve(m_namedNodes.size());
  for(auto entry : m_namedNodes) {
    if(!dest || dest->namedNode(entry.first.c_str()))
      nodes.push_back(entry.second);
  }
  for(SvgNode* node : nodes) {
    std::string newid = "#x-" + randomStr(10);
    replaceId(this, node->xmlId(), newid.c_str());
    node->setXmlId(newid.c_str() + 1);  // skip '#'; this will update m_namedNodes
  }
}

void SvgDocument::setWidth(const SvgLength& w)
{
  if(w != m_width)  //&& (w.isPercent() || m_width.isPercent() || m_viewBox.isValid()))
    invalidate(m_viewBox.isValid());
  m_width = w;
}

void SvgDocument::setHeight(const SvgLength& h)
{
  if(h != m_height)  //&& (h.isPercent() || m_height.isPercent() || m_viewBox.isValid()))
    invalidate(m_viewBox.isValid());
  m_height = h;
}

void SvgDocument::setUseSize(real w, real h)
{
  ASSERT(((w == 0 && h == 0) || (m_useWidth == 0 && m_useHeight == 0)) && "This shouldn't happen");
  SvgLength oldw = width(), oldh = height();
  m_useWidth = w;
  m_useHeight = h;
  if(width() != oldw || height() != oldh)
    invalidateBounds(true);  // clear cached bounds but do not set as dirty!   invalidate(m_viewBox.isValid());
}

Rect SvgDocument::viewportRect() const
{
  real w = width().value;
  real h = height().value;
  if(width().isPercent() || height().isPercent()) {
    SvgDocument* parent_doc = parent() ? parent()->document() : NULL;
    Rect canvas = parent_doc ? parent_doc->bounds() : canvasRect();
    if(!canvas.isValid())
      canvas = m_viewBox;  // hopefully this is valid
    if(width().isPercent()) w *= canvas.width()/100;
    if(height().isPercent()) h *= canvas.height()/100;
  }
  return Rect::wh(w, h);
}

Transform2D SvgDocument::viewBoxTransform() const
{
  Transform2D tf;
  tf.translate(m_x, m_y);
  Rect source = m_viewBox;
  if(source.isValid()) {
    Rect target = viewportRect();
    if(source != target) {
      real sx = target.width() / source.width();
      real sy = target.height() / source.height();
      if(preserveAspectRatio())
        sx = sy = std::min(sx, sy);
      tf.translate(-source.center()).scale(sx, sy).translate(target.center());
      // tf.translate(-source.origin()).scale(sx, sy).translate(target.origin());  -- previous approach
    }
  }
  return tf;
}

void SvgDocument::addSvgFont(SvgFont* font)
{
  m_fonts.emplace(font->familyName(), font);
}

SvgFont* SvgDocument::svgFont(const char* family) const
{
  SvgDocument* parent_doc;
  auto it = m_fonts.find(family);
  if(it == m_fonts.end() && m_parent && (parent_doc = m_parent->document()))
    return parent_doc->svgFont(family);
  return it != m_fonts.end() ? const_cast<SvgFont*>(it->second) : NULL;
}

void SvgDocument::addNamedNode(SvgNode* node)
{
  const char* id = node->xmlId();
  //m_namedNodes.emplace(id, node);  // does not replace existing
  m_namedNodes[id] = node;  // does replace existing
}

void SvgDocument::removeNamedNode(SvgNode* node)
{
  const char* id = node->xmlId();
  auto it = m_namedNodes.find(id);
  if(it != m_namedNodes.end() && it->second == node)
    m_namedNodes.erase(it);
}

SvgNode* SvgDocument::namedNode(const char* id) const
{
  SvgDocument* parent_doc;
  auto it = m_namedNodes.find(id[0] == '#' ? id + 1 : id);
  if(it == m_namedNodes.end() && m_parent && (parent_doc = m_parent->document())) {
    if(IS_DEBUG) ASSERT(!parent_doc->namedNode(id) && "named node found on parent!");
    return parent_doc->namedNode(id);
  }
  return it != m_namedNodes.end() ? it->second : NULL;
}

#ifndef NO_DYNAMIC_STYLE
SvgDocument::~SvgDocument() { if(m_stylesheet) delete m_stylesheet; }
// caller should call restyle() after setting stylesheet (after adding styles and calling sort_rules())
void SvgDocument::setStylesheet(SvgCssStylesheet* ss) { if(m_stylesheet) delete m_stylesheet; m_stylesheet = ss; }
#endif

// behavior here is not consistent with Chrome or Firefox - they seem to merge all <style>s into
//  one and give no special treatment to <svg>
void SvgDocument::restyleNode(SvgNode* node)
{
#ifndef NO_DYNAMIC_STYLE
  // TODO: calling parent doc restyle is a noop if we have a stylesheet set since parseCssStyle completely
  //  overwrites all CSS style
  SvgDocument* parent_doc;
  if(m_parent && (parent_doc = m_parent->document()))
    parent_doc->restyleNode(node);
  if(m_stylesheet)
    m_stylesheet->applyStyle(node);
#endif
}

bool SvgDocument::canRestyle()
{
#ifndef NO_DYNAMIC_STYLE
  return (m_stylesheet && !m_stylesheet->rules().empty())
      || (m_parent && m_parent->document() && m_parent->document()->canRestyle());
#else
  return false;
#endif
}

SvgImage::SvgImage(Image image, const Rect& bounds, const char* linkStr)
    : m_image(std::move(image)), m_bounds(bounds), m_linkStr(linkStr ? linkStr : "") {}

SvgImage::SvgImage(const SvgImage& other)
    : SvgNode(other), m_image(other.m_image.copy()), m_bounds(other.m_bounds), m_linkStr(other.m_linkStr) {}

Rect SvgImage::viewport() const
{
  real w = m_bounds.width(), h = m_bounds.height();
  real imgw = m_image.getWidth(), imgh = m_image.getHeight();
  if(w > 0 && h > 0)
    return m_bounds;
  if(w <= 0 && h <= 0)
    return Rect::ltwh(m_bounds.left, m_bounds.top, imgw, imgh);
  if(w > 0)  // h <= 0
    return Rect::ltwh(m_bounds.left, m_bounds.top, w, imgh*w/imgw);
  //if(h > 0)  // w <= 0
  return Rect::ltwh(m_bounds.left, m_bounds.top, imgw*h/imgh, h);
}

// SvgPath / SvgRect

// rect (incl. rounded rects) are key GUI elements, thus we will separate from SvgPath

SvgRect::SvgRect(const Rect& rect, real rx, real ry) : SvgPath(RECT), m_rect(rect), m_rx(rx), m_ry(ry)
{
  updatePath();
}

// if rx (or ry) is not passed or < 0, rx (or ry) is not changed
void SvgRect::setRect(const Rect& rect, real rx, real ry)
{
  if(!rect.isValid() || (m_rect == rect && rx == m_rx && ry == m_ry))
    return;
  m_rect = rect;
  m_rx = rx >= 0 ? rx : m_rx;
  m_ry = ry >= 0 ? ry : m_ry;
  updatePath();
  invalidate(false);
}

void SvgRect::updatePath()
{
  m_path.clear();
  if(m_rect.width() <= 0 || m_rect.height() <= 0)
    return;  // SVG spec says <rect> width or height == 0 disables rendering
  if(m_rx > 0 || m_ry > 0) {
    real x = m_rect.left;
    real y = m_rect.top;
    real w = m_rect.width();
    real h = m_rect.height();
    real rxx2 = std::min(m_rx, w/2);
    real ryy2 = std::min(m_ry, h/2);

    m_path.moveTo(x, y+ryy2);
    m_path.addArc(x+rxx2, y+ryy2, rxx2, ryy2, M_PI, M_PI/2);
    m_path.lineTo(x+w-rxx2, y);
    m_path.addArc(x+w-rxx2, y+ryy2, rxx2, ryy2, -M_PI/2, M_PI/2);
    m_path.lineTo(x+w, y+h-ryy2);
    m_path.addArc(x+w-rxx2, y+h-ryy2, rxx2, ryy2, 0, M_PI/2);
    m_path.lineTo(x+rxx2, y+h);
    m_path.addArc(x+rxx2, y+h-ryy2, rxx2, ryy2, M_PI/2, M_PI/2);
    m_path.closeSubpath();
  }
  else
    m_path.addRect(m_rect);
}

// if <use> refers to external document, it should be passed as doc and will be deleted when SvgUse is
//  deleted; use of shared_ptr ensures correct behavior even if this node is cloned
SvgUse::SvgUse(const Rect& bounds, const char* linkStr, const SvgNode* node, SvgDocument* doc)
  : m_link(node), m_viewport(bounds), m_linkStr(linkStr), m_doc(doc) {}

const SvgNode* SvgUse::target() const
{
  if(m_link)
    return m_link;
  SvgDocument* d = m_doc ? m_doc.get() : document();
  return d ? d->namedNode(m_linkStr.c_str()) : NULL;
}

void SvgUse::setTarget(const SvgNode* link)
{
  if(m_link != link) {
    m_linkStr.clear();  // href is invalid now
    m_link = link;
    invalidate(false);
  }
}

// SvgText / SvgTspan

bool SvgTspan::restyle()
{
#ifndef NO_DYNAMIC_STYLE
  if(!SvgNode::restyle())
    return false;
  for(SvgTspan* tspan : tspans())
    tspan->restyle();
  return true;
#endif
}

void SvgTspan::addTspan(SvgTspan* tspan)
{
  if(!m_text.empty()) {
    tspans().push_back(new SvgTspan(false));
    tspans().back()->setParent(this);
    tspans().back()->m_text.swap(m_text);
  }
  tspans().push_back(tspan);
  tspan->setParent(this);
  tspan->restyle();
  invalidate(false);
}

void SvgTspan::addText(const char* text)
{
  if(tspans().empty() && !strchr(text, '\n')) {
    m_text.append(text);
    invalidate(false);
    return;
  }

  const char* endl = NULL;
  do {
    endl = strchr(text, '\n');
    if(text[0] && (!endl || endl - text > 0)) {
      SvgTspan* tspan = new SvgTspan(false);
      tspan->m_text.append(text, endl ? (endl - text) : strlen(text));
      // if m_text was not empty, the first call to addTspan will move it to a tspan
      addTspan(tspan);
    }
    if(endl)
      addTspan(new SvgTspan(false, "\n"));
    text = endl + 1;
  } while(endl);
}

void SvgTspan::clearText()
{
  m_text.clear();
  for(SvgTspan* tspan : tspans())
    delete tspan;
  tspans().clear();
  invalidate(false);
}

std::string SvgTspan::text() const
{
  // remember only one of m_text and m_tspans will be non-empty
  std::string s = m_text;
  for(const SvgTspan* tspan : tspans())
    s.append(tspan->text());
  return s;
}

// We only support xml:space="preserve" mode (consecutive spaces not collapsed)
std::string SvgTspan::displayText() const
{
  std::string s(m_text);
  size_t ii = 0;
  while( (ii = s.find_first_of("\f\n\r\t\v", ii)) != std::string::npos )
    s[ii++] = ' ';
  return s;
}

SvgGlyph::SvgGlyph(const char* name, const char* unicode, real horizAdvX)
    : m_name(name), m_unicode(unicode), m_horizAdvX(horizAdvX) {}

SvgFont::~SvgFont()
{
  for(SvgGlyph* glyph : m_glyphs)
    delete glyph;
}

void SvgFont::addGlyph(SvgGlyph* glyph)
{
  const char* unicode = glyph->m_unicode.c_str();
  m_glyphs.push_back(glyph);
  m_glyphMap.emplace(unicode, glyph);
  m_maxUnicodeLen = std::max(m_maxUnicodeLen, (int)strlen(unicode));
}

real SvgFont::horizAdvX(const SvgGlyph* glyph) const
{
  return glyph->m_horizAdvX >= 0 ? glyph->m_horizAdvX : m_horizAdvX;
}

// This is basically a brute force search (but using hash map) to find the glyph
// this is needed because glyphs can be provided for arbitrary unicode strings to support ligatures etc.
// e.g. we might have glyphs for f, ff, and ffl, so if the string reads from the current position "ffl..."
//  we want to use the ffl glyph

std::vector<const SvgGlyph*> SvgFont::glyphsForText(const char* str) const
{
  std::vector<const SvgGlyph*> glyphs;

  // currently, missing-glyph node is distinguished by empty key - probably should store explicitly instead
  auto dg = m_glyphMap.find("");
  const SvgGlyph* defaultGlyph = dg != m_glyphMap.end() ? dg->second : NULL;

  int len = strlen(str);
  for(int ii = 0; ii < len; ++ii) {
    const SvgGlyph* glyph = defaultGlyph;
    // find glyph with longest matching unicode sequence
    for(int jj = 1; jj <= std::min(m_maxUnicodeLen, len - ii); ++jj) {
      auto it = m_glyphMap.find(std::string(str + ii, jj));
      if(it != m_glyphMap.end())
        glyph = it->second;
    }
    if(glyph) {
      glyphs.push_back(glyph);
      ii += std::max(0, (int)glyph->m_unicode.size() - 1);
    }
  }
  return glyphs;
}

real SvgFont::kerningForPair(const SvgGlyph* g1, const SvgGlyph* g2) const
{
  // brute force again ... note that we don't support ranges, only comma-separated lists
  for(const SvgFont::Kerning& k : m_kerning) {
    if((containsWord(k.g1.c_str(), g1->m_name.c_str(), ',') || containsWord(k.u1.c_str(), g1->m_unicode.c_str(), ','))
        && (containsWord(k.g2.c_str(), g2->m_name.c_str(), ',') || containsWord(k.u2.c_str(), g2->m_unicode.c_str(), ',')))
      return k.k;
  }
  return 0;
}
