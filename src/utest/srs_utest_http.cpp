#include <srs_utest_http.hpp>

MockResponseWriter::MockResponseWriter()
{
    w = new SrsHttpResponseWriter(&io);
    w->hf = this;
}

MockResponseWriter::~MockResponseWriter()
{
    srs_freep(w);
}

srs_error_t MockResponseWriter::final_request()
{
    return w->final_request();
}

SrsHttpHeader* MockResponseWriter::header()
{
    return w->header();
}

srs_error_t MockResponseWriter::write(char* data, int size)
{
    return w->write(data, size);
}

srs_error_t MockResponseWriter::writev(const iovec* iov, int iovcnt, ssize_t* pnwrite)
{
    return w->writev(iov, iovcnt, pnwrite);
}

void MockResponseWriter::write_header(int code)
{
    w->write_header(code);
}

srs_error_t MockResponseWriter::filter(SrsHttpHeader* h)
{
    h->del("Content-Type");
    h->del("Server");
    h->del("Connection");
    h->del("Location");
    h->del("Content-Range");
    h->del("Access-Control-Allow-Origin");
    h->del("Access-Control-Allow-Methods");
    h->del("Access-Control-Expose-Headers");
    h->del("Access-Control-Allow-Headers");
    return srs_success;
}

string mock_http_response(int status, string content)
{
    stringstream ss;
    ss << "HTTP/1.1 " << status << " " << srs_generate_http_status_text(status) << "\r\n"
        << "Content-Length: " << content.length() << "\r\n"
        << "\r\n"
        << content;
    return ss.str();
}

string mock_http_response2(int status, string content)
{
    stringstream ss;
    ss << "HTTP/1.1 " << status << " " << srs_generate_http_status_text(status) << "\r\n"
        << "Transfer-Encoding: chunked" << "\r\n"
        << "\r\n"
        << content;
    return ss.str();
}

string mock_http_response3(int status, string content)
{
    stringstream ss;
    ss << "HTTP/1.1 " << status << " " << srs_generate_http_status_text(status) << "\r\n"
        << "Server:" << "\r\n"
        << "\r\n"
        << content;
    return ss.str();
}

bool is_string_contain(string substr, string str)
{
    return (string::npos != str.find(substr));
}

MockMSegmentsReader::MockMSegmentsReader()
{
}

MockMSegmentsReader::~MockMSegmentsReader()
{
}

srs_error_t MockMSegmentsReader::read(void* buf, size_t size, ssize_t* nread)
{
    srs_error_t err = srs_success;

    for (;;) {
        if (in_bytes.empty() || size <= 0) {
            return srs_error_new(-1, "EOF");
        }

        string v = in_bytes[0];
        if (v.empty()) {
            in_bytes.erase(in_bytes.begin());
            continue;
        }

        int nn = srs_min(size, v.length());
        memcpy(buf, v.data(), nn);
        if (nread) {
            *nread = nn;
        }

        if (nn < (int)v.length()) {
            in_bytes[0] = string(v.data() + nn, v.length() - nn);
        } else {
            in_bytes.erase(in_bytes.begin());
        }
        break;
    }

    return err;
}


VOID TEST(http, ResponseWriter)
{
    srs_error_t err;
    if (true) {
        MockResponseWriter w;

        char msg[] = "Hello, world!";
        w.header()->set_content_length(sizeof(msg) - 1);
        HELPER_EXPECT_SUCCESS(w.write((char*)msg, 5));
        HELPER_EXPECT_SUCCESS(w.write((char*)(msg+5), 2));
        HELPER_EXPECT_SUCCESS(w.write((char*)(msg+7), 5));
        HELPER_EXPECT_SUCCESS(w.write((char*)(msg+12), 1));

        __MOCK_HTTP_EXPECT_STREQ(200, "Hello, world!", w);

    }
}

VOID TEST(ProtocolHTTPTest, ChunkSmallBuffer)
{
    srs_error_t err;

    // No chunk end flag, error.
    if (true) {
        MockMSegmentsReader io;
        io.append(mock_http_response2(200, "0d\r\n"));
        io.append("Hello, world!\r\n");

        SrsHttpParser hp; HELPER_ASSERT_SUCCESS(hp.initialize(HTTP_RESPONSE));
        ISrsHttpMessage* msg = NULL; HELPER_ASSERT_SUCCESS(hp.parse_message(&io, &msg));

        char buf[32]; ssize_t nread = 0;
        ISrsHttpResponseReader* r = msg->body_reader();

        HELPER_ARRAY_INIT(buf, sizeof(buf), 0);
        HELPER_ASSERT_SUCCESS(r->read(buf, 32, &nread));
        EXPECT_EQ(13, nread);
        EXPECT_STREQ("Hello, world!", buf);

        err = r->read(buf, 32, &nread);
        EXPECT_EQ(-1, srs_error_code(err));
        srs_freep(err);

        srs_freep(msg);
    }
}