#if 0

#ifndef _INC_H_
#define _INC_H_

typedef char           Int8;
typedef short          Int16;
typedef long           Int32;
typedef unsigned char  UInt8;
typedef unsigned short UInt16;
typedef unsigned long  UInt32;
typedef float          Float32;
typedef double         Float64;
#define MAX(a, b)              (((a) > (b)) ? (a) : (b))
#define MIN(a, b)              (((a) < (b)) ? (a) : (b))
#define RANDOM(x)              (random() % (x))
#define RANDOMIZE()             srand((int) time(NULL))

#define FTYPE_UNKNOWN 0
#define FTYPE_IWAD    1    /* .wad  "IWAD" */
#define FTYPE_PWAD    2    /* .wad  "PWAD" */
#define FTYPE_PACK    3    /* .pak  "PACK" */
#define FTYPE_WAD2    4    /* .wad  "WAD2" */
#define FTYPE_BSP     10   /* .bsp  (0x17 0x00 0x00 0x00) */
#define FTYPE_MODEL   11   /* .mdl  "IDPO" */
#define FTYPE_SPRITE  12   /* .spr  "IDSP" */
#define FTYPE_WAV     20   /* .wav  "RIFF" */
#define FTYPE_AU      21   /* .au   ".snd" */
#define FTYPE_VOC     22   /* .voc  ?      */
#define FTYPE_PBM_ASC 30   /* .pbm  "P1"   */
#define FTYPE_PGM_ASC 31   /* .pgm  "P2"   */
#define FTYPE_PPM_ASC 32   /* .ppm  "P3"   */
#define FTYPE_PBM_RAW 33   /* .pbm  "P4"   */
#define FTYPE_PGM_RAW 34   /* .pgm  "P5"   */
#define FTYPE_PPM_RAW 35   /* .ppm  "P6"   */
#define FTYPE_BMP     36   /* .bmp  "BM"   */
#define FTYPE_GIF     37   /* .gif  "GIF8" */
#define FTYPE_PCX     38   /* .pcx  (0x0a 0x05 0x01 0x08) */
#define FTYPE_ERROR   -1

#ifdef FAT_ENDIAN
Bool	ReadInt16		(FILE *file, UInt16 huge *x);
Bool	ReadInt32		(FILE *file, UInt32 huge *x);
Bool	ReadFloat32		(FILE *file, Float32 huge *x);
Bool	WriteInt16		(FILE *file, UInt16 huge *x);
Bool	WriteInt32		(FILE *file, UInt32 huge *x);
Bool	WriteFloat32	(FILE *file, Float32 huge *x);
UInt16	SwapInt16		(UInt16 x);
UInt32	SwapInt32		(UInt32 x);
Float32	SwapFloat32		(Float32 x);
#else
#define ReadInt16(f, p)		ReadBytes((f), (p), 2L)
#define ReadInt32(f, p)		ReadBytes((f), (p), 4L)
#define ReadFloat32(f, p)	ReadBytes((f), (p), 4L)
#define WriteInt16(f, p)	WriteBytes((f), (p), 2L)
#define WriteInt32(f, p)	WriteBytes((f), (p), 4L)
#define WriteFloat32(f, p)	WriteBytes((f), (p), 4L)
#define SwapInt16(x)		(x)
#define SwapInt32(x)		(x)
#define SwapFloat32(x)		(x)
#endif /* FAT_ENDIAN */

#define FROMDISK	-1
struct PACKDirectory
{
   char   name[56];             /* name of file */
   UInt32 offset;               /* offset to start of data */
   UInt32 size;                 /* byte size of data */
};
typedef struct PACKDirectory *PACKDirPtr;

typedef struct DirListStruct
{
	char					dirname[256];
	int						from;
	struct	DirListStruct	*next;
} DIRLIST;

typedef struct FileListStruct
{
	char					filename[64];
	UInt32					offset;
	UInt32					size;
	struct	FileListStruct	*next;
} FILELIST;

typedef struct DirStruct
{
	char				name[64];
	FILELIST			*files;
	struct DirStruct	*next;
} DIRECTORY;

extern int m_nPAKIndex;
extern FILE* pakfile[16];
extern BOOL			pakopen;
extern DIRECTORY	*paktextures;

void	ClearFileList				(FILELIST **);
void	ClearDirList				(DIRLIST **);
int		GetPackFileList				(FILELIST **, char *);
int		GetPackTextureDirs			(DIRLIST **);
BOOL	AddToDirListAlphabetized	(DIRLIST **, char *, int);
BOOL	AddToFileListAlphabetized	(FILELIST **t, char *, UInt32, UInt32, BOOL);
BOOL	PakLoadFile					(char *, void **);
void	OpenPakFile					(char *);
void	ClosePakFile				(void);
int PakLoadAnyFile(char *filename, void **bufferptr);

#endif

#endif
