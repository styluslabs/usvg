# simple C/C++ makefile

TARGET = usvgtest
#TARGET = svgconcat
SOURCES = \
  svgnode.cpp \
  svgstyleparser.cpp \
  svgparser.cpp \
  svgpainter.cpp \
  svgwriter.cpp \
  cssparser.cpp \
  test/usvgtest.cpp
#  test/svgconcat.cpp

SOURCES += \
  ../pugixml/src/pugixml.cpp \
  ../ulib/geom.cpp \
  ../ulib/image.cpp \
  ../ulib/path2d.cpp \
  ../ulib/painter.cpp \
  ../nanovgXC/src/nanovg.c \

#SOURCES += \
#  ../miniz/miniz.c \
#  ../miniz/miniz_tdef.c \
#  ../miniz/miniz_tinfl.c

# if source contains ../ paths, this should be the <current> directory; if ../../, <parent>/<current>; etc.
# - this ensures object files remain under build directory
TOPDIR = usvg

INC = . .. ../nanovgXC/src
INCSYS = ../pugixml/src ../stb
DEFS = PUGIXML_NO_XPATH PUGIXML_NO_EXCEPTIONS NO_PAINTER_GL NO_MINIZ

LIBS =

include Makefile.unix
