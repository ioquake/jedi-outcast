// lbmlib.c

#include "stdafx.h"
#include "cmdlib.h"
#include "lbmlib.h"
#include "bmp.h"
#include "jpgfile.h"
#include "../libs/pakstuff.h"


/*
============================================================================

						LBM STUFF

============================================================================
*/


typedef unsigned char	UBYTE;
//conflicts with windows typedef short			WORD;
typedef unsigned short	UWORD;
typedef long			LONG;

typedef enum
{
	ms_none,
	ms_mask,
	ms_transcolor,
	ms_lasso
} mask_t;

typedef enum
{
	cm_none,
	cm_rle1
} compress_t;

typedef struct
{
	UWORD		w,h;
	short		x,y;
	UBYTE		nPlanes;
	UBYTE		masking;
	UBYTE		compression;
	UBYTE		pad1;
	UWORD		transparentColor;
	UBYTE		xAspect,yAspect;
	short		pageWidth,pageHeight;
} bmhd_t;

extern	bmhd_t	bmhd;						// will be in native byte order



#define FORMID ('F'+('O'<<8)+((int)'R'<<16)+((int)'M'<<24))
#define ILBMID ('I'+('L'<<8)+((int)'B'<<16)+((int)'M'<<24))
#define PBMID  ('P'+('B'<<8)+((int)'M'<<16)+((int)' '<<24))
#define BMHDID ('B'+('M'<<8)+((int)'H'<<16)+((int)'D'<<24))
#define BODYID ('B'+('O'<<8)+((int)'D'<<16)+((int)'Y'<<24))
#define CMAPID ('C'+('M'<<8)+((int)'A'<<16)+((int)'P'<<24))


bmhd_t  bmhd;

int    Align (int l)
{
	if (l&1)
		return l+1;
	return l;
}



/*
================
LBMRLEdecompress

Source must be evenly aligned!
================
*/
byte  *LBMRLEDecompress (byte *source,byte *unpacked, int bpwidth)
{
	int     count;
	byte    b,rept;

	count = 0;

	do
	{
		rept = *source++;

		if (rept > 0x80)
		{
			rept = (rept^0xff)+2;
			b = *source++;
			memset(unpacked,b,rept);
			unpacked += rept;
		}
		else if (rept < 0x80)
		{
			rept++;
			memcpy(unpacked,source,rept);
			unpacked += rept;
			source += rept;
		}
		else
			rept = 0;               // rept of 0x80 is NOP

		count += rept;

	} while (count<bpwidth);

	if (count>bpwidth)
		Error ("Decompression exceeded width!\n");


	return source;
}


/*
=================
LoadLBM
=================
*/
void LoadLBM (char *filename, byte **picture, byte **palette)
{
	byte    *LBMbuffer, *picbuffer, *cmapbuffer;
	int             y;
	byte    *LBM_P, *LBMEND_P;
	byte    *pic_p;
	byte    *body_p;

	int    formtype,formlength;
	int    chunktype,chunklength;

// qiet compiler warnings
	picbuffer = NULL;
	cmapbuffer = NULL;

//
// load the LBM
//
	LoadFile (filename, (void **)&LBMbuffer);

//
// parse the LBM header
//
	LBM_P = LBMbuffer;
	if ( *(int *)LBMbuffer != LittleLong(FORMID) )
	   Error ("No FORM ID at start of file!\n");

	LBM_P += 4;
	formlength = BigLong( *(int *)LBM_P );
	LBM_P += 4;
	LBMEND_P = LBM_P + Align(formlength);

	formtype = LittleLong(*(int *)LBM_P);

	if (formtype != ILBMID && formtype != PBMID)
		Error ("Unrecognized form type: %c%c%c%c\n", formtype&0xff
		,(formtype>>8)&0xff,(formtype>>16)&0xff,(formtype>>24)&0xff);

	LBM_P += 4;

//
// parse chunks
//

	while (LBM_P < LBMEND_P)
	{
		chunktype = LBM_P[0] + (LBM_P[1]<<8) + (LBM_P[2]<<16) + (LBM_P[3]<<24);
		LBM_P += 4;
		chunklength = LBM_P[3] + (LBM_P[2]<<8) + (LBM_P[1]<<16) + (LBM_P[0]<<24);
		LBM_P += 4;

		switch ( chunktype )
		{
		case BMHDID:
			memcpy (&bmhd,LBM_P,sizeof(bmhd));
			bmhd.w = BigShort(bmhd.w);
			bmhd.h = BigShort(bmhd.h);
			bmhd.x = BigShort(bmhd.x);
			bmhd.y = BigShort(bmhd.y);
			bmhd.pageWidth = BigShort(bmhd.pageWidth);
			bmhd.pageHeight = BigShort(bmhd.pageHeight);
			break;

		case CMAPID:
			cmapbuffer = (unsigned char*)malloc (768);
			memset (cmapbuffer, 0, 768);
			memcpy (cmapbuffer, LBM_P, chunklength);
			break;

		case BODYID:
			body_p = LBM_P;

			pic_p = picbuffer = (unsigned char*)malloc (bmhd.w*bmhd.h);
			if (formtype == PBMID)
			{
			//
			// unpack PBM
			//
				for (y=0 ; y<bmhd.h ; y++, pic_p += bmhd.w)
				{
					if (bmhd.compression == cm_rle1)
						body_p = LBMRLEDecompress ((byte *)body_p
						, pic_p , bmhd.w);
					else if (bmhd.compression == cm_none)
					{
						memcpy (pic_p,body_p,bmhd.w);
						body_p += Align(bmhd.w);
					}
				}

			}
			else
			{
			//
			// unpack ILBM
			//
				Error ("%s is an interlaced LBM, not packed", filename);
			}
			break;
		}

		LBM_P += Align(chunklength);
	}

	free (LBMbuffer);

	*picture = picbuffer;

	if (palette)
		*palette = cmapbuffer;
}


/*
============================================================================

							WRITE LBM

============================================================================
*/

/*
==============
WriteLBMfile
==============
*/
void WriteLBMfile (char *filename, byte *data,
				   int width, int height, byte *palette)
{
	byte    *lbm, *lbmptr;
	int    *formlength, *bmhdlength, *cmaplength, *bodylength;
	int    length;
	bmhd_t  basebmhd;

	lbm = lbmptr = (unsigned char*)malloc (width*height+1000);

//
// start FORM
//
	*lbmptr++ = 'F';
	*lbmptr++ = 'O';
	*lbmptr++ = 'R';
	*lbmptr++ = 'M';

	formlength = (int*)lbmptr;
	lbmptr+=4;                      // leave space for length

	*lbmptr++ = 'P';
	*lbmptr++ = 'B';
	*lbmptr++ = 'M';
	*lbmptr++ = ' ';

//
// write BMHD
//
	*lbmptr++ = 'B';
	*lbmptr++ = 'M';
	*lbmptr++ = 'H';
	*lbmptr++ = 'D';

	bmhdlength = (int *)lbmptr;
	lbmptr+=4;                      // leave space for length

	memset (&basebmhd,0,sizeof(basebmhd));
	basebmhd.w = BigShort((short)width);
	basebmhd.h = BigShort((short)height);
	basebmhd.nPlanes = 8;
	basebmhd.xAspect = 5;
	basebmhd.yAspect = 6;
	basebmhd.pageWidth = BigShort((short)width);
	basebmhd.pageHeight = BigShort((short)height);

	memcpy (lbmptr,&basebmhd,sizeof(basebmhd));
	lbmptr += sizeof(basebmhd);

	length = lbmptr-(byte *)bmhdlength-4;
	*bmhdlength = BigLong(length);
	if (length&1)
		*lbmptr++ = 0;          // pad chunk to even offset

//
// write CMAP
//
	*lbmptr++ = 'C';
	*lbmptr++ = 'M';
	*lbmptr++ = 'A';
	*lbmptr++ = 'P';

	cmaplength = (int *)lbmptr;
	lbmptr+=4;                      // leave space for length

	memcpy (lbmptr,palette,768);
	lbmptr += 768;

	length = lbmptr-(byte *)cmaplength-4;
	*cmaplength = BigLong(length);
	if (length&1)
		*lbmptr++ = 0;          // pad chunk to even offset

//
// write BODY
//
	*lbmptr++ = 'B';
	*lbmptr++ = 'O';
	*lbmptr++ = 'D';
	*lbmptr++ = 'Y';

	bodylength = (int *)lbmptr;
	lbmptr+=4;                      // leave space for length

	memcpy (lbmptr,data,width*height);
	lbmptr += width*height;

	length = lbmptr-(byte *)bodylength-4;
	*bodylength = BigLong(length);
	if (length&1)
		*lbmptr++ = 0;          // pad chunk to even offset

//
// done
//
	length = lbmptr-(byte *)formlength-4;
	*formlength = BigLong(length);
	if (length&1)
		*lbmptr++ = 0;          // pad chunk to even offset

//
// write output file
//
	SaveFile (filename, lbm, lbmptr-lbm);
	free (lbm);
}


/*
============================================================================

LOAD PCX

============================================================================
*/

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned char	data;			// unbounded
} pcx_t;


/*
==============
LoadPCX
==============
*/
void LoadPCX (char *filename, byte **pic, byte **palette, int *width, int *height)
{
	byte	*raw = 0;
	pcx_t	*pcx;
	int		x, y;
	int		len;
	int		dataByte, runLength;
	byte	*out, *pix;

	if (pic)
		*pic = NULL;
	if (palette)
		*palette = NULL;
	if (width)
		*width = 0;
	if (height)
		*height = 0;

	//
	// load the file
	//
	len = LoadFile (filename, (void **)&raw);
	if (len == -1)
		return;

	//
	// parse the PCX file
	//
	pcx = (pcx_t *)raw;
	raw = &pcx->data;

	pcx->xmin = LittleShort(pcx->xmin);
	pcx->ymin = LittleShort(pcx->ymin);
	pcx->xmax = LittleShort(pcx->xmax);
	pcx->ymax = LittleShort(pcx->ymax);
	pcx->hres = LittleShort(pcx->hres);
	pcx->vres = LittleShort(pcx->vres);
	pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
	pcx->palette_type = LittleShort(pcx->palette_type);

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 640
		|| pcx->ymax >= 480)
		Error ("Bad pcx file %s", filename);
	
	if (palette)
	{
		*palette = (unsigned char*)malloc(768);
		memcpy (*palette, (byte *)pcx + len - 768, 768);
	}

	if (width)
		*width = pcx->xmax+1;
	if (height)
		*height = pcx->ymax+1;

	if (!pic)
  {
    free(pcx);
		return;
  }

	out = (unsigned char*)malloc ( (pcx->ymax+1) * (pcx->xmax+1) );
	if (!out)
		Error ("Skin_Cache: couldn't allocate");

	*pic = out;

	pix = out;

	for (y=0 ; y<=pcx->ymax ; y++, pix += pcx->xmax+1)
	{
		for (x=0 ; x<=pcx->xmax ; )
		{
			dataByte = *raw++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
				runLength = 1;

			while(runLength-- > 0)
				pix[x++] = dataByte;
		}

	}

	if ( raw - (byte *)pcx > len)
		Error ("PCX file %s was malformed", filename);

	free (pcx);
}

/* 
============== 
WritePCXfile 
============== 
*/ 
void WritePCXfile (char *filename, byte *data, 
				   int width, int height, byte *palette) 
{
	int		i, j, length;
	pcx_t	*pcx;
	byte		*pack;
	  
	pcx = (pcx_t*)malloc (width*height*2+1000);
	memset (pcx, 0, sizeof(*pcx));

	pcx->manufacturer = 0x0a;	// PCX id
	pcx->version = 5;			// 256 color
 	pcx->encoding = 1;		// uncompressed
	pcx->bits_per_pixel = 8;		// 256 color
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = LittleShort((short)(width-1));
	pcx->ymax = LittleShort((short)(height-1));
	pcx->hres = LittleShort((short)width);
	pcx->vres = LittleShort((short)height);
	pcx->color_planes = 1;		// chunky image
	pcx->bytes_per_line = LittleShort((short)width);
	pcx->palette_type = LittleShort(2);		// not a grey scale

	// pack the image
	pack = &pcx->data;
	
	for (i=0 ; i<height ; i++)
	{
		for (j=0 ; j<width ; j++)
		{
			if ( (*data & 0xc0) != 0xc0)
				*pack++ = *data++;
			else
			{
				*pack++ = 0xc1;
				*pack++ = *data++;
			}
		}
	}
			
	// write the palette
	*pack++ = 0x0c;	// palette ID byte
	for (i=0 ; i<768 ; i++)
		*pack++ = *palette++;
		
// write output file 
	length = pack - (byte *)pcx;
	SaveFile (filename, pcx, length);

	free (pcx);
} 
 

/*
============================================================================

LOAD IMAGE

============================================================================
*/

/*
==============
Load256Image

Will load either an lbm or pcx, depending on extension.
Any of the return pointers can be NULL if you don't want them.
==============
*/
void Load256Image (char *name, byte **pixels, byte **palette,
				   int *width, int *height)
{
	char	ext[128];
	bitmap_t	bmp;

	ExtractFileExtension (name, ext);
	if (!Q_strcasecmp (ext, "lbm"))
	{
		LoadLBM (name, pixels, palette);
		if (width)
			*width = bmhd.w;
		if (height)
			*height = bmhd.h;
	}
	else if (!Q_strcasecmp (ext, "pcx"))
	{
		LoadPCX (name, pixels, palette, width, height);
	}
	else if (!Q_strcasecmp (ext, "bmp"))
	{
		LoadBMP (name, &bmp);
		if (bmp.palette)
		{
			*palette = (unsigned char*)malloc (768);
			memcpy (*palette, bmp.palette, 768);
		}
		FreeBMP (&bmp);		
	}
	else
		Error ("%s doesn't have a known image extension", name);
}


/*
==============
Save256Image

Will save either an lbm or pcx, depending on extension.
==============
*/
void Save256Image (char *name, byte *pixels, byte *palette,
				   int width, int height)
{
	char	ext[128];

	ExtractFileExtension (name, ext);
	if (!Q_strcasecmp (ext, "lbm"))
	{
		WriteLBMfile (name, pixels, width, height, palette);
	}
	else if (!Q_strcasecmp (ext, "pcx"))
	{
		WritePCXfile (name, pixels, width, height, palette);
	}
	else
		Error ("%s doesn't have a known image extension", name);
}




/*
============================================================================

TARGA IMAGE

============================================================================
*/

typedef struct _TargaHeader {
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

int fgetLittleShort (FILE *f)
{
	byte	b1, b2;

	b1 = fgetc(f);
	b2 = fgetc(f);

	return (short)(b1 + b2*256);
}

int getLittleShort (byte*& p)
{
	byte	b1, b2;
	b1 = *p++;
	b2 = *p++;
	return (short)(b1 + b2*256);
}

int getLittleLong (byte*& p)
{
	byte	b1, b2, b3, b4;

	b1 = *p++;
	b2 = *p++;
	b3 = *p++;
	b4 = *p++;
	return b1 + (b2<<8) + (b3<<16) + (b4<<24);
}

char getc(byte*& p)
{
  return *p++;
}


// return code is if *pixels is NZ on exit
//
void LoadTGA_Actual (char *name, byte **pixels, int *width, int *height)
{
	int				columns, rows, numPixels;
	byte			*pixbuf;
	int				row, column;
	byte			*targa_rgba;
	TargaHeader		targa_header;
  byte      *raw;

	int len = LoadFile (name, (void **)&raw);
	if (len == -1)
	{	
		len = PakLoadAnyFile (name, (void **)&raw);
		if (len == -1)
		{
			return;
		}
	}

  byte* pBuff = raw;

	targa_header.id_length = getc(pBuff);
	targa_header.colormap_type = getc(pBuff);
	targa_header.image_type = getc(pBuff);
	
	targa_header.colormap_index = getLittleShort(pBuff);
	targa_header.colormap_length = getLittleShort(pBuff);
	targa_header.colormap_size = getc(pBuff);
	targa_header.x_origin = getLittleShort(pBuff);
	targa_header.y_origin = getLittleShort(pBuff);
	targa_header.width = getLittleShort(pBuff);
	targa_header.height = getLittleShort(pBuff);
	targa_header.pixel_size = getc(pBuff);
	targa_header.attributes = getc(pBuff);

	if (targa_header.image_type != 2 && targa_header.image_type != 10) 
  {
		Sys_Printf("LoadTGA: Only type 2 and 10 targa RGB images supported\n");
    free(raw);
    return;
  }

	if (targa_header.colormap_type !=0 || (targa_header.pixel_size!=32 && targa_header.pixel_size!=24))
  {
		Sys_Printf("Texture_LoadTGA: Only 32 or 24 bit images supported (no colormaps)\n");
    free(raw);
  }

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if (width)
		*width = columns;
	if (height)
		*height = rows;
	targa_rgba = (unsigned char*)malloc(numPixels*4);

	*pixels = targa_rgba;	// at this point the return code is effectively true

	if (targa_header.id_length != 0)
		pBuff += targa_header.id_length;  // skip TARGA image comment
	
	if (targa_header.image_type==2) {  // Uncompressed, RGB images
		for(row=rows-1; row>=0; row--) {
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; column++) {
				unsigned char red,green,blue,alphabyte;
				switch (targa_header.pixel_size) {
					case 24:
							
							blue = getc(pBuff);
							green = getc(pBuff);
							red = getc(pBuff);
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
					case 32:
							blue = getc(pBuff);
							green = getc(pBuff);
							red = getc(pBuff);
							alphabyte = getc(pBuff);
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
				}
			}
		}
	}
	else if (targa_header.image_type==10) {   // Runlength encoded RGB images
		unsigned char red,green,blue,alphabyte,packetHeader,packetSize,j;
		for(row=rows-1; row>=0; row--) {
			pixbuf = targa_rgba + row*columns*4;
			for(column=0; column<columns; ) 
      {
				packetHeader=getc(pBuff);
				packetSize = 1 + (packetHeader & 0x7f);
				if (packetHeader & 0x80) 
        {        // run-length packet
					switch (targa_header.pixel_size) {
						case 24:
								blue = getc(pBuff);
								green = getc(pBuff);
								red = getc(pBuff);
								alphabyte = 255;
								break;
						case 32:
								blue = getc(pBuff);
								green = getc(pBuff);
								red = getc(pBuff);
								alphabyte = getc(pBuff);
								break;
					}
	
					for(j=0;j<packetSize;j++) {
						*pixbuf++=red;
						*pixbuf++=green;
						*pixbuf++=blue;
						*pixbuf++=alphabyte;
						column++;
						if (column==columns) { // run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}
					}
				}
				else {                            // non run-length packet
					for(j=0;j<packetSize;j++) {
						switch (targa_header.pixel_size) {
							case 24:
									blue = getc(pBuff);
									green = getc(pBuff);
									red = getc(pBuff);
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = 255;
									break;
							case 32:
									blue = getc(pBuff);
									green = getc(pBuff);
									red = getc(pBuff);
									alphabyte = getc(pBuff);
									*pixbuf++ = red;
									*pixbuf++ = green;
									*pixbuf++ = blue;
									*pixbuf++ = alphabyte;
									break;
						}
						column++;
						if (column==columns) { // pixel packet run spans across rows
							column=0;
							if (row>0)
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row*columns*4;
						}						
					}
				}
			}
			breakOut:;
		}
	}
  free(raw);
}


// tacky suff for time/date compares...
//
char		gsFoundFilename[MAX_PATH];
FILETIME	gtFileTime;
bool		gbFileTimeValid;
extern bool GetFileTime(LPCSTR psFileName, FILETIME &ft);
void LoadTGA (char *name, byte **pixels, int *width, int *height)
{
	// try a TGA first...
	//
	LoadTGA_Actual(name, pixels, width, height);

	if (*pixels)
	{
		strcpy(gsFoundFilename,name);
		gbFileTimeValid = GetFileTime(gsFoundFilename, gtFileTime);
		return;
	}

#ifdef QUAKE3
	// Trek version only:   didn't find the TGA, so let's try a JPG instead...
	//
	char sJPGFilename[MAX_PATH];

	strcpy(sJPGFilename,name);
	strlwr(sJPGFilename);
	char *p = strstr(sJPGFilename,".tga");
	if (p)
	{
		strcpy(p,".jpg");
		LoadJPG( sJPGFilename, pixels, width, height );
		if (*pixels)
		{
			strcpy(gsFoundFilename,sJPGFilename);
			gbFileTimeValid = GetFileTime(gsFoundFilename, gtFileTime);
			return;
		}
	}
#endif	
}



