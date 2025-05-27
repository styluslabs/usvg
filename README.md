# usvg #

usvg? μsvg? micro-svg?

C++ SVG library: reading, writing, modifying, and rendering.  Does not support entire SVG specification but should handle most documents correctly.

### Features ###

* basic CSS support, including dynamic styling
* SVG font support
* [extension mechanism](svgnode.h#L100) enabling custom behavior - used by [ugui](https://github.com/styluslabs/ugui) to implement a SVG-based GUI
* dirty rectangle/damage tracking
* preserves document structure, including unrecognized attributes and elements

### Example ###

On Linux, `git clone --recurse-submodules` [Write](https://github.com/styluslabs/Write), then `cd usvg && make` to generate `Release/usvgtest`.  Run `usvgtest <somefile>.svg` to save an image to <somefile>_out.png and save SVG to <somefile>_out.svg.  This will also compare the rendered image to <somefile>_ref.png if present.

The makefile currently only supports Linux, but adding other platforms is straightforward - see Makefile in  [nanovgXC](https://github.com/styluslabs/nanovgXC)

## Dependencies ##

* https://github.com/styluslabs/ulib - basic classes: Path2D, Transform2D, etc.
* https://github.com/styluslabs/nanovgXC - rendering
* [pugixml](https://github.com/zeux/pugixml) - XML reading and writing
* [stb](https://github.com/nothings/stb) libraries - stb_image, stb_image_write for image I/O
