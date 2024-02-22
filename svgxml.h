#pragma once

#include <vector>
#include <memory>

#include "pugixml.hpp"
// don't want to fork pugi repo on account of two lines!
#if !defined(PUGIXML_NO_XPATH) || !defined(PUGIXML_NO_EXCEPTIONS)
#error PUGIXML_NO_XPATH and PUGIXML_NO_EXCEPTIONS must be defined - edit pugiconfig.hpp or use -D switch
#endif

#include "ulib/stringutil.h"
#include "ulib/platformutil.h"
#include "ulib/fileutil.h"

struct PugiXMLWriter : public pugi::xml_writer
{
  IOStream& strm;
  PugiXMLWriter(IOStream& _strm) : strm(_strm) {}
  void write(const void* data, size_t size) override { strm.write(data, size); }
};

class XmlFragment
{
public:
  pugi::xml_document doc;
  XmlFragment(const pugi::xml_node& node) { doc.append_copy(node); }
  XmlFragment* clone() const { return new XmlFragment(doc.first_child()); }
  const char* name() const { return doc.first_child().name(); }
};

// TODO: remove "write" prefix from each method
class XmlStreamWriter
{
  pugi::xml_document doc;
  pugi::xml_node node;

  bool writeStyleAttr = false;
  std::string styleAttr;

  std::vector<char> temp;

public:
  int defaultFloatPrecision = 3;

  XmlStreamWriter() : temp(1024) { node = doc; }

  XmlStreamWriter& writeStartElement(const char* name) { node = node.append_child(name); return *this; }
  XmlStreamWriter& writeEndElement() { node = node.parent();  return *this; }
  XmlStreamWriter& writeAttribute(const char* name, const char* value)
  {
    if(writeStyleAttr) {
      styleAttr.append(name); styleAttr.append(":"); styleAttr.append(value); styleAttr.append(";");
    }
    else
      node.append_attribute(name).set_value(value);
    return *this;
  }

  XmlStreamWriter& writeAttribute(const char* name, const std::string& value)
  {
    return writeAttribute(name, value.c_str());
  }

  XmlStreamWriter& writeAttribute(const char* name, double value, int precision = -1)
  {
    // if value is an int, print as an int
    if(int(value) == value)
      return writeAttribute(name, int(value));

    char* buff = temp.data();
    int ii = realToStr(buff, value, precision < 0 ? defaultFloatPrecision : precision);
    buff[ii] = '\0';
    return writeAttribute(name, buff);
  }

  XmlStreamWriter& writeAttribute(const char* name, int value)
  {
    char* buff = temp.data();
    int ii = intToStr(buff, value);
    buff[ii] = '\0';
    return writeAttribute(name, buff);
  }

  XmlStreamWriter& writeCharacters(const char* text)
  {
    node.append_child(pugi::node_pcdata).set_value(text);
    return *this;
  }

  XmlStreamWriter& writeFragment(const XmlFragment& fragment)
  {
    node.append_copy(fragment.doc.first_child());
    return *this;
  }

  XmlStreamWriter& startStyleAttribute() { writeStyleAttr = true; return *this; }
  XmlStreamWriter& endStyleAttribute(const char* attribname = "style")
  {
    writeStyleAttr = false;
    if(!styleAttr.empty())
      writeAttribute(attribname, styleAttr.c_str());
    styleAttr.clear();
    return *this;
  }

  char* getTemp(size_t reserve = 0) { if(reserve > temp.size()) { temp.resize(reserve); } return temp.data(); }
  void saveFile(const char* filename, const char* indent = "  ")
  {
    FileStream strm(filename, "wb");
    save(strm, indent);  // let FileStream handle unicode on Windows instead of using pugi save_file wstring
  }

  void save(std::ostream& strm, const char* indent = "  ")
  {
    doc.save(strm, indent, pugi::format_default | pugi::format_no_declaration);
  }

  void save(IOStream& strm, const char* indent = "  ")
  {
    PugiXMLWriter writer(strm);
    doc.save(writer, indent, pugi::format_default | pugi::format_no_declaration);
  }
};

class XmlStreamAttribute
{
  pugi::xml_attribute attr;
public:
  XmlStreamAttribute(pugi::xml_attribute _attr) : attr(_attr) {}
  const char* name() const { return attr.name(); }
  const char* value() const { return attr.as_string(); }
  XmlStreamAttribute next() const { return XmlStreamAttribute(attr.next_attribute()); }
  operator bool() { return attr; }
};

// TODO: consider requiring that caller manage 'doc' instead of us!
class XmlStreamAttributes
{
protected:
  pugi::xml_node node;
  std::unique_ptr<pugi::xml_document> doc;
public:
  XmlStreamAttributes(pugi::xml_node _node) : node(_node), doc(nullptr) {}
  XmlStreamAttributes() : doc(new pugi::xml_document()) { node = doc->append_child("dummy"); }
  XmlStreamAttributes(XmlStreamAttributes&&) = default;

  const char* value(const char* name) const { return node.attribute(name).as_string(); }
  void append(const char* name, const char* value) { node.append_attribute(name).set_value(value); }
  XmlStreamAttribute firstAttribute() const { return XmlStreamAttribute(node.first_attribute()); }
};

class XmlStreamReader
{
  pugi::xml_document doc;
  pugi::xml_node topNode;
  pugi::xml_parse_result parseResult;
  std::vector<pugi::xml_node> nodes;
  bool starting;

public:
  enum TokenType {NoToken=0, StartDocument, EndDocument,
      StartElement, EndElement, CData, ProcessingInstruction, Comment, Other};
  enum { ParseDefault = pugi::parse_default, BufferInPlace = 0x10000000 };

  XmlStreamReader(const char* data, int len, unsigned int opts = ParseDefault) : starting(true)
  {
    parseResult = opts & BufferInPlace ?
        doc.load_buffer_inplace((void*)data, len, (opts & ~BufferInPlace)) : doc.load_buffer(data, len, opts);
    topNode = doc;
  }

  XmlStreamReader(std::istream& strm, unsigned int opts = ParseDefault) : starting(true)
  {
    doc.load(strm, opts);
    topNode = doc;
  }

  // for processing only a subtree of a document
  XmlStreamReader(const pugi::xml_node& node) : topNode(node), starting(true) {}

  int parseStatus() { return parseResult.status; }
  bool atEnd() { return tokenType() == EndDocument; }
  const char* name() { return nodes.back().name(); }
  const char* text() { return nodes.back().value(); }
  XmlStreamAttributes attributes() { return XmlStreamAttributes(nodes.back()); }

  // this can be used to store unrecognized nodes
  XmlFragment* readNodeAsFragment()
  {
    // this will cause next call to readNext() to advance to next sibling of current node
    starting = false;
    return nodes.empty() ? new XmlFragment(pugi::xml_node()) : new XmlFragment(nodes.back());
  }

  // depth-first traversal of doc; rules: elements on stack are always valid (i.e. never null)
  TokenType readNext()
  {
    if(nodes.empty()) {
      nodes.push_back(topNode);
    }
    else if(starting && nodes.back().first_child()) {
      nodes.push_back(nodes.back().first_child());
      starting = true;
    }
    else if(starting && nodes.back().type() == pugi::node_element)
      starting = false;
    else {
      // this should handle both node_element types and other node types with never have sibilings
      pugi::xml_node next = nodes.back().next_sibling();
      if(next) {
        nodes.back() = next;
        starting = true;
      }
      else {
        if(nodes.size() > 1)
          nodes.pop_back();
        starting = false;
      }
    }
    //PLATFORM_LOG("%s for %s", tokenNames[int(tokenType())], nodes.back().name());
    return tokenType();
  }

  TokenType tokenType()
  {
    if(nodes.empty())
      return NoToken;
    if(nodes.size() == 1)
      return starting ? StartDocument : EndDocument;
    pugi::xml_node_type type = nodes.back().type();
    if(type == pugi::node_element)
      return starting ? StartElement : EndElement;
    else if(type == pugi::node_pcdata || type == pugi::node_cdata)
      return CData;
    else if(type == pugi::node_pi)
      return ProcessingInstruction;
    else if(type == pugi::node_comment)
      return Comment;
    else
      return Other;
  }
};
