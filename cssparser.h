#pragma once

// CSS parser based on https://github.com/litehtml/litehtml (BSD license)

#include <string>
#include <vector>
#include <memory>
#include <functional>

enum attr_select_condition
{
  select_exists,
  select_equal,
  select_contain_str,
  select_start_str,
  select_end_str,
  select_pseudo_class,
  select_pseudo_element,
};

struct css_attribute_selector
{
  std::string attribute;
  std::string val;
  //std::vector<std::string>    class_val;
  attr_select_condition condition = select_exists;
};

class css_element_selector
{
public:
  std::string m_tag;
  std::vector<css_attribute_selector> m_attrs;
  void parse(const std::string& txt);
};

enum css_combinator {combinator_descendant, combinator_child, combinator_adjacent_sibling, combinator_general_sibling};

class css_selector
{
public:
  css_element_selector m_right;
  std::unique_ptr<css_selector> m_left;
  css_combinator m_combinator = combinator_descendant;

  bool parse(const std::string& text);
  int calc_specificity();
};

// subclass this to store declarations (passed to parseDecl) and provide fns needed for selector matching
class css_declarations
{
public:
  virtual ~css_declarations() {}
  virtual bool isTag(void* el, const char* tag) const = 0;
  virtual bool hasClass(void* el, const char* cls) const = 0;
  virtual bool hasId(void* el, const char* id) const = 0;
  virtual const char* attribute(void* el, const char* name) const = 0;
  virtual void* parent(void* el) const = 0;
  virtual void parseDecl(const char* name, const char* value) = 0;
};

// rule = selector + declaration block
class css_rule
{
public:
  std::unique_ptr<css_selector> m_selector;
  std::unique_ptr<css_declarations> m_decls;
  int m_specificity = 0;
  int m_order = 0;

  css_rule(css_selector* sel, css_declarations* decls, int order)
    : m_selector(sel), m_decls(decls), m_specificity(sel->calc_specificity()), m_order(order) {}
  const css_declarations* decls() const { return m_decls.get(); }
  void parse_declarations(const std::string& stylestr);
  bool select(void* el) const { return select(el, *m_selector); }

private:
  bool select(void* el, const css_selector& selector) const;
  bool select_ancestor(void* el, const css_selector& selector) const;
  bool element_select(void* el, const css_element_selector& selector) const;
};

// pass stylesheet(s) as text to parse_stylesheet(), then call sort_rules() after last stylesheet
class css_stylesheet
{
public:
  virtual ~css_stylesheet() {}
  virtual css_declarations* createCssDecls() = 0;
  const std::vector<css_rule>& rules() const { return m_rules; }
  void parse_stylesheet(const char* str);  //, const char* baseurl);
  void sort_rules();

private:
  std::vector<css_rule> m_rules;

  void parse_selectors(const std::string& txt, const std::string& styles);
};
