//
// Copyright 2018 Sepehr Taghdisian (septag@github). All rights reserved.
// License: https://github.com/septag/sx#license-bsd-2-clause
//
// stream-io.h v1.0: Some streaming primitives
//      sx_mem_block: Memory block represents a piece of memory that can also grow with an allocator
//              sx_mem_create_block         allocates the entire continous block object with it's
//                                          memory. this type CAN NOT grow
//              sx_mem_destroy_block        destroys the allocated block, use only with
//                                          sx_mem_create_block
//              sx_mem_init_block           allocates a block of memory from allocator,
//                                          this type can grow later
//              sx_mem_release_block        destroy the memory inside the memory block object,
//                                          only use with 'sx_mem_init_block'
//              sx_mem_init_block_ptr       initializes a block of memory from pre-allocated memory,
//                                          this type CAN NOT grow
//              sx_mem_grow                 grows the memory is 'size' bytes, does not affect blocks
//                                          initialized with _ptr
//              sx_define_mem_block_onstack creates and initializes memory block on stack
//
//      sx_mem_writer: Writes to an initialized memory block for streamed writing
//              sx_mem_init_writer          initializes the writer, allocates init_size as start
//              sx_mem_release_writer       releases writer memory
//              sx_mem_write                writes a piece of data to memory
//              sx_mem_seekw                seeks inside the buffer
//              sx_mem_write_var            helper macro to write single variables
//
//      sx_mem_reader: Reads from a pre-allocated memory for streamed reading
//              sx_init_reader              initializes the read, data must be pre-allocated and
//                                          alive during the read operations
//              sx_mem_read                 reads a piece of data to memory
//              sx_mem_seekr                seeks inside the buffer
//              sx_mem_read_var             helper macro to read a single variable
//              sx_mem_get_iff_chunk        helper for reading IFF chunks
//                                          searches `size` bytes from the current location for
//                                          FOURCC chunk type. if `size` <= 0, searches till the end
//                                          if chunk is not found, returning `pos` variable will be -1                                    -1
//
//      sx_file: file related IO functions
//              sx_file_open                opens a file for writing or reading, initiates file objects
//              sx_file_close               closes an opened file
//              sx_file_read                Reads a buffer from file, offsets file pointer 
//              sx_file_write               Writes a buffer to file, offsets file pointer
//              sx_file_seek                Sets file pointer, offset is in bytes
//              
//              sx_file_load_bin            allocates memory and loads the file data into it
//              sx_file_load_text           Same as binary, but appends a null terminator to the end of the buffer
//              
//              sx_file_write_var           Helper macro: writes a variable to file (no need for sizeof)
//              sx_file_write_text          Helper macro: writes a string to file (no need for strlen)
//              sx_file_read_var            Helper macro: reads a variable from file (no need for sizeof)
//
#pragma once

#include "sx.h"

typedef struct sx_alloc sx_alloc;

#ifndef sx_data_truncate
#    define sx_data_truncate() sx_assert_rel(0 && "Data truncated !")
#endif

typedef enum sx_whence { SX_WHENCE_BEGIN = 0, SX_WHENCE_CURRENT, SX_WHENCE_END } sx_whence;

// sx_mem_block
typedef struct sx_mem_block {
    const sx_alloc* alloc;
    void* data;
    int64_t size;
    int align;
} sx_mem_block;

SX_API sx_mem_block* sx_mem_create_block(const sx_alloc* alloc, int64_t size,
                                         const void* data sx_default(NULL),
                                         int align sx_default(0));
SX_API sx_mem_block* sx_mem_ref_block(const sx_alloc* alloc, int64_t size, void* data);
SX_API void sx_mem_destroy_block(sx_mem_block* mem);

SX_API void sx_mem_init_block_ptr(sx_mem_block* mem, void* data, int64_t size);
SX_API bool sx_mem_grow(sx_mem_block** pmem, int64_t size);

#define sx_define_mem_block_onstack(_name, _size) \
    uint8_t _name##_buff_[(_size)];               \
    sx_mem_block _name;                           \
    sx_mem_init_block_ptr(&(_name), _name##_buff_, (_size));

// sx_mem_writer
typedef struct sx_mem_writer {
    sx_mem_block* mem;
    uint8_t* data;
    int64_t pos;
    int64_t top;
    int64_t size;
} sx_mem_writer;

SX_API void sx_mem_init_writer(sx_mem_writer* writer, const sx_alloc* alloc, int init_size);
SX_API void sx_mem_release_writer(sx_mem_writer* writer);

SX_API int sx_mem_write(sx_mem_writer* writer, const void* data, int size);
SX_API int64_t sx_mem_seekw(sx_mem_writer* writer, int64_t offset,
                            sx_whence whence sx_default(SX_WHENCE_CURRENT));

#define sx_mem_write_var(w, v) sx_mem_write((w), &(v), sizeof(v))
#define sx_mem_write_text(w, s) sx_mem_write((w), (s), sx_strlen(s))

// sx_mem_reader
typedef struct sx_mem_reader {
    const uint8_t* data;
    int64_t pos;
    int64_t top;
} sx_mem_reader;

SX_API void sx_mem_init_reader(sx_mem_reader* reader, const void* data, int64_t size);
SX_API int sx_mem_read(sx_mem_reader* reader, void* data, int size);
SX_API int64_t sx_mem_seekr(sx_mem_reader* reader, int64_t offset,
                            sx_whence whence sx_default(SX_WHENCE_CURRENT));
#define sx_mem_read_var(r, v) sx_mem_read((r), &(v), sizeof(v))

typedef struct sx_file {
    sx_align_decl(16, uint8_t) data[32];
} sx_file;

// for proper file buffering (SX_FILE_NOCACHE) alignment requirements under win32, visit:
// https://docs.microsoft.com/en-us/windows/win32/fileio/file-buffering
// as a general rule, if you use SX_FILE_NOCACHE flag, use page aligned memory buffers 
// obtained from sx_os_pagesz(), or allocate memory with virtual memory (virtual-alloc.h)
typedef enum sx_file_open_flag {
    SX_FILE_READ = 0x01,            // open for reading
    SX_FILE_WRITE = 0x02,           // open for writing
    SX_FILE_APPEND = 0x04,          // append to the end of the file (write mode only)
    SX_FILE_NOCACHE = 0x08,         // disable cache, suitable for large files, best bet is to align buffers to virtual memory pages
    SX_FILE_WRITE_THROUGH = 0x10,   // write-through, write meta information to disk immediately
    SX_FILE_SEQ_SCAN = 0x20,        // optimize cache for sequential scan (not used in NOCACHE)
    SX_FILE_RANDOM_ACCESS = 0x40,   // optimize cache for random access (not used in NOCACHE)
    SX_FILE_TEMP = 0x80
} sx_file_open_flag;
typedef uint32_t sx_file_open_flags;

SX_API bool sx_file_open(sx_file* file, const char* filepath, sx_file_open_flags flags);
SX_API void sx_file_close(sx_file* file);

SX_API int64_t sx_file_read(sx_file* file, void* data, int64_t size);
SX_API int64_t sx_file_write(sx_file* file, const void* data, int64_t size);
SX_API int64_t sx_file_seek(sx_file* file, int64_t offset, sx_whence whence);
SX_API int64_t sx_file_size(const sx_file* file);

SX_API sx_mem_block* sx_file_load_text(const sx_alloc* alloc, const char* filepath);
SX_API sx_mem_block* sx_file_load_bin(const sx_alloc* alloc, const char* filepath);

#define sx_file_write_var(w, v) sx_file_write((w), &(v), sizeof(v))
#define sx_file_write_text(w, s) sx_file_write((w), (s), sx_strlen(s))
#define sx_file_read_var(w, v) sx_file_read((w), &(v), sizeof(v))

#if 0
typedef struct sx_iff_chunk {
    int64_t pos;
    uint32_t size;
    uint32_t fourcc;
    int parent_id;
} sx_iff_chunk;

SX_API sx_iff_chunk sx_mem_get_iff_chunk(sx_mem_reader* reader, int64_t size, uint32_t fourcc);
#endif

// IFF file
// https://en.wikipedia.org/wiki/Interchange_File_Format
//
typedef struct sx_iff_chunk {
    int64_t next_chunk_offset;
    int64_t next_child_offset;
    uint32_t size;
    uint32_t fourcc;
    int parent_id;
} sx_iff_chunk;

typedef enum sx_iff_type {
    SX_IFFTYPE_MEM_READER,
    SX_IFFTYPE_MEM_WRITER,
    SX_IFFTYPE_DISK
} sx_iff_type;

typedef enum sx_iff_flag {
    SX_IFFFLAG_READ_ALL_CHUNKS = 0x1
} sx_iff_flag;
typedef uint32_t sx_iff_flags;

typedef struct sx_iff_file {
    sx_iff_type type;
    sx_iff_chunk* chunks;    // sx_array
    const sx_alloc* alloc;
    union {
        sx_mem_reader* mread;
        sx_mem_writer* mwrite;
        sx_file* disk;
    };
} sx_iff_file;

SX_API void sx_iff_init_from_file(sx_iff_file* iff, sx_file* file, sx_iff_flags flags,
                                  const sx_alloc* alloc);
SX_API void sx_iff_init_from_mem_reader(sx_iff_file* iff, sx_mem_reader* mread, sx_iff_flags flags,
                                        const sx_alloc* alloc);
SX_API void sx_iff_init_from_mem_writer(sx_iff_file* iff, sx_mem_writer* mwrite, sx_iff_flags flags,
                                        const sx_alloc* alloc);
SX_API void sx_iff_release(sx_iff_file* iff);
SX_API void sx_iff_commit(sx_iff_file* iff);

SX_API int sx_iff_get_chunk(sx_iff_file* iff, uint32_t fourcc, int parent_chunk);
SX_API int sx_iff_get_next_chunk(sx_iff_file* iff, int chunk_id, int prev_chunk);

SX_API int sx_iff_put_chunk(sx_iff_file* iff, int parent_id, uint32_t fourcc,
                            const void* chunk_data, uint32_t size);
