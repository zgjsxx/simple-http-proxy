#ifndef SRS_PROTOCOL_STREAM_HPP
#define SRS_PROTOCOL_STREAM_HPP

#include <srs_core.hpp>
#include <srs_kernel_io.hpp>
#include <srs_kernel_error.hpp>

class SrsFastStream
{
private:
    // the user-space buffer to fill by reader,
    // which use fast index and reset when chunk body read ok.
    // @see https://github.com/ossrs/srs/issues/248
    // ptr to the current read position.
    char* p;
    // ptr to the content end.
    char* end;
    // ptr to the buffer.
    //      buffer <= p <= end <= buffer+nb_buffer
    char* buffer;
    // the size of buffer.
    int nb_buffer;
public:
    // If buffer is 0, use default size.
    SrsFastStream(int size=0);
    virtual ~SrsFastStream();
public:
    /**
     * get the size of current bytes in buffer.
     */
    virtual int size();
    virtual void set_buffer(int buffer_size);    
    virtual char* read_slice(int size);
    virtual char* bytes();
        /**
     * read 1byte from buffer, move to next bytes.
     * @remark assert buffer already grow(1).
     */
    virtual char read_1byte();
    /**
     * skip some bytes in buffer.
     * @param size the bytes to skip. positive to next; negative to previous.
     * @remark assert buffer already grow(size).
     * @remark always use read_slice to consume bytes, which will reset for EOF.
     *       while skip never consume bytes.
     */
    virtual void skip(int size);    
public:
    /**
     * grow buffer to atleast required size, loop to read from skt to fill.
     * @param reader, read more bytes from reader to fill the buffer to required size.
     * @param required_size, loop to fill to ensure buffer size to required.
     * @return an int error code, error if required_size negative.
     * @remark, we actually maybe read more than required_size, maybe 4k for example.
     */
    virtual srs_error_t grow(ISrsReader* reader, int required_size);

};

#endif