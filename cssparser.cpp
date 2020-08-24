// CSS parser based on https://github.com/litehtml/litehtml (BSD license)

#include "cssparser.h"
#include <algorithm>
#include <cctype>
#include <string.h>

static std::string& lcase(std::string& s)
{
  for(char& c : s)
    c = std::tolower(c);
  return s;
}

static void trim(std::string &s)
{
  size_t pos = s.find_first_not_of(" \n\r\t");
  if(pos != std::string::npos)
    s.erase(s.begin(), s.begin() + pos);
  pos = s.find_last_not_of(" \n\r\t");
  if(pos != std::string::npos)
    s.erase(s.begin() + pos + 1, s.end());
}

static std::string trimmed(const std::string& s)
{
  size_t bpos = s.find_first_not_of(" \n\r\t");
  if(bpos == std::string::npos) return "";
  size_t epos = s.find_last_not_of(" \n\r\t");
  return s.substr(bpos, epos == std::string::npos ? epos : epos - bpos + 1);
}

static size_t find_close_bracket(const std::string &s, size_t off, char open_b, char close_b)
{
  int cnt = 0;
  for(size_t i = off; i < s.length(); i++) {
    if(s[i] == open_b) {
      cnt++;
    } else if(s[i] == close_b) {
      cnt--;
      if(!cnt)
        return i;
    }
  }
  return std::string::npos;
}
static void split_string(const std::string& str, std::vector<std::string>& tokens,
    const std::string& delims, const std::string& delims_preserve = "", const std::string& quote = "\"")
{
  if(str.empty() || (delims.empty() && delims_preserve.empty()))
    return;

  std::string all_delims = delims + delims_preserve + quote;
  size_t token_start  = 0;
  size_t token_end  = str.find_first_of(all_delims, token_start);
  size_t token_len  = 0;
  std::string token;
  while(1) {
    while( token_end != std::string::npos && quote.find_first_of(str[token_end]) != std::string::npos )  {
      if(str[token_end] == '(')
        token_end = find_close_bracket(str, token_end, '(', ')');
      else if(str[token_end] == '[')
        token_end = find_close_bracket(str, token_end, '[', ']');
      else if(str[token_end] == '{')
        token_end = find_close_bracket(str, token_end, '{', '}');
      else
        token_end = str.find_first_of(str[token_end], token_end + 1);

      if(token_end != std::string::npos)
        token_end = str.find_first_of(all_delims, token_end + 1);
    }

    token_len = (token_end == std::string::npos) ? std::string::npos : token_end - token_start;
    token = str.substr(token_start, token_len);
    if(!token.empty())
      tokens.push_back( token );
    if(token_end != std::string::npos && !delims_preserve.empty() && delims_preserve.find_first_of(str[token_end]) != std::string::npos)
      tokens.push_back( str.substr(token_end, 1) );

    token_start = token_end;
    if(token_start == std::string::npos) break;
    token_start++;
    if(token_start == str.length()) break;
    token_end = str.find_first_of(all_delims, token_start);
  }
}

void css_element_selector::parse( const std::string& txt )
{
  size_t el_end = txt.find_first_of(".#[:");
  m_tag = txt.substr(0, el_end);
  lcase(m_tag);
  m_attrs.clear();
  while(el_end != std::string::npos) {
    if(txt[el_end] == '.') {
      css_attribute_selector attribute;
      size_t pos = txt.find_first_of(".#[:", el_end + 1);
      attribute.val   = txt.substr(el_end + 1, pos - el_end - 1);
      // I don't understand this ... multiple classes are added w/ .cls1.cls2.cls3, not spaces!
      //split_string( attribute.val, attribute.class_val, " " );
      attribute.condition = select_equal;
      attribute.attribute = "class";
      m_attrs.push_back(attribute);
      el_end = pos;
    } else if(txt[el_end] == ':') {
      css_attribute_selector attribute;
      if(txt[el_end + 1] == ':') {
        size_t pos = txt.find_first_of(".#[:", el_end + 2);
        attribute.val   = txt.substr(el_end + 2, pos - el_end - 2);
        attribute.condition = select_pseudo_element;
        lcase(attribute.val);
        attribute.attribute = "pseudo-el";
        m_attrs.push_back(attribute);
        el_end = pos;
      } else {
        size_t pos = txt.find_first_of(".#[:(", el_end + 1);
        if(pos != std::string::npos && txt.at(pos) == '(') {
          pos = find_close_bracket(txt, pos, '(', ')');
          if(pos != std::string::npos)
            pos++;
        }
        if(pos != std::string::npos)
          attribute.val = txt.substr(el_end + 1, pos - el_end - 1);
        else
          attribute.val = txt.substr(el_end + 1);
        lcase(attribute.val);
        bool is_pseudo_elem = attribute.val == "after" || attribute.val == "before";
        attribute.condition = is_pseudo_elem ? select_pseudo_element : select_pseudo_class;
        attribute.attribute = "pseudo";
        m_attrs.push_back(attribute);
        el_end = pos;
      }
    } else if(txt[el_end] == '#') {
      css_attribute_selector attribute;
      size_t pos = txt.find_first_of(".#[:", el_end + 1);
      attribute.val   = txt.substr(el_end + 1, pos - el_end - 1);
      attribute.condition = select_equal;
      attribute.attribute = "id";
      m_attrs.push_back(attribute);
      el_end = pos;
    } else if(txt[el_end] == '[') {
      css_attribute_selector attribute;
      size_t pos = txt.find_first_of("]~=|$*^", el_end + 1);
      std::string attr = txt.substr(el_end + 1, pos - el_end - 1);
      trim(attr);
      lcase(attr);
      if(pos != std::string::npos) {
        if(txt[pos] == ']')
          attribute.condition = select_exists;
        else if(txt[pos] == '=')
          attribute.condition = select_equal;
        else if(txt.substr(pos, 2) == "~=")
          attribute.condition = select_contain_str;
        else if(txt.substr(pos, 2) == "|=")
          attribute.condition = select_start_str;
        else if(txt.substr(pos, 2) == "^=")
          attribute.condition = select_start_str;
        else if(txt.substr(pos, 2) == "$=")
          attribute.condition = select_end_str;
        else if(txt.substr(pos, 2) == "*=")
          attribute.condition = select_contain_str;
        else
          attribute.condition = select_exists;

        if(txt[++pos] == '=') ++pos;

        pos = txt.find_first_not_of(" \t", pos);
        if(pos != std::string::npos) {
          if(txt[pos] == '"') {
            size_t pos2 = txt.find_first_of("\"", pos + 1);
            attribute.val = txt.substr(pos + 1, pos2 == std::string::npos ? pos2 : (pos2 - pos - 1));
            pos = pos2 == std::string::npos ? pos2 : (pos2 + 1);
          } else if(txt[pos] == ']') {
            pos++;
          } else {
            size_t pos2 = txt.find_first_of("]", pos + 1);
            attribute.val = txt.substr(pos, pos2 == std::string::npos ? pos2 : (pos2 - pos));
            trim(attribute.val);
            pos = pos2 == std::string::npos ? pos2 : (pos2 + 1);
          }
        }
      } else {
        attribute.condition = select_exists;
      }
      attribute.attribute = attr;
      m_attrs.push_back(attribute);
      el_end = pos;
    } else {
      el_end++;
    }
    el_end = txt.find_first_of(".#[:", el_end);
  }
}

bool css_selector::parse( const std::string& text )
{
  if(text.empty())
    return false;
  std::vector<std::string> tokens;
  split_string(text, tokens, "", " \t>+~", "([");
  if(tokens.empty())
    return false;

  std::string left;
  std::string right = tokens.back();
  char combinator = 0;
  tokens.pop_back();
  while(!tokens.empty() && (tokens.back() == " " || tokens.back() == "\t"
      || tokens.back() == "+" || tokens.back() == "~" || tokens.back() == ">")) {
    if(combinator == ' ' || combinator == 0)
      combinator = tokens.back()[0];
    tokens.pop_back();
  }

  for(auto i = tokens.begin(); i != tokens.end(); i++)
    left += *i;

  trim(left);
  trim(right);
  if(right.empty())
    return false;

  m_right.parse(right);
  switch(combinator) {
    case '>': m_combinator  = combinator_child;  break;
    case '+': m_combinator  = combinator_adjacent_sibling;  break;
    case '~': m_combinator  = combinator_general_sibling; break;
    default: m_combinator  = combinator_descendant; break;
  }

  m_left = 0;
  if(!left.empty()) {
    m_left = std::make_unique<css_selector>();
    if(!m_left->parse(left))
      return false;
  }
  return true;
}

int css_selector::calc_specificity()
{
  int b = 0, c = 0;
  int d = (!m_right.m_tag.empty() && m_right.m_tag != "*") ? 1 : 0;
  for(auto i = m_right.m_attrs.begin(); i != m_right.m_attrs.end(); i++)
    i->attribute == "id" ? b++ : c++;

  int specificity = b*0x10000 + c*0x100 + d;
  return m_left ? m_left->calc_specificity() + specificity : specificity;
}

void css_stylesheet::parse_selectors(const std::string& txt, const std::string& styles)
{
  std::string selstr = txt;
  trim(selstr);
  std::vector<std::string> tokens;
  split_string(selstr, tokens, ",");
  for(auto tok = tokens.begin(); tok != tokens.end(); tok++) {
    css_selector* selector = new css_selector;
    trim(*tok);
    if(selector->parse(*tok)) {
      m_rules.emplace_back(selector, createCssDecls(), m_rules.size());  // preserve order from source through sorting
      m_rules.back().parse_declarations(styles);
    }
  }
}

void css_stylesheet::parse_stylesheet(const char* str)  //, const char* baseurl)
{
  std::string text = str;
  // remove comments - how to do this because we do blind search for '}' after finding '{'
  size_t c_start = text.find("/*");
  while(c_start != std::string::npos) {
    size_t c_end = text.find("*/", c_start + 2);
    text.erase(c_start, c_end - c_start + 2);
    c_start = text.find("/*");
  }

  size_t pos = text.find_first_not_of(" \n\r\t");
  while(pos != std::string::npos) {
    /*while(pos != tstring::npos && text[pos] == '@')
    {
      size_t sPos = pos;
      pos = text.find_first_of("{;", pos);
      if(pos != tstring::npos && text[pos] == '{')
        pos = find_close_bracket(text, pos, '{', '}');

      if(pos != tstring::npos)
        parse_atrule(text.substr(sPos, pos - sPos + 1), baseurl, doc, media);
      else
        parse_atrule(text.substr(sPos), baseurl, doc, media);

      if(pos != tstring::npos)
        pos = text.find_first_not_of(" \n\r\t", pos + 1);
    }
    if(pos == tstring::npos)
      break;
    */
    size_t style_start = text.find("{", pos);
    size_t style_end  = text.find("}", pos);
    if(style_start != std::string::npos && style_end != std::string::npos) {
      std::string stylestr = text.substr(style_start + 1, style_end - style_start - 1);
      parse_selectors(text.substr(pos, style_start - pos), stylestr);
      pos = style_end + 1;
    } else {
      pos = std::string::npos;
    }
    if(pos != std::string::npos)
      pos = text.find_first_not_of(" \n\r\t", pos);
  }
}

// this should be called once after parsing all stylesheets
void css_stylesheet::sort_rules()
{
  // Note that we use '>' instead of '<' to sort from high to low priority
  std::sort(m_rules.begin(), m_rules.end(), [](const css_rule& v1, const css_rule& v2) {
    return (v1.m_specificity == v2.m_specificity) ? (v1.m_order > v2.m_order) : (v1.m_specificity > v2.m_specificity);
  });
}

/// css_rule ///

void css_rule::parse_declarations(const std::string& stylestr)
{
  std::vector<std::string> styles;
  split_string(stylestr, styles, ";");
  for(std::string& style : styles) {
    size_t colon = style.find(':');
    if(colon != std::string::npos) {
      std::string name(style, 0, colon);
      std::string value(style, colon+1);
      m_decls->parseDecl(trimmed(name).c_str(), trimmed(value).c_str());
    }
  }
}

// matching code from litehtml html_tag.cpp

bool css_rule::element_select(void* el, const css_element_selector& selector) const
{
  if(!selector.m_tag.empty() && selector.m_tag != "*" && !m_decls->isTag(el, selector.m_tag.c_str()))
    return false;

  for(auto i = selector.m_attrs.begin(); i != selector.m_attrs.end(); i++) {
    if(i->condition == select_equal && i->attribute == "class") {
      if(!m_decls->hasClass(el, i->val.c_str()))
        return false;
    } else if(i->condition == select_equal && i->attribute == "id") {
      if(!m_decls->hasId(el, i->val.c_str()))
        return false;
    } else {
      const char* attr_value = m_decls->attribute(el, i->attribute.c_str());
      if(!attr_value)
        return false;
      std::string attr_str(attr_value);
      lcase(attr_str);
      switch(i->condition) {
      case select_exists:
        break;
      case select_equal:
        if(attr_str != i->val)
          return false;
        break;
      case select_contain_str:
        if(!strstr(attr_str.c_str(), i->val.c_str()))
          return false;
        break;
      case select_start_str:
        if(strncmp(attr_str.c_str(), i->val.c_str(), i->val.length()) != 0)
          return false;
        break;
      case select_end_str:
        if(i->val.size() > attr_str.size()
            || strcmp(attr_str.c_str() + attr_str.size() - i->val.size(), i->val.c_str()) != 0)
          return false;
        break;
      default:
        return false;
      }
    }
  }
  return true;
}

bool css_rule::select_ancestor(void* el, const css_selector& selector) const
{
  return el && (select(el, selector) || select_ancestor(m_decls->parent(el), selector));
}

bool css_rule::select(void* el, const css_selector& selector) const
{
  if(!element_select(el, selector.m_right))
    return false;

  if(selector.m_left) {
    void* el_parent = m_decls->parent(el);
    if (!el_parent)
      return false;
    switch(selector.m_combinator) {
    case combinator_descendant:
      if(!select_ancestor(el_parent, *selector.m_left))
        return false;
      break;
    case combinator_child:
      if(!select(el_parent, *selector.m_left))
        return false;
      break;
    //case combinator_adjacent_sibling:
    //case combinator_general_sibling:
    default:
      return false;
    }
  }
  return true;
}
