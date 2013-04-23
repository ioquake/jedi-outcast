/* $Header: /roq/tiff/tif_getimage.c 1     11/02/99 4:39p Zaphod $ */


/*

 * Copyright (c) 1991-1996 Sam Leffler

 * Copyright (c) 1991-1996 Silicon Graphics, Inc.

 *

 * Permission to use, copy, modify, distribute, and sell this software and 

 * its documentation for any purpose is hereby granted without fee, provided

 * that (i) the above copyright notices and this permission notice appear in

 * all copies of the software and related documentation, and (ii) the names of

 * Sam Leffler and Silicon Graphics may not be used in any advertising or

 * publicity relating to the software without the specific, prior written

 * permission of Sam Leffler and Silicon Graphics.

 * 

 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND, 

 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY 

 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  

 * 

 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR

 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,

 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,

 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF 

 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE 

 * OF THIS SOFTWARE.

 */



/*

 * TIFF Library

 *

 * Read and return a packed RGBA image.

 */

#include "tiffiop.h"

#include <assert.h>

#include <stdio.h>



static	int gtTileContig(TIFFRGBAImage*, uint32*, uint32, uint32);

static	int gtTileSeparate(TIFFRGBAImage*, uint32*, uint32, uint32);

static	int gtStripContig(TIFFRGBAImage*, uint32*, uint32, uint32);

static	int gtStripSeparate(TIFFRGBAImage*, uint32*, uint32, uint32);

static	int pickTileContigCase(TIFFRGBAImage*);

static	int pickTileSeparateCase(TIFFRGBAImage*);



static	const char photoTag[] = "PhotometricInterpretation";



/*

 * Check the image to see if TIFFReadRGBAImage can deal with it.

 * 1/0 is returned according to whether or not the image can

 * be handled.  If 0 is returned, emsg contains the reason

 * why it is being rejected.

 */

int

TIFFRGBAImageOK(TIFF* tif, char emsg[1024])

{

    TIFFDirectory* td = &tif->tif_dir;

    uint16 photometric;

    int colorchannels;



    switch (td->td_bitspersample) {

    case 1: case 2: case 4:

    case 8: case 16:

	break;

    default:

	sprintf(emsg, "Sorry, can not handle images with %d-bit samples",

	    td->td_bitspersample);

	return (0);

    }

    colorchannels = td->td_samplesperpixel - td->td_extrasamples;

    if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &photometric)) {

	switch (colorchannels) {

	case 1:

	    photometric = PHOTOMETRIC_MINISBLACK;

	    break;

	case 3:

	    photometric = PHOTOMETRIC_RGB;

	    break;

	default:

	    sprintf(emsg, "Missing needed %s tag", photoTag);

	    return (0);

	}

    }

    switch (photometric) {

    case PHOTOMETRIC_MINISWHITE:

    case PHOTOMETRIC_MINISBLACK:

    case PHOTOMETRIC_PALETTE:

	if (td->td_planarconfig == PLANARCONFIG_CONTIG && td->td_samplesperpixel != 1) {

	    sprintf(emsg,

		"Sorry, can not handle contiguous data with %s=%d, and %s=%d",

		photoTag, photometric,

		"Samples/pixel", td->td_samplesperpixel);

	    return (0);

	}

	break;

    case PHOTOMETRIC_YCBCR:

	if (td->td_planarconfig != PLANARCONFIG_CONTIG) {

	    sprintf(emsg, "Sorry, can not handle YCbCr images with %s=%d",

		"Planarconfiguration", td->td_planarconfig);

	    return (0);

	}

	break;

    case PHOTOMETRIC_RGB: 

	if (colorchannels < 3) {

	    sprintf(emsg, "Sorry, can not handle RGB image with %s=%d",

		"Color channels", colorchannels);

	    return (0);

	}

	break;

#ifdef CMYK_SUPPORT

    case PHOTOMETRIC_SEPARATED:

	if (td->td_inkset != INKSET_CMYK) {

	    sprintf(emsg, "Sorry, can not handle separated image with %s=%d",

		"InkSet", td->td_inkset);

	    return (0);

	}

	if (td->td_samplesperpixel != 4) {

	    sprintf(emsg, "Sorry, can not handle separated image with %s=%d",

		"Samples/pixel", td->td_samplesperpixel);

	    return (0);

	}

	break;

#endif

    default:

	sprintf(emsg, "Sorry, can not handle image with %s=%d",

	    photoTag, photometric);

	return (0);

    }

    return (1);

}



void

TIFFRGBAImageEnd(TIFFRGBAImage* img)

{

    if (img->Map)

	_TIFFfree(img->Map), img->Map = NULL;

    if (img->BWmap)

	_TIFFfree(img->BWmap), img->BWmap = NULL;

    if (img->PALmap)

	_TIFFfree(img->PALmap), img->PALmap = NULL;

    if (img->ycbcr)

	_TIFFfree(img->ycbcr), img->ycbcr = NULL;

}



static int

isCCITTCompression(TIFF* tif)

{

    uint16 compress;

    TIFFGetField(tif, TIFFTAG_COMPRESSION, &compress);

    return (compress == COMPRESSION_CCITTFAX3 ||

	    compress == COMPRESSION_CCITTFAX4 ||

	    compress == COMPRESSION_CCITTRLE ||

	    compress == COMPRESSION_CCITTRLEW);

}



int

TIFFRGBAImageBegin(TIFFRGBAImage* img, TIFF* tif, int stop, char emsg[1024])

{

    uint16* sampleinfo;

    uint16 extrasamples;

    uint16 planarconfig;

    int colorchannels;



    img->tif = tif;

    img->stoponerr = stop;

    TIFFGetFieldDefaulted(tif, TIFFTAG_BITSPERSAMPLE, &img->bitspersample);

    switch (img->bitspersample) {

    case 1: case 2: case 4:

    case 8: case 16:

	break;

    default:

	sprintf(emsg, "Sorry, can not image with %d-bit samples",

	    img->bitspersample);

	return (0);

    }

    img->alpha = 0;

    TIFFGetFieldDefaulted(tif, TIFFTAG_SAMPLESPERPIXEL, &img->samplesperpixel);

    TIFFGetFieldDefaulted(tif, TIFFTAG_EXTRASAMPLES,

	&extrasamples, &sampleinfo);

    if (extrasamples == 1)

	switch (sampleinfo[0]) {

	case EXTRASAMPLE_ASSOCALPHA:	/* data is pre-multiplied */

	case EXTRASAMPLE_UNASSALPHA:	/* data is not pre-multiplied */

	    img->alpha = sampleinfo[0];

	    break;

	}

    colorchannels = img->samplesperpixel - extrasamples;

    TIFFGetFieldDefaulted(tif, TIFFTAG_PLANARCONFIG, &planarconfig);

    if (!TIFFGetField(tif, TIFFTAG_PHOTOMETRIC, &img->photometric)) {

	switch (colorchannels) {

	case 1:

	    if (isCCITTCompression(tif))

		img->photometric = PHOTOMETRIC_MINISWHITE;

	    else

		img->photometric = PHOTOMETRIC_MINISBLACK;

	    break;

	case 3:

	    img->photometric = PHOTOMETRIC_RGB;

	    break;

	default:

	    sprintf(emsg, "Missing needed %s tag", photoTag);

	    return (0);

	}

    }

    switch (img->photometric) {

    case PHOTOMETRIC_PALETTE:

	if (!TIFFGetField(tif, TIFFTAG_COLORMAP,

	    &img->redcmap, &img->greencmap, &img->bluecmap)) {

	    TIFFError(TIFFFileName(tif), "Missing required \"Colormap\" tag");

	    return (0);

	}

	/* fall thru... */

    case PHOTOMETRIC_MINISWHITE:

    case PHOTOMETRIC_MINISBLACK:

	if (planarconfig == PLANARCONFIG_CONTIG && img->samplesperpixel != 1) {

	    sprintf(emsg,

		"Sorry, can not handle contiguous data with %s=%d, and %s=%d",

		photoTag, img->photometric,

		"Samples/pixel", img->samplesperpixel);

	    return (0);

	}

	break;

    case PHOTOMETRIC_YCBCR:

	if (planarconfig != PLANARCONFIG_CONTIG) {

	    sprintf(emsg, "Sorry, can not handle YCbCr images with %s=%d",

		"Planarconfiguration", planarconfig);

	    return (0);

	}

	/* It would probably be nice to have a reality check here. */

	{ uint16 compress;

	  TIFFGetField(tif, TIFFTAG_COMPRESSION, &compress);

	  if (compress == COMPRESSION_JPEG && planarconfig == PLANARCONFIG_CONTIG) {

	    /* can rely on libjpeg to convert to RGB */

	    /* XXX should restore current state on exit */

	    TIFFSetField(tif, TIFFTAG_JPEGCOLORMODE, JPEGCOLORMODE_RGB);

	    img->photometric = PHOTOMETRIC_RGB;

	  }

	}

	break;

    case PHOTOMETRIC_RGB: 

	if (colorchannels < 3) {

	    sprintf(emsg, "Sorry, can not handle RGB image with %s=%d",

		"Color channels", colorchannels);

	    return (0);

	}

	break;

    case PHOTOMETRIC_SEPARATED: {

	uint16 inkset;

	TIFFGetFieldDefaulted(tif, TIFFTAG_INKSET, &inkset);

	if (inkset != INKSET_CMYK) {

	    sprintf(emsg, "Sorry, can not handle separated image with %s=%d",

		"InkSet", inkset);

	    return (0);

	}

	if (img->samplesperpixel != 4) {

	    sprintf(emsg, "Sorry, can not handle separated image with %s=%d",

		"Samples/pixel", img->samplesperpixel);

	    return (0);

	}

	break;

    }

    default:

	sprintf(emsg, "Sorry, can not handle image with %s=%d",

	    photoTag, img->photometric);

	return (0);

    }

    img->Map = NULL;

    img->BWmap = NULL;

    img->PALmap = NULL;

    img->ycbcr = NULL;

    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &img->width);

    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &img->height);

    TIFFGetFieldDefaulted(tif, TIFFTAG_ORIENTATION, &img->orientation);

    img->isContig =

	!(planarconfig == PLANARCONFIG_SEPARATE && colorchannels > 1);

    if (img->isContig) {

	img->get = TIFFIsTiled(tif) ? gtTileContig : gtStripContig;

	(void) pickTileContigCase(img);

    } else {

	img->get = TIFFIsTiled(tif) ? gtTileSeparate : gtStripSeparate;

	(void) pickTileSeparateCase(img);

    }

    return (1);

}



int

TIFFRGBAImageGet(TIFFRGBAImage* img, uint32* raster, uint32 w, uint32 h)

{

    if (img->get == NULL) {

	TIFFError(TIFFFileName(img->tif), "No \"get\" routine setup");

	return (0);

    }

    if (img->put.any == NULL) {

	TIFFError(TIFFFileName(img->tif),

	    "No \"put\" routine setupl; probably can not handle image format");

	return (0);

    }

    return (*img->get)(img, raster, w, h);

}



/*

 * Read the specified image into an ABGR-format raster.

 */

int

TIFFReadRGBAImage(TIFF* tif,

    uint32 rwidth, uint32 rheight, uint32* raster, int stop)

{

    char emsg[1024];

    TIFFRGBAImage img;

    int ok;



    if (TIFFRGBAImageBegin(&img, tif, stop, emsg)) {

	/* XXX verify rwidth and rheight against width and height */

	ok = TIFFRGBAImageGet(&img, raster+(rheight-img.height)*rwidth,

	    rwidth, img.height);

	TIFFRGBAImageEnd(&img);

    } else {

	TIFFError(TIFFFileName(tif), emsg);

	ok = 0;

    }

    return (ok);

}



static uint32

setorientation(TIFFRGBAImage* img, uint32 h)

{

    TIFF* tif = img->tif;

    uint32 y;



    switch (img->orientation) {

    case ORIENTATION_BOTRIGHT:

    case ORIENTATION_RIGHTBOT:	/* XXX */

    case ORIENTATION_LEFTBOT:	/* XXX */

	TIFFWarning(TIFFFileName(tif), "using bottom-left orientation");

	img->orientation = ORIENTATION_BOTLEFT;

	/* fall thru... */

    case ORIENTATION_BOTLEFT:

	y = 0;

	break;

    case ORIENTATION_TOPRIGHT:

    case ORIENTATION_RIGHTTOP:	/* XXX */

    case ORIENTATION_LEFTTOP:	/* XXX */

    default:

	TIFFWarning(TIFFFileName(tif), "using top-left orientation");

	img->orientation = ORIENTATION_TOPLEFT;

	/* fall thru... */

    case ORIENTATION_TOPLEFT:

	y = h-1;

	break;

    }

    return (y);

}



/*

 * Get an tile-organized image that has

 *	PlanarConfiguration contiguous if SamplesPerPixel > 1

 * or

 *	SamplesPerPixel == 1

 */	

static int

gtTileContig(TIFFRGBAImage* img, uint32* raster, uint32 w, uint32 h)

{

    TIFF* tif = img->tif;

    tileContigRoutine put = img->put.contig;

    uint16 orientation;

    uint32 col, row, y;

    uint32 tw, th;

    u_char* buf;

    int32 fromskew, toskew;

    uint32 nrow;



    buf = (u_char*) _TIFFmalloc(TIFFTileSize(tif));

    if (buf == 0) {

	TIFFError(TIFFFileName(tif), "No space for tile buffer");

	return (0);

    }

    TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tw);

    TIFFGetField(tif, TIFFTAG_TILELENGTH, &th);

    y = setorientation(img, h);

    orientation = img->orientation;

    toskew = -(int32) (orientation == ORIENTATION_TOPLEFT ? tw+w : tw-w);

    for (row = 0; row < h; row += th) {

	nrow = (row + th > h ? h - row : th);

	for (col = 0; col < w; col += tw) {

	    if (TIFFReadTile(tif, buf, col, row, 0, 0) < 0 && img->stoponerr)

		break;

	    if (col + tw > w) {

		/*

		 * Tile is clipped horizontally.  Calculate

		 * visible portion and skewing factors.

		 */

		uint32 npix = w - col;

		fromskew = tw - npix;

		(*put)(img, raster+y*w+col, col, y,

		    npix, nrow, fromskew, toskew + fromskew, buf);

	    } else {

		(*put)(img, raster+y*w+col, col, y, tw, nrow, 0, toskew, buf);

	    }

	}

	y += (orientation == ORIENTATION_TOPLEFT ?

	    -(int32) nrow : (int32) nrow);

    }

    _TIFFfree(buf);

    return (1);

}



/*

 * Get an tile-organized image that has

 *	 SamplesPerPixel > 1

 *	 PlanarConfiguration separated

 * We assume that all such images are RGB.

 */	

static int

gtTileSeparate(TIFFRGBAImage* img, uint32* raster, uint32 w, uint32 h)

{

    TIFF* tif = img->tif;

    tileSeparateRoutine put = img->put.separate;

    uint16 orientation;

    uint32 col, row, y;

    uint32 tw, th;

    u_char* buf;

    u_char* r;

    u_char* g;

    u_char* b;

    u_char* a;

    tsize_t tilesize;

    int32 fromskew, toskew;

    int alpha = img->alpha;

    uint32 nrow;



    tilesize = TIFFTileSize(tif);

    buf = (u_char*) _TIFFmalloc(4*tilesize);

    if (buf == 0) {

	TIFFError(TIFFFileName(tif), "No space for tile buffer");

	return (0);

    }

    r = buf;

    g = r + tilesize;

    b = g + tilesize;

    a = b + tilesize;

    if (!alpha)

	memset(a, 0xff, tilesize);

    TIFFGetField(tif, TIFFTAG_TILEWIDTH, &tw);

    TIFFGetField(tif, TIFFTAG_TILELENGTH, &th);

    y = setorientation(img, h);

    orientation = img->orientation;

    toskew = -(int32) (orientation == ORIENTATION_TOPLEFT ? tw+w : tw-w);

    for (row = 0; row < h; row += th) {

	nrow = (row + th > h ? h - row : th);

	for (col = 0; col < w; col += tw) {

	    if (TIFFReadTile(tif, r, col, row,0,0) < 0 && img->stoponerr)

		break;

	    if (TIFFReadTile(tif, g, col, row,0,1) < 0 && img->stoponerr)

		break;

	    if (TIFFReadTile(tif, b, col, row,0,2) < 0 && img->stoponerr)

		break;

	    if (alpha && TIFFReadTile(tif,a,col,row,0,3) < 0 && img->stoponerr)

		break;

	    if (col + tw > w) {

		/*

		 * Tile is clipped horizontally.  Calculate

		 * visible portion and skewing factors.

		 */

		uint32 npix = w - col;

		fromskew = tw - npix;

		(*put)(img, raster+y*w+col, col, y,

		    npix, nrow, fromskew, toskew + fromskew, r, g, b, a);

	    } else {

		(*put)(img, raster+y*w+col, col, y,

		    tw, nrow, 0, toskew, r, g, b, a);

	    }

	}

	y += (orientation == ORIENTATION_TOPLEFT ?

	    -(int32) nrow : (int32) nrow);

    }

    _TIFFfree(buf);

    return (1);

}



/*

 * Get a strip-organized image that has

 *	PlanarConfiguration contiguous if SamplesPerPixel > 1

 * or

 *	SamplesPerPixel == 1

 */	

static int

gtStripContig(TIFFRGBAImage* img, uint32* raster, uint32 w, uint32 h)

{

    TIFF* tif = img->tif;

    tileContigRoutine put = img->put.contig;

    uint16 orientation;

    uint32 row, y, nrow;

    u_char* buf;

    uint32 rowsperstrip;

    uint32 imagewidth = img->width;

    tsize_t scanline;

    int32 fromskew, toskew;



    buf = (u_char*) _TIFFmalloc(TIFFStripSize(tif));

    if (buf == 0) {

	TIFFError(TIFFFileName(tif), "No space for strip buffer");

	return (0);

    }

    y = setorientation(img, h);

    orientation = img->orientation;

    toskew = -(int32) (orientation == ORIENTATION_TOPLEFT ? w+w : w-w);

    TIFFGetFieldDefaulted(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);

    scanline = TIFFScanlineSize(tif);

    fromskew = (w < imagewidth ? imagewidth - w : 0);

    for (row = 0; row < h; row += rowsperstrip) {

	nrow = (row + rowsperstrip > h ? h - row : rowsperstrip);

	if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, row, 0),

	    buf, nrow*scanline) < 0 && img->stoponerr)

		break;

	(*put)(img, raster+y*w, 0, y, w, nrow, fromskew, toskew, buf);

	y += (orientation == ORIENTATION_TOPLEFT ?

	    -(int32) nrow : (int32) nrow);

    }

    _TIFFfree(buf);

    return (1);

}



/*

 * Get a strip-organized image with

 *	 SamplesPerPixel > 1

 *	 PlanarConfiguration separated

 * We assume that all such images are RGB.

 */

static int

gtStripSeparate(TIFFRGBAImage* img, uint32* raster, uint32 w, uint32 h)

{

    TIFF* tif = img->tif;

    tileSeparateRoutine put = img->put.separate;

    uint16 orientation;

    u_char *buf;

    u_char *r, *g, *b, *a;

    uint32 row, y, nrow;

    tsize_t scanline;

    uint32 rowsperstrip;

    uint32 imagewidth = img->width;

    tsize_t stripsize;

    int32 fromskew, toskew;

    int alpha = img->alpha;



    stripsize = TIFFStripSize(tif);

    r = buf = (u_char *)_TIFFmalloc(4*stripsize);

    if (buf == 0) {

	TIFFError(TIFFFileName(tif), "No space for tile buffer");

	return (0);

    }

    g = r + stripsize;

    b = g + stripsize;

    a = b + stripsize;

    if (!alpha)

	memset(a, 0xff, stripsize);

    y = setorientation(img, h);

    orientation = img->orientation;

    toskew = -(int32) (orientation == ORIENTATION_TOPLEFT ? w+w : w-w);

    TIFFGetFieldDefaulted(tif, TIFFTAG_ROWSPERSTRIP, &rowsperstrip);

    scanline = TIFFScanlineSize(tif);

    fromskew = (w < imagewidth ? imagewidth - w : 0);

    for (row = 0; row < h; row += rowsperstrip) {

	nrow = (row + rowsperstrip > h ? h - row : rowsperstrip);

	if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, row, 0),

	    r, nrow*scanline) < 0 && img->stoponerr)

	    break;

	if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, row, 1),

	    g, nrow*scanline) < 0 && img->stoponerr)

	    break;

	if (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, row, 2),

	    b, nrow*scanline) < 0 && img->stoponerr)

	    break;

	if (alpha &&

	    (TIFFReadEncodedStrip(tif, TIFFComputeStrip(tif, row, 3),

	    a, nrow*scanline) < 0 && img->stoponerr))

	    break;

	(*put)(img, raster+y*w, 0, y, w, nrow, fromskew, toskew, r, g, b, a);

	y += (orientation == ORIENTATION_TOPLEFT ?

	    -(int32) nrow : (int32) nrow);

    }

    _TIFFfree(buf);

    return (1);

}



/*

 * The following routines move decoded data returned

 * from the TIFF library into rasters filled with packed

 * ABGR pixels (i.e. suitable for passing to lrecwrite.)

 *

 * The routines have been created according to the most

 * important cases and optimized.  pickTileContigCase and

 * pickTileSeparateCase analyze the parameters and select

 * the appropriate "put" routine to use.

 */

#define	REPEAT8(op)	REPEAT4(op); REPEAT4(op)

#define	REPEAT4(op)	REPEAT2(op); REPEAT2(op)

#define	REPEAT2(op)	op; op

#define	CASE8(x,op)			\
    switch (x) {			\
    case 7: op; case 6: op; case 5: op;	\
    case 4: op; case 3: op; case 2: op;	\
    case 1: op;				\
    }

#define	CASE4(x,op)	switch (x) { case 3: op; case 2: op; case 1: op; }

#define	NOP



#define	UNROLL8(w, op1, op2) {		\
    uint32 _x;				\
    for (_x = w; _x >= 8; _x -= 8) {	\
	op1;				\
	REPEAT8(op2);			\
    }					\
    if (_x > 0) {			\
	op1;				\
	CASE8(_x,op2);			\
    }					\
}

#define	UNROLL4(w, op1, op2) {		\
    uint32 _x;				\
    for (_x = w; _x >= 4; _x -= 4) {	\
	op1;				\
	REPEAT4(op2);			\
    }					\
    if (_x > 0) {			\
	op1;				\
	CASE4(_x,op2);			\
    }					\
}

#define	UNROLL2(w, op1, op2) {		\
    uint32 _x;				\
    for (_x = w; _x >= 2; _x -= 2) {	\
	op1;				\
	REPEAT2(op2);			\
    }					\
    if (_x) {				\
	op1;				\
	op2;				\
    }					\
}

    

#define	SKEW(r,g,b,skew)	{ r += skew; g += skew; b += skew; }

#define	SKEW4(r,g,b,a,skew)	{ r += skew; g += skew; b += skew; a+= skew; }



#define A1 ((uint32)(0xffL<<24))

#define	PACK(r,g,b)	\
	((uint32)(r)|((uint32)(g)<<8)|((uint32)(b)<<16)|A1)

#define	PACK4(r,g,b,a)	\
	((uint32)(r)|((uint32)(g)<<8)|((uint32)(b)<<16)|((uint32)(a)<<24))

#define W2B(v) (((v)>>8)&0xff)

#define	PACKW(r,g,b)	\
	((uint32)W2B(r)|((uint32)W2B(g)<<8)|((uint32)W2B(b)<<16)|A1)

#define	PACKW4(r,g,b,a)	\
	((uint32)W2B(r)|((uint32)W2B(g)<<8)|((uint32)W2B(b)<<16)|((uint32)W2B(a)<<24))



#define	DECLAREContigPutFunc(name) \
static void name(\
    TIFFRGBAImage* img, \
    uint32* cp, \
    uint32 x, uint32 y, \
    uint32 w, uint32 h, \
    int32 fromskew, int32 toskew, \
    u_char* pp \
)



/*

 * 8-bit palette => colormap/RGB

 */

DECLAREContigPutFunc(put8bitcmaptile)

{

    uint32** PALmap = img->PALmap;



    (void) x; (void) y;

    while (h-- > 0) {

	UNROLL8(w, NOP, *cp++ = PALmap[*pp++][0]);

	cp += toskew;

	pp += fromskew;

    }

}



/*

 * 4-bit palette => colormap/RGB

 */

DECLAREContigPutFunc(put4bitcmaptile)

{

    uint32** PALmap = img->PALmap;



    (void) x; (void) y;

    fromskew /= 2;

    while (h-- > 0) {

	uint32* bw;

	UNROLL2(w, bw = PALmap[*pp++], *cp++ = *bw++);

	cp += toskew;

	pp += fromskew;

    }

}



/*

 * 2-bit palette => colormap/RGB

 */

DECLAREContigPutFunc(put2bitcmaptile)

{

    uint32** PALmap = img->PALmap;



    (void) x; (void) y;

    fromskew /= 4;

    while (h-- > 0) {

	uint32* bw;

	UNROLL4(w, bw = PALmap[*pp++], *cp++ = *bw++);

	cp += toskew;

	pp += fromskew;

    }

}



/*

 * 1-bit palette => colormap/RGB

 */

DECLAREContigPutFunc(put1bitcmaptile)

{

    uint32** PALmap = img->PALmap;



    (void) x; (void) y;

    fromskew /= 8;

    while (h-- > 0) {

	uint32* bw;

	UNROLL8(w, bw = PALmap[*pp++], *cp++ = *bw++);

	cp += toskew;

	pp += fromskew;

    }

}



/*

 * 8-bit greyscale => colormap/RGB

 */

DECLAREContigPutFunc(putgreytile)

{

    uint32** BWmap = img->BWmap;



    (void) y;

    while (h-- > 0) {

	for (x = w; x-- > 0;)

	    *cp++ = BWmap[*pp++][0];

	cp += toskew;

	pp += fromskew;

    }

}



/*

 * 1-bit bilevel => colormap/RGB

 */

DECLAREContigPutFunc(put1bitbwtile)

{

    uint32** BWmap = img->BWmap;



    (void) x; (void) y;

    fromskew /= 8;

    while (h-- > 0) {

	uint32* bw;

	UNROLL8(w, bw = BWmap[*pp++], *cp++ = *bw++);

	cp += toskew;

	pp += fromskew;

    }

}



/*

 * 2-bit greyscale => colormap/RGB

 */

DECLAREContigPutFunc(put2bitbwtile)

{

    uint32** BWmap = img->BWmap;



    (void) x; (void) y;

    fromskew /= 4;

    while (h-- > 0) {

	uint32* bw;

	UNROLL4(w, bw = BWmap[*pp++], *cp++ = *bw++);

	cp += toskew;

	pp += fromskew;

    }

}



/*

 * 4-bit greyscale => colormap/RGB

 */

DECLAREContigPutFunc(put4bitbwtile)

{

    uint32** BWmap = img->BWmap;



    (void) x; (void) y;

    fromskew /= 2;

    while (h-- > 0) {

	uint32* bw;

	UNROLL2(w, bw = BWmap[*pp++], *cp++ = *bw++);

	cp += toskew;

	pp += fromskew;

    }

}



/*

 * 8-bit packed samples, no Map => RGB

 */

DECLAREContigPutFunc(putRGBcontig8bittile)

{

    int samplesperpixel = img->samplesperpixel;



    (void) x; (void) y;

    fromskew *= samplesperpixel;

    while (h-- > 0) {

	UNROLL8(w, NOP,

	    *cp++ = PACK(pp[0], pp[1], pp[2]);

	    pp += samplesperpixel);

	cp += toskew;

	pp += fromskew;

    }

}



/*

 * 8-bit packed samples, w/ Map => RGB

 */

DECLAREContigPutFunc(putRGBcontig8bitMaptile)

{

    TIFFRGBValue* Map = img->Map;

    int samplesperpixel = img->samplesperpixel;



    (void) y;

    fromskew *= samplesperpixel;

    while (h-- > 0) {

	for (x = w; x-- > 0;) {

	    *cp++ = PACK(Map[pp[0]], Map[pp[1]], Map[pp[2]]);

	    pp += samplesperpixel;

	}

	pp += fromskew;

	cp += toskew;

    }

}



/*

 * 8-bit packed samples => RGBA w/ associated alpha

 * (known to have Map == NULL)

 */

DECLAREContigPutFunc(putRGBAAcontig8bittile)

{

    int samplesperpixel = img->samplesperpixel;



    (void) x; (void) y;

    fromskew *= samplesperpixel;

    while (h-- > 0) {

	UNROLL8(w, NOP,

	    *cp++ = PACK4(pp[0], pp[1], pp[2], pp[3]);

	    pp += samplesperpixel);

	cp += toskew;

	pp += fromskew;

    }

}



/*

 * 8-bit packed samples => RGBA w/ unassociated alpha

 * (known to have Map == NULL)

 */

DECLAREContigPutFunc(putRGBUAcontig8bittile)

{

    int samplesperpixel = img->samplesperpixel;



    (void) y;

    fromskew *= samplesperpixel;

    while (h-- > 0) {

	uint32 r, g, b, a;

	for (x = w; x-- > 0;) {

	    a = pp[3];

	    r = (pp[0] * a) / 255;

	    g = (pp[1] * a) / 255;

	    b = (pp[2] * a) / 255;

	    *cp++ = PACK4(r,g,b,a);

	    pp += samplesperpixel;

	}

	cp += toskew;

	pp += fromskew;

    }

}



/*

 * 16-bit packed samples => RGB

 */

DECLAREContigPutFunc(putRGBcontig16bittile)

{

    int samplesperpixel = img->samplesperpixel;

    uint16 *wp = (uint16 *)pp;



    (void) y;

    fromskew *= samplesperpixel;

    while (h-- > 0) {

	for (x = w; x-- > 0;) {

	    *cp++ = PACKW(wp[0], wp[1], wp[2]);

	    wp += samplesperpixel;

	}

	cp += toskew;

	wp += fromskew;

    }

}



/*

 * 16-bit packed samples => RGBA w/ associated alpha

 * (known to have Map == NULL)

 */

DECLAREContigPutFunc(putRGBAAcontig16bittile)

{

    int samplesperpixel = img->samplesperpixel;

    uint16 *wp = (uint16 *)pp;



    (void) y;

    fromskew *= samplesperpixel;

    while (h-- > 0) {

	for (x = w; x-- > 0;) {

	    *cp++ = PACKW4(wp[0], wp[1], wp[2], wp[3]);

	    wp += samplesperpixel;

	}

	cp += toskew;

	wp += fromskew;

    }

}



/*

 * 16-bit packed samples => RGBA w/ unassociated alpha

 * (known to have Map == NULL)

 */

DECLAREContigPutFunc(putRGBUAcontig16bittile)

{

    int samplesperpixel = img->samplesperpixel;

    uint16 *wp = (uint16 *)pp;



    (void) y;

    fromskew *= samplesperpixel;

    while (h-- > 0) {

	uint32 r,g,b,a;

	/*

	 * We shift alpha down four bits just in case unsigned

	 * arithmetic doesn't handle the full range.

	 * We still have plenty of accuracy, since the output is 8 bits.

	 * So we have (r * 0xffff) * (a * 0xfff)) = r*a * (0xffff*0xfff)

	 * Since we want r*a * 0xff for eight bit output,

	 * we divide by (0xffff * 0xfff) / 0xff == 0x10eff.

	 */

	for (x = w; x-- > 0;) {

	    a = wp[3] >> 4; 

	    r = (wp[0] * a) / 0x10eff;

	    g = (wp[1] * a) / 0x10eff;

	    b = (wp[2] * a) / 0x10eff;

	    *cp++ = PACK4(r,g,b,a);

	    wp += samplesperpixel;

	}

	cp += toskew;

	wp += fromskew;

    }

}



/*

 * 8-bit packed CMYK samples w/o Map => RGB

 *

 * NB: The conversion of CMYK->RGB is *very* crude.

 */

DECLAREContigPutFunc(putRGBcontig8bitCMYKtile)

{

    int samplesperpixel = img->samplesperpixel;

    uint16 r, g, b, k;



    (void) x; (void) y;

    fromskew *= samplesperpixel;

    while (h-- > 0) {

	UNROLL8(w, NOP,

	    k = 255 - pp[3];

	    r = (k*(255-pp[0]))/255;

	    g = (k*(255-pp[1]))/255;

	    b = (k*(255-pp[2]))/255;

	    *cp++ = PACK(r, g, b);

	    pp += samplesperpixel);

	cp += toskew;

	pp += fromskew;

    }

}



/*

 * 8-bit packed CMYK samples w/Map => RGB

 *

 * NB: The conversion of CMYK->RGB is *very* crude.

 */

DECLAREContigPutFunc(putRGBcontig8bitCMYKMaptile)

{

    int samplesperpixel = img->samplesperpixel;

    TIFFRGBValue* Map = img->Map;

    uint16 r, g, b, k;



    (void) y;

    fromskew *= samplesperpixel;

    while (h-- > 0) {

	for (x = w; x-- > 0;) {

	    k = 255 - pp[3];

	    r = (k*(255-pp[0]))/255;

	    g = (k*(255-pp[1]))/255;

	    b = (k*(255-pp[2]))/255;

	    *cp++ = PACK(Map[r], Map[g], Map[b]);

	    pp += samplesperpixel;

	}

	pp += fromskew;

	cp += toskew;

    }

}



#define	DECLARESepPutFunc(name) \
static void name(\
    TIFFRGBAImage* img,\
    uint32* cp,\
    uint32 x, uint32 y, \
    uint32 w, uint32 h,\
    int32 fromskew, int32 toskew,\
    u_char* r, u_char* g, u_char* b, u_char* a\
)



/*

 * 8-bit unpacked samples => RGB

 */

DECLARESepPutFunc(putRGBseparate8bittile)

{

    (void) img; (void) x; (void) y; (void) a;

    while (h-- > 0) {

	UNROLL8(w, NOP, *cp++ = PACK(*r++, *g++, *b++));

	SKEW(r, g, b, fromskew);

	cp += toskew;

    }

}



/*

 * 8-bit unpacked samples => RGB

 */

DECLARESepPutFunc(putRGBseparate8bitMaptile)

{

    TIFFRGBValue* Map = img->Map;



    (void) y; (void) a;

    while (h-- > 0) {

	for (x = w; x > 0; x--)

	    *cp++ = PACK(Map[*r++], Map[*g++], Map[*b++]);

	SKEW(r, g, b, fromskew);

	cp += toskew;

    }

}



/*

 * 8-bit unpacked samples => RGBA w/ associated alpha

 */

DECLARESepPutFunc(putRGBAAseparate8bittile)

{

    (void) img; (void) x; (void) y;

    while (h-- > 0) {

	UNROLL8(w, NOP, *cp++ = PACK4(*r++, *g++, *b++, *a++));

	SKEW4(r, g, b, a, fromskew);

	cp += toskew;

    }

}



/*

 * 8-bit unpacked samples => RGBA w/ unassociated alpha

 */

DECLARESepPutFunc(putRGBUAseparate8bittile)

{

    (void) img; (void) y;

    while (h-- > 0) {

	uint32 rv, gv, bv, av;

	for (x = w; x-- > 0;) {

	    av = *a++;

	    rv = (*r++ * av) / 255;

	    gv = (*g++ * av) / 255;

	    bv = (*b++ * av) / 255;

	    *cp++ = PACK4(rv,gv,bv,av);

	}

	SKEW4(r, g, b, a, fromskew);

	cp += toskew;

    }

}



/*

 * 16-bit unpacked samples => RGB

 */

DECLARESepPutFunc(putRGBseparate16bittile)

{

    uint16 *wr = (uint16*) r;

    uint16 *wg = (uint16*) g;

    uint16 *wb = (uint16*) b;



    (void) img; (void) y; (void) a;

    while (h-- > 0) {

	for (x = 0; x < w; x++)

	    *cp++ = PACKW(*wr++, *wg++, *wb++);

	SKEW(wr, wg, wb, fromskew);

	cp += toskew;

    }

}



/*

 * 16-bit unpacked samples => RGBA w/ associated alpha

 */

DECLARESepPutFunc(putRGBAAseparate16bittile)

{

    uint16 *wr = (uint16*) r;

    uint16 *wg = (uint16*) g;

    uint16 *wb = (uint16*) b;

    uint16 *wa = (uint16*) a;



    (void) img; (void) y;

    while (h-- > 0) {

	for (x = 0; x < w; x++)

	    *cp++ = PACKW4(*wr++, *wg++, *wb++, *wa++);

	SKEW4(wr, wg, wb, wa, fromskew);

	cp += toskew;

    }

}



/*

 * 16-bit unpacked samples => RGBA w/ unassociated alpha

 */

DECLARESepPutFunc(putRGBUAseparate16bittile)

{

    uint16 *wr = (uint16*) r;

    uint16 *wg = (uint16*) g;

    uint16 *wb = (uint16*) b;

    uint16 *wa = (uint16*) a;



    (void) img; (void) y;

    while (h-- > 0) {

	uint32 r,g,b,a;

	/*

	 * We shift alpha down four bits just in case unsigned

	 * arithmetic doesn't handle the full range.

	 * We still have plenty of accuracy, since the output is 8 bits.

	 * So we have (r * 0xffff) * (a * 0xfff)) = r*a * (0xffff*0xfff)

	 * Since we want r*a * 0xff for eight bit output,

	 * we divide by (0xffff * 0xfff) / 0xff == 0x10eff.

	 */

	for (x = w; x-- > 0;) {

	    a = *wa++ >> 4; 

	    r = (*wr++ * a) / 0x10eff;

	    g = (*wg++ * a) / 0x10eff;

	    b = (*wb++ * a) / 0x10eff;

	    *cp++ = PACK4(r,g,b,a);

	}

	SKEW4(wr, wg, wb, wa, fromskew);

	cp += toskew;

    }

}



/*

 * YCbCr -> RGB conversion and packing routines.  The colorspace

 * conversion algorithm comes from the IJG v5a code; see below

 * for more information on how it works.

 */



#define	YCbCrtoRGB(dst, yc) {						\
    int Y = (yc);							\
    dst = PACK(								\
	clamptab[Y+Crrtab[Cr]],						\
	clamptab[Y + (int)((Cbgtab[Cb]+Crgtab[Cr])>>16)],		\
	clamptab[Y+Cbbtab[Cb]]);					\
}

#define	YCbCrSetup							\
    TIFFYCbCrToRGB* ycbcr = img->ycbcr;					\
    int* Crrtab = ycbcr->Cr_r_tab;					\
    int* Cbbtab = ycbcr->Cb_b_tab;					\
    int32* Crgtab = ycbcr->Cr_g_tab;					\
    int32* Cbgtab = ycbcr->Cb_g_tab;					\
    TIFFRGBValue* clamptab = ycbcr->clamptab


/*

 * 8-bit packed YCbCr samples w/ 4,4 subsampling => RGB

 */

DECLAREContigPutFunc(putcontig8bitYCbCr44tile)

{

    YCbCrSetup;

    uint32* cp1 = cp+w+toskew;

    uint32* cp2 = cp1+w+toskew;

    uint32* cp3 = cp2+w+toskew;

    u_int incr = 3*w+4*toskew;



    (void) y;

    /* XXX adjust fromskew */

    for (; h >= 4; h -= 4) {

	x = w>>2;

	do {

	    int Cb = pp[16];

	    int Cr = pp[17];



	    YCbCrtoRGB(cp [0], pp[ 0]);

	    YCbCrtoRGB(cp [1], pp[ 1]);

	    YCbCrtoRGB(cp [2], pp[ 2]);

	    YCbCrtoRGB(cp [3], pp[ 3]);

	    YCbCrtoRGB(cp1[0], pp[ 4]);

	    YCbCrtoRGB(cp1[1], pp[ 5]);

	    YCbCrtoRGB(cp1[2], pp[ 6]);

	    YCbCrtoRGB(cp1[3], pp[ 7]);

	    YCbCrtoRGB(cp2[0], pp[ 8]);

	    YCbCrtoRGB(cp2[1], pp[ 9]);

	    YCbCrtoRGB(cp2[2], pp[10]);

	    YCbCrtoRGB(cp2[3], pp[11]);

	    YCbCrtoRGB(cp3[0], pp[12]);

	    YCbCrtoRGB(cp3[1], pp[13]);

	    YCbCrtoRGB(cp3[2], pp[14]);

	    YCbCrtoRGB(cp3[3], pp[15]);



	    cp += 4, cp1 += 4, cp2 += 4, cp3 += 4;

	    pp += 18;

	} while (--x);

	cp += incr, cp1 += incr, cp2 += incr, cp3 += incr;

	pp += fromskew;

    }

}



/*

 * 8-bit packed YCbCr samples w/ 4,2 subsampling => RGB

 */

DECLAREContigPutFunc(putcontig8bitYCbCr42tile)

{

    YCbCrSetup;

    uint32* cp1 = cp+w+toskew;

    u_int incr = 2*toskew+w;



    (void) y;

    /* XXX adjust fromskew */

    for (; h >= 2; h -= 2) {

	x = w>>2;

	do {

	    int Cb = pp[8];

	    int Cr = pp[9];



	    YCbCrtoRGB(cp [0], pp[0]);

	    YCbCrtoRGB(cp [1], pp[1]);

	    YCbCrtoRGB(cp [2], pp[2]);

	    YCbCrtoRGB(cp [3], pp[3]);

	    YCbCrtoRGB(cp1[0], pp[4]);

	    YCbCrtoRGB(cp1[1], pp[5]);

	    YCbCrtoRGB(cp1[2], pp[6]);

	    YCbCrtoRGB(cp1[3], pp[7]);



	    cp += 4, cp1 += 4;

	    pp += 10;

	} while (--x);

	cp += incr, cp1 += incr;

	pp += fromskew;

    }

}



/*

 * 8-bit packed YCbCr samples w/ 4,1 subsampling => RGB

 */

DECLAREContigPutFunc(putcontig8bitYCbCr41tile)

{

    YCbCrSetup;



    (void) y;

    /* XXX adjust fromskew */

    do {

	x = w>>2;

	do {

	    int Cb = pp[4];

	    int Cr = pp[5];



	    YCbCrtoRGB(cp [0], pp[0]);

	    YCbCrtoRGB(cp [1], pp[1]);

	    YCbCrtoRGB(cp [2], pp[2]);

	    YCbCrtoRGB(cp [3], pp[3]);



	    cp += 4;

	    pp += 6;

	} while (--x);

	cp += toskew;

	pp += fromskew;

    } while (--h);

}



/*

 * 8-bit packed YCbCr samples w/ 2,2 subsampling => RGB

 */

DECLAREContigPutFunc(putcontig8bitYCbCr22tile)

{

    YCbCrSetup;

    uint32* cp1 = cp+w+toskew;

    u_int incr = 2*toskew+w;



    (void) y;

    /* XXX adjust fromskew */

    for (; h >= 2; h -= 2) {

	x = w>>1;

	do {

	    int Cb = pp[4];

	    int Cr = pp[5];



	    YCbCrtoRGB(cp [0], pp[0]);

	    YCbCrtoRGB(cp [1], pp[1]);

	    YCbCrtoRGB(cp1[0], pp[2]);

	    YCbCrtoRGB(cp1[1], pp[3]);



	    cp += 2, cp1 += 2;

	    pp += 6;

	} while (--x);

	cp += incr, cp1 += incr;

	pp += fromskew;

    }

}



/*

 * 8-bit packed YCbCr samples w/ 2,1 subsampling => RGB

 */

DECLAREContigPutFunc(putcontig8bitYCbCr21tile)

{

    YCbCrSetup;



    (void) y;

    /* XXX adjust fromskew */

    do {

	x = w>>1;

	do {

	    int Cb = pp[2];

	    int Cr = pp[3];



	    YCbCrtoRGB(cp[0], pp[0]);

	    YCbCrtoRGB(cp[1], pp[1]);



	    cp += 2;

	    pp += 4;

	} while (--x);

	cp += toskew;

	pp += fromskew;

    } while (--h);

}



/*

 * 8-bit packed YCbCr samples w/ no subsampling => RGB

 */

DECLAREContigPutFunc(putcontig8bitYCbCr11tile)

{

    YCbCrSetup;



    (void) y;

    /* XXX adjust fromskew */

    do {

	x = w>>1;

	do {

	    int Cb = pp[1];

	    int Cr = pp[2];



	    YCbCrtoRGB(*cp++, pp[0]);



	    pp += 3;

	} while (--x);

	cp += toskew;

	pp += fromskew;

    } while (--h);

}

#undef	YCbCrSetup

#undef	YCbCrtoRGB



#define	LumaRed			coeffs[0]

#define	LumaGreen		coeffs[1]

#define	LumaBlue		coeffs[2]

#define	SHIFT			16

#define	FIX(x)			((int32)((x) * (1L<<SHIFT) + 0.5))

#define	ONE_HALF		((int32)(1<<(SHIFT-1)))



/*

 * Initialize the YCbCr->RGB conversion tables.  The conversion

 * is done according to the 6.0 spec:

 *

 *    R = Y + Cr*(2 - 2*LumaRed)

 *    B = Y + Cb*(2 - 2*LumaBlue)

 *    G =   Y

 *        - LumaBlue*Cb*(2-2*LumaBlue)/LumaGreen

 *        - LumaRed*Cr*(2-2*LumaRed)/LumaGreen

 *

 * To avoid floating point arithmetic the fractional constants that

 * come out of the equations are represented as fixed point values

 * in the range 0...2^16.  We also eliminate multiplications by

 * pre-calculating possible values indexed by Cb and Cr (this code

 * assumes conversion is being done for 8-bit samples).

 */

static void

TIFFYCbCrToRGBInit(TIFFYCbCrToRGB* ycbcr, TIFF* tif)

{

    TIFFRGBValue* clamptab;

    float* coeffs;

    int i;



    clamptab = (TIFFRGBValue*)(

	(tidata_t) ycbcr+TIFFroundup(sizeof (TIFFYCbCrToRGB), sizeof (long)));

    _TIFFmemset(clamptab, 0, 256);		/* v < 0 => 0 */

    ycbcr->clamptab = (clamptab += 256);

    for (i = 0; i < 256; i++)

	clamptab[i] = i;

    _TIFFmemset(clamptab+256, 255, 2*256);	/* v > 255 => 255 */

    TIFFGetFieldDefaulted(tif, TIFFTAG_YCBCRCOEFFICIENTS, &coeffs);

    _TIFFmemcpy(ycbcr->coeffs, coeffs, 3*sizeof (float));

    { float f1 = 2-2*LumaRed;		int32 D1 = FIX(f1);

      float f2 = LumaRed*f1/LumaGreen;	int32 D2 = -FIX(f2);

      float f3 = 2-2*LumaBlue;		int32 D3 = FIX(f3);

      float f4 = LumaBlue*f3/LumaGreen;	int32 D4 = -FIX(f4);

      int x;



      ycbcr->Cr_r_tab = (int*) (clamptab + 3*256);

      ycbcr->Cb_b_tab = ycbcr->Cr_r_tab + 256;

      ycbcr->Cr_g_tab = (int32*) (ycbcr->Cb_b_tab + 256);

      ycbcr->Cb_g_tab = ycbcr->Cr_g_tab + 256;

      /*

       * i is the actual input pixel value in the range 0..255

       * Cb and Cr values are in the range -128..127 (actually

       * they are in a range defined by the ReferenceBlackWhite

       * tag) so there is some range shifting to do here when

       * constructing tables indexed by the raw pixel data.

       *

       * XXX handle ReferenceBlackWhite correctly to calculate

       *     Cb/Cr values to use in constructing the tables.

       */

      for (i = 0, x = -128; i < 256; i++, x++) {

	  ycbcr->Cr_r_tab[i] = (int)((D1*x + ONE_HALF)>>SHIFT);

	  ycbcr->Cb_b_tab[i] = (int)((D3*x + ONE_HALF)>>SHIFT);

	  ycbcr->Cr_g_tab[i] = D2*x;

	  ycbcr->Cb_g_tab[i] = D4*x + ONE_HALF;

      }

    }

}

#undef	SHIFT

#undef	ONE_HALF

#undef	FIX

#undef	LumaBlue

#undef	LumaGreen

#undef	LumaRed



static tileContigRoutine

initYCbCrConversion(TIFFRGBAImage* img)

{

    uint16 hs, vs;



    if (img->ycbcr == NULL) {

	img->ycbcr = (TIFFYCbCrToRGB*) _TIFFmalloc(

	      TIFFroundup(sizeof (TIFFYCbCrToRGB), sizeof (long))

	    + 4*256*sizeof (TIFFRGBValue)

	    + 2*256*sizeof (int)

	    + 2*256*sizeof (int32)

	);

	if (img->ycbcr == NULL) {

	    TIFFError(TIFFFileName(img->tif),

		"No space for YCbCr->RGB conversion state");

	    return (NULL);

	}

	TIFFYCbCrToRGBInit(img->ycbcr, img->tif);

    } else {

	float* coeffs;



	TIFFGetFieldDefaulted(img->tif, TIFFTAG_YCBCRCOEFFICIENTS, &coeffs);

	if (_TIFFmemcmp(coeffs, img->ycbcr->coeffs, 3*sizeof (float)) != 0)

	    TIFFYCbCrToRGBInit(img->ycbcr, img->tif);

    }

    /*

     * The 6.0 spec says that subsampling must be

     * one of 1, 2, or 4, and that vertical subsampling

     * must always be <= horizontal subsampling; so

     * there are only a few possibilities and we just

     * enumerate the cases.

     */

    TIFFGetFieldDefaulted(img->tif, TIFFTAG_YCBCRSUBSAMPLING, &hs, &vs);

    switch ((hs<<4)|vs) {

    case 0x44: return (putcontig8bitYCbCr44tile);

    case 0x42: return (putcontig8bitYCbCr42tile);

    case 0x41: return (putcontig8bitYCbCr41tile);

    case 0x22: return (putcontig8bitYCbCr22tile);

    case 0x21: return (putcontig8bitYCbCr21tile);

    case 0x11: return (putcontig8bitYCbCr11tile);

    }

    return (NULL);

}



/*

 * Greyscale images with less than 8 bits/sample are handled

 * with a table to avoid lots of shifts and masks.  The table

 * is setup so that put*bwtile (below) can retrieve 8/bitspersample

 * pixel values simply by indexing into the table with one

 * number.

 */

static int

makebwmap(TIFFRGBAImage* img)

{

    TIFFRGBValue* Map = img->Map;

    int bitspersample = img->bitspersample;

    int nsamples = 8 / bitspersample;

    int i;

    uint32* p;



    img->BWmap = (uint32**) _TIFFmalloc(

	256*sizeof (uint32 *)+(256*nsamples*sizeof(uint32)));

    if (img->BWmap == NULL) {

	TIFFError(TIFFFileName(img->tif), "No space for B&W mapping table");

	return (0);

    }

    p = (uint32*)(img->BWmap + 256);

    for (i = 0; i < 256; i++) {

	TIFFRGBValue c;

	img->BWmap[i] = p;

	switch (bitspersample) {

#define	GREY(x)	c = Map[x]; *p++ = PACK(c,c,c);

	case 1:

	    GREY(i>>7);

	    GREY((i>>6)&1);

	    GREY((i>>5)&1);

	    GREY((i>>4)&1);

	    GREY((i>>3)&1);

	    GREY((i>>2)&1);

	    GREY((i>>1)&1);

	    GREY(i&1);

	    break;

	case 2:

	    GREY(i>>6);

	    GREY((i>>4)&3);

	    GREY((i>>2)&3);

	    GREY(i&3);

	    break;

	case 4:

	    GREY(i>>4);

	    GREY(i&0xf);

	    break;

	case 8:

	    GREY(i);

	    break;

	}

#undef	GREY

    }

    return (1);

}



/*

 * Construct a mapping table to convert from the range

 * of the data samples to [0,255] --for display.  This

 * process also handles inverting B&W images when needed.

 */ 

static int

setupMap(TIFFRGBAImage* img)

{

    int32 x, range;



    range = (int32)((1L<<img->bitspersample)-1);

    img->Map = (TIFFRGBValue*) _TIFFmalloc((range+1) * sizeof (TIFFRGBValue));

    if (img->Map == NULL) {

	TIFFError(TIFFFileName(img->tif),

	    "No space for photometric conversion table");

	return (0);

    }

    if (img->photometric == PHOTOMETRIC_MINISWHITE) {

	for (x = 0; x <= range; x++)

	    img->Map[x] = ((range - x) * 255) / range;

    } else {

	for (x = 0; x <= range; x++)

	    img->Map[x] = (x * 255) / range;

    }

    if (img->bitspersample <= 8 &&

	(img->photometric == PHOTOMETRIC_MINISBLACK ||

	 img->photometric == PHOTOMETRIC_MINISWHITE)) {

	/*

	 * Use photometric mapping table to construct

	 * unpacking tables for samples <= 8 bits.

	 */

	if (!makebwmap(img))

	    return (0);

	/* no longer need Map, free it */

	_TIFFfree(img->Map), img->Map = NULL;

    }

    return (1);

}



static int

checkcmap(TIFFRGBAImage* img)

{

    uint16* r = img->redcmap;

    uint16* g = img->greencmap;

    uint16* b = img->bluecmap;

    long n = 1L<<img->bitspersample;



    while (n-- > 0)

	if (*r++ >= 256 || *g++ >= 256 || *b++ >= 256)

	    return (16);

    return (8);

}



static void

cvtcmap(TIFFRGBAImage* img)

{

    uint16* r = img->redcmap;

    uint16* g = img->greencmap;

    uint16* b = img->bluecmap;

    long i;



    for (i = (1L<<img->bitspersample)-1; i >= 0; i--) {

#define	CVT(x)		((uint16)(((x) * 255) / ((1L<<16)-1)))

	r[i] = CVT(r[i]);

	g[i] = CVT(g[i]);

	b[i] = CVT(b[i]);

#undef	CVT

    }

}



/*

 * Palette images with <= 8 bits/sample are handled

 * with a table to avoid lots of shifts and masks.  The table

 * is setup so that put*cmaptile (below) can retrieve 8/bitspersample

 * pixel values simply by indexing into the table with one

 * number.

 */

static int

makecmap(TIFFRGBAImage* img)

{

    int bitspersample = img->bitspersample;

    int nsamples = 8 / bitspersample;

    uint16* r = img->redcmap;

    uint16* g = img->greencmap;

    uint16* b = img->bluecmap;

    uint32 *p;

    int i;



    img->PALmap = (uint32**) _TIFFmalloc(

	256*sizeof (uint32 *)+(256*nsamples*sizeof(uint32)));

    if (img->PALmap == NULL) {

	TIFFError(TIFFFileName(img->tif), "No space for Palette mapping table");

	return (0);

    }

    p = (uint32*)(img->PALmap + 256);

    for (i = 0; i < 256; i++) {

	TIFFRGBValue c;

	img->PALmap[i] = p;

#define	CMAP(x)	c = x; *p++ = PACK(r[c]&0xff, g[c]&0xff, b[c]&0xff);

	switch (bitspersample) {

	case 1:

	    CMAP(i>>7);

	    CMAP((i>>6)&1);

	    CMAP((i>>5)&1);

	    CMAP((i>>4)&1);

	    CMAP((i>>3)&1);

	    CMAP((i>>2)&1);

	    CMAP((i>>1)&1);

	    CMAP(i&1);

	    break;

	case 2:

	    CMAP(i>>6);

	    CMAP((i>>4)&3);

	    CMAP((i>>2)&3);

	    CMAP(i&3);

	    break;

	case 4:

	    CMAP(i>>4);

	    CMAP(i&0xf);

	    break;

	case 8:

	    CMAP(i);

	    break;

	}

#undef CMAP

    }

    return (1);

}



/* 

 * Construct any mapping table used

 * by the associated put routine.

 */

static int

buildMap(TIFFRGBAImage* img)

{

    switch (img->photometric) {

    case PHOTOMETRIC_RGB:

    case PHOTOMETRIC_YCBCR:

    case PHOTOMETRIC_SEPARATED:

	if (img->bitspersample == 8)

	    break;

	/* fall thru... */

    case PHOTOMETRIC_MINISBLACK:

    case PHOTOMETRIC_MINISWHITE:

	if (!setupMap(img))

	    return (0);

	break;

    case PHOTOMETRIC_PALETTE:

	/*

	 * Convert 16-bit colormap to 8-bit (unless it looks

	 * like an old-style 8-bit colormap).

	 */

	if (checkcmap(img) == 16)

	    cvtcmap(img);

	else

	    TIFFWarning(TIFFFileName(img->tif), "Assuming 8-bit colormap");

	/*

	 * Use mapping table and colormap to construct

	 * unpacking tables for samples < 8 bits.

	 */

	if (img->bitspersample <= 8 && !makecmap(img))

	    return (0);

	break;

    }

    return (1);

}



/*

 * Select the appropriate conversion routine for packed data.

 */

static int

pickTileContigCase(TIFFRGBAImage* img)

{

    tileContigRoutine put = 0;



    if (buildMap(img)) {

	switch (img->photometric) {

	case PHOTOMETRIC_RGB:

	    switch (img->bitspersample) {

	    case 8:

		if (!img->Map) {

		    if (img->alpha == EXTRASAMPLE_ASSOCALPHA)

			put = putRGBAAcontig8bittile;

		    else if (img->alpha == EXTRASAMPLE_UNASSALPHA)

			put = putRGBUAcontig8bittile;

		    else

			put = putRGBcontig8bittile;

		} else

		    put = putRGBcontig8bitMaptile;

		break;

	    case 16:

		put = putRGBcontig16bittile;

		if (!img->Map) {

		    if (img->alpha == EXTRASAMPLE_ASSOCALPHA)

			put = putRGBAAcontig16bittile;

		    else if (img->alpha == EXTRASAMPLE_UNASSALPHA)

			put = putRGBUAcontig16bittile;

		}

		break;

	    }

	    break;

	case PHOTOMETRIC_SEPARATED:

	    if (img->bitspersample == 8) {

		if (!img->Map)

		    put = putRGBcontig8bitCMYKtile;

		else

		    put = putRGBcontig8bitCMYKMaptile;

	    }

	    break;

	case PHOTOMETRIC_PALETTE:

	    switch (img->bitspersample) {

	    case 8:	put = put8bitcmaptile; break;

	    case 4: put = put4bitcmaptile; break;

	    case 2: put = put2bitcmaptile; break;

	    case 1: put = put1bitcmaptile; break;

	    }

	    break;

	case PHOTOMETRIC_MINISWHITE:

	case PHOTOMETRIC_MINISBLACK:

	    switch (img->bitspersample) {

	    case 8:	put = putgreytile; break;

	    case 4: put = put4bitbwtile; break;

	    case 2: put = put2bitbwtile; break;

	    case 1: put = put1bitbwtile; break;

	    }

	    break;

	case PHOTOMETRIC_YCBCR:

	    if (img->bitspersample == 8)

		put = initYCbCrConversion(img);

	    break;

	}

    }

    return ((img->put.contig = put) != 0);

}



/*

 * Select the appropriate conversion routine for unpacked data.

 *

 * NB: we assume that unpacked single channel data is directed

 *	 to the "packed routines.

 */

static int

pickTileSeparateCase(TIFFRGBAImage* img)

{

    tileSeparateRoutine put = 0;



    if (buildMap(img)) {

	switch (img->photometric) {

	case PHOTOMETRIC_RGB:

	    switch (img->bitspersample) {

	    case 8:

		if (!img->Map) {

		    if (img->alpha == EXTRASAMPLE_ASSOCALPHA)

			put = putRGBAAseparate8bittile;

		    else if (img->alpha == EXTRASAMPLE_UNASSALPHA)

			put = putRGBUAseparate8bittile;

		    else

			put = putRGBseparate8bittile;

		} else

		    put = putRGBseparate8bitMaptile;

		break;

	    case 16:

		put = putRGBseparate16bittile;

		if (!img->Map) {

		    if (img->alpha == EXTRASAMPLE_ASSOCALPHA)

			put = putRGBAAseparate16bittile;

		    else if (img->alpha == EXTRASAMPLE_UNASSALPHA)

			put = putRGBUAseparate16bittile;

		}

		break;

	    }

	    break;

	}

    }

    return ((img->put.separate = put) != 0);

}

