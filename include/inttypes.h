#ifndef INTTYPES_H
#define INTTYPES_H

#include <stdint.h>

#define PRId8			"d"
#define PRId16			"d"
#define PRId32			"ld"

#define PRIdLEAST8		"d"
#define PRIdLEAST16		"d"
#define PRIdLEAST32		"ld"

#define PRIdFAST8		"d"
#define PRIdFAST16		"d"
#define PRIdFAST32		"ld"

#define PRIdMAX 		"ld"
#define PRIdPTR 		"d"


#define PRIo8			"o"
#define PRIo16			"o"
#define PRIo32			"lo"

#define PRIoLEAST8		"o"
#define PRIoLEAST16		"o"
#define PRIoLEAST32		"lo"

#define PRIoFAST8		"o"
#define PRIoFAST16		"o"
#define PRIoFAST32		"lo"

#define PRIoMAX 		"lo"
#define PRIoPTR 		"o"

#define PRIu8			"u"
#define PRIu16			"u"
#define PRIu32			"lu"

#define PRIuLEAST8		"u"
#define PRIuLEAST16		"u"
#define PRIuLEAST32		"lu"

#define PRIuFAST8		"u"
#define PRIuFAST16		"u"
#define PRIuFAST32		"lu"

#define PRIuMAX 		"lu"
#define PRIuPTR 		"u"


#define PRIx8			"x"
#define PRIx16			"x"
#define PRIx32			"lx"

#define PRIxLEAST8		"x"
#define PRIxLEAST16		"x"
#define PRIxLEAST32		"lx"

#define PRIxFAST8		"x"
#define PRIxFAST16		"x"
#define PRIxFAST32		"lx"

#define PRIxMAX 		"lx"
#define PRIxPTR 		"x"


#define PRIX8			"X"
#define PRIX16			"X"
#define PRIX32			"lX"

#define PRIXLEAST8		"X"
#define PRIXLEAST16		"X"
#define PRIXLEAST32		"lX"

#define PRIXFAST8		"X"
#define PRIXFAST16		"X"
#define PRIXFAST32		"lX"

#define PRIXMAX 		"lX"
#define PRIXPTR 		"X"


typedef struct {
	intmax_t quot;
	intmax_t rem;
} imaxdiv_t;

intmax_t  imaxabs(intmax_t n);
imaxdiv_t imaxdiv(intmax_t l, intmax_t r);
intmax_t  strtoimax(const char * s, char ** endp, int base);
uintmax_t strtoumax(const char * s, char ** endp, int base);


#pragma compile("inttypes.c")

#endif
