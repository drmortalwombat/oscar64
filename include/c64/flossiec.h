#ifndef FLOSSIEC_H
#define FLOSSIEC_H

// When building you can use various defines to change the behaviour

// FLOSSIEC_BORDER=1		Enable border flashing while loading
// FLOSSIEC_NODISPLAY=1		Disable the display while loading
// FLOSSIEC_NOIRQ=1			Disable IRQ during load	
// FLOSSIEC_CODE=cseg		Code segment to be used, when defined
// FLOSSIEC_BSS=bseg		BSS segment to be used, when defined

// Initialize the fastloader to be used without the kernal
bool flossiec_init(char drive);

// Shutdown the fastloader when used without the kernal
void flossiec_shutdown(void);

// Open a file for read with the fastloader without the kernal.
// The file has to be read to completion before you can close
// it again,
bool flossiec_open(char track, char sector);

// Close a file after reading
void flossiec_close(void);


// Initialize the fastloader to be used with the kernal
bool flosskio_init(char drive);

// Shutdown the fastloader when used with the kernal
void flosskio_shutdown(void);


// Open a file for read with the fastloader with the kernal
// The file has to be read to completion before you can close
// it again,
bool flosskio_open(char track, char sector);

// Close a file after reading
void flosskio_close(void);


// Track and sector start of a file
struct floss_blk
{
	char	track, sector;
};

// Map a comma separated list of filenames to an array of
// block start positions by reading the directory, using the
// kernal.
bool flosskio_mapdir(const char * fnames, floss_blk * blks);

// Map a comma separated list of filenames to an array of
// block start positions by reading the directory, without the
// kernal.
bool flossiec_mapdir(const char * fnames, floss_blk * blks);

// Check for end of file while reading
inline bool flossiec_eof(void);

// Get one char from uncompressed file
inline char flossiec_get(void);

// Get one char from compressed file
inline char flossiec_get_lzo(void);

// Read a section of a file into memory up to size bytes,
// returns the first address after the read
char * flossiec_read(char * dp, unsigned size);

// Read and expand section of a file into memory up to size 
// bytes, returns the first address after the read
char * flossiec_read_lzo(char * dp, unsigned size);


#pragma compile("flossiec.c")

#endif
