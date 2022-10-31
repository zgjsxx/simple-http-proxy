#ifndef SRS_KERNEL_FILE_HPP
#define SRS_KERNEL_FILE_HPP

#include <srs_core.hpp>
#include <string>
#include <srs_kernel_io.hpp>
#include <srs_kernel_error.hpp>

class SrsFileReader;

/**
 * file writer, to write to file.
 */
class SrsFileWriter : public ISrsWriteSeeker
{
private:
    std::string path;
    int fd;
public:
    SrsFileWriter();
    virtual ~SrsFileWriter();
public:
    /**
     * open file writer, in truncate mode.
     * @param p a string indicates the path of file to open.
     */
    virtual srs_error_t open(std::string p);
    /**
     * open file writer, in append mode.
     * @param p a string indicates the path of file to open.
     */
    virtual srs_error_t open_append(std::string p);
    /**
     * close current writer.
     * @remark user can reopen again.
     */
    virtual void close();
public:
    virtual bool is_open();
    virtual void seek2(int64_t offset);
    virtual int64_t tellg();
// Interface ISrsWriteSeeker
public:
    virtual srs_error_t write(void* buf, size_t count, ssize_t* pnwrite);
    virtual srs_error_t writev(const iovec* iov, int iovcnt, ssize_t* pnwrite);
    virtual srs_error_t lseek(off_t offset, int whence, off_t* seeked);
};

/**
 * file reader, to read from file.
 */
class SrsFileReader : public ISrsReadSeeker
{
private:
    std::string path;
    int fd;
public:
    SrsFileReader();
    virtual ~SrsFileReader();
public:
    /**
     * open file writer, in truncate mode.
     * @param p a string indicates the path of file to open.
     */
    virtual srs_error_t open(std::string p);
    /**
     * close current reader.
     * @remark user can reopen again.
     */
    virtual void close();
public:
    virtual bool is_open();   
    virtual int64_t tellg(); 
    virtual void skip(int64_t size);
    virtual int64_t seek2(int64_t offset);
    virtual int64_t filesize();
    // Interface ISrsReadSeeker
public:
    virtual srs_error_t read(void* buf, size_t count, ssize_t* pnread);
    virtual srs_error_t lseek(off_t offset, int whence, off_t* seeked);
};

// For utest to mock it.
typedef int (*srs_open_t)(const char* path, int oflag, ...);
typedef ssize_t (*srs_write_t)(int fildes, const void* buf, size_t nbyte);
typedef ssize_t (*srs_read_t)(int fildes, void* buf, size_t nbyte);
typedef off_t (*srs_lseek_t)(int fildes, off_t offset, int whence);
typedef int (*srs_close_t)(int fildes);

#endif