#include <netinet/tcp.h> 
#include <algorithm>
#include <map>
#include <malloc.h>
#include <crypto/err.h>
#include <srs_app_conn.hpp>
#include <netinet/in.h>
#include <srs_kernel_error.hpp>
#include <srs_kernel_log.hpp>
#include <srs_protocol_log.hpp>
#include <srs_app_log.hpp>
#include <srs_core_auto_free.hpp>

using std::vector;
using std::map;
using std::string;

extern EVP_PKEY *ca_key;

SrsResourceManager::SrsResourceManager(const std::string& label, bool verbose)
{
    verbose_ = verbose;
    label_ = label;
    cond = srs_cond_new();
    trd = NULL;
    p_disposing_ = NULL;
    removing_ = false;

    nn_level0_cache_ = 100000;
    // conns_level0_cache_ = new SrsResourceFastIdItem[nn_level0_cache_];
}

SrsResourceManager::~SrsResourceManager()
{
    if (trd) {
        srs_cond_signal(cond);
        trd->stop();

        srs_freep(trd);
        srs_cond_destroy(cond);
    }

    // clear();

    // srs_freepa(conns_level0_cache_);
}

void SrsResourceManager::add(ISrsResource* conn, bool* exists)
{
    if (std::find(conns_.begin(), conns_.end(), conn) == conns_.end()) {
        conns_.push_back(conn);
    } else {
        if (exists) {
            *exists = true;
        }
    }
}

srs_error_t SrsResourceManager::start()
{
    srs_error_t err = srs_success;

    cid_ = _srs_context->generate_id();
    trd = new SrsSTCoroutine("manager", this, cid_);

    if ((err = trd->start()) != srs_success) {
        return srs_error_wrap(err, "conn manager");
    }

    return err;
}

srs_error_t SrsResourceManager::cycle()
{
    srs_error_t err = srs_success;

    srs_trace("%s: connection manager run, conns=%d", label_.c_str(), (int)conns_.size());

    while (true) {
        if ((err = trd->pull()) != srs_success) {
            return srs_error_wrap(err, "conn manager");
        }

        // Clear all zombies, because we may switch context and lost signal
        // when we clear zombie connection.
        while (!zombies_.empty()) {
            clear();
        }
        srs_trace("current connection is %d", conns_.size());

        srs_cond_wait(cond);
    }

    return err;
}

void SrsResourceManager::clear()
{
    if (zombies_.empty()) {
        return;
    }

    SrsContextRestore(cid_);
    if (verbose_) {
        srs_trace("%s: clear zombies=%d resources, conns=%d, removing=%d, unsubs=%d",
            label_.c_str(), (int)zombies_.size(), (int)conns_.size(), removing_, (int)unsubs_.size());
    }

    // Clear all unsubscribing handlers, if not removing any resource.
    if (!removing_ && !unsubs_.empty()) {
        vector<ISrsDisposingHandler*>().swap(unsubs_);
    }

    do_clear();
}

void SrsResourceManager::do_clear()
{
    // To prevent thread switch when delete connection,
    // we copy all connections then free one by one.
    vector<ISrsResource*> copy;
    copy.swap(zombies_);
    p_disposing_ = &copy;

    for (int i = 0; i < (int)copy.size(); i++) {
        ISrsResource* conn = copy.at(i);

        if (verbose_) {
            _srs_context->set_id(conn->get_id());
            srs_trace("%s: disposing #%d resource(%s)(%p), conns=%d, disposing=%d, zombies=%d", label_.c_str(),
                i, conn->desc().c_str(), conn, (int)conns_.size(), (int)copy.size(), (int)zombies_.size());
        }

        // ++_srs_pps_dispose->sugar;

        dispose(conn);
    }

    // Reset it for it points to a local object.
    // @remark We must set the disposing to NULL to avoid reusing address,
    // because the context might switch.
    p_disposing_ = NULL;

    // We should free the resources when finished all disposing callbacks,
    // which might cause context switch and reuse the freed addresses.
    for (int i = 0; i < (int)copy.size(); i++) {
        srs_trace("free conn");
        ISrsResource* conn = copy.at(i);
        srs_freep(conn);
        
        // malloc_trim(0);
    }
}

void SrsResourceManager::dispose(ISrsResource* c)
{
    for (map<string, ISrsResource*>::iterator it = conns_name_.begin(); it != conns_name_.end();) {
        if (c != it->second) {
            ++it;
        } else {
            // Use C++98 style: https://stackoverflow.com/a/4636230
            conns_name_.erase(it++);
        }
    }

    for (map<string, ISrsResource*>::iterator it = conns_id_.begin(); it != conns_id_.end();) {
        if (c != it->second) {
            ++it;
        } else {
            // Use C++98 style: https://stackoverflow.com/a/4636230
            conns_id_.erase(it++);
        }
    }

    // for (map<uint64_t, ISrsResource*>::iterator it = conns_fast_id_.begin(); it != conns_fast_id_.end();) {
    //     if (c != it->second) {
    //         ++it;
    //     } else {
    //         // Update the level-0 cache for fast-id.
    //         uint64_t id = it->first;
    //         SrsResourceFastIdItem* item = &conns_level0_cache_[(id | id>>32) % nn_level0_cache_];
    //         item->nn_collisions--;
    //         if (!item->nn_collisions) {
    //             item->fast_id = 0;
    //             item->available = false;
    //         }

    //         // Use C++98 style: https://stackoverflow.com/a/4636230
    //         conns_fast_id_.erase(it++);
    //     }
    // }

    vector<ISrsResource*>::iterator it = std::find(conns_.begin(), conns_.end(), c);
    if (it != conns_.end()) {
        conns_.erase(it);
    }

    // We should copy all handlers, because it may change during callback.
    vector<ISrsDisposingHandler*> handlers = handlers_;

    // Notify other handlers to handle the disposing event.
    for (int i = 0; i < (int)handlers.size(); i++) {
        ISrsDisposingHandler* h = handlers.at(i);

        // Ignore if handler is unsubscribing.
        if (!unsubs_.empty() && std::find(unsubs_.begin(), unsubs_.end(), h) != unsubs_.end()) {
            srs_warn2(TAG_RESOURCE_UNSUB, "%s: ignore disposing resource(%s)(%p) for %p, conns=%d",
                label_.c_str(), c->desc().c_str(), c, h, (int)conns_.size());
            continue;
        }

        h->on_disposing(c);
    }
}

void SrsResourceManager::remove(ISrsResource* c)
{
    SrsContextRestore(_srs_context->get_id());

    removing_ = true;
    srs_trace("do remove");
    do_remove(c);
    removing_ = false;
}

void SrsResourceManager::do_remove(ISrsResource* c)
{
    bool in_zombie = false;
    bool in_disposing = false;
    check_remove(c, in_zombie, in_disposing);
    bool ignored = in_zombie || in_disposing;

    if (verbose_) {
        _srs_context->set_id(c->get_id());
        srs_trace("%s: before dispose resource(%s)(%p), conns=%d, zombies=%d, ign=%d, inz=%d, ind=%d",
            label_.c_str(), c->desc().c_str(), c, (int)conns_.size(), (int)zombies_.size(), ignored,
            in_zombie, in_disposing);
    }
    if (ignored) {
        return;
    }

    // Push to zombies, we will free it in another coroutine.
    zombies_.push_back(c);

    // We should copy all handlers, because it may change during callback.
    vector<ISrsDisposingHandler*> handlers = handlers_;

    // Notify other handlers to handle the before-dispose event.
    for (int i = 0; i < (int)handlers.size(); i++) {
        ISrsDisposingHandler* h = handlers.at(i);

        // Ignore if handler is unsubscribing.
        if (!unsubs_.empty() && std::find(unsubs_.begin(), unsubs_.end(), h) != unsubs_.end()) {
            srs_warn2(TAG_RESOURCE_UNSUB, "%s: ignore before-dispose resource(%s)(%p) for %p, conns=%d",
                label_.c_str(), c->desc().c_str(), c, h, (int)conns_.size());
            continue;
        }

        h->on_before_dispose(c);
    }

    // Notify the coroutine to free it.
    srs_cond_signal(cond);
}

void SrsResourceManager::check_remove(ISrsResource* c, bool& in_zombie, bool& in_disposing)
{
    // Only notify when not removed(in zombies_).
    vector<ISrsResource*>::iterator it = std::find(zombies_.begin(), zombies_.end(), c);
    if (it != zombies_.end()) {
        in_zombie = true;
    }

    // Also ignore when we are disposing it.
    if (p_disposing_) {
        it = std::find(p_disposing_->begin(), p_disposing_->end(), c);
        if (it != p_disposing_->end()) {
            in_disposing = true;
        }
    }
}

SrsTcpConnection::SrsTcpConnection(srs_netfd_t c)
{
    stfd = c;
    skt = new SrsStSocket(c);
}

SrsTcpConnection::~SrsTcpConnection()
{
    srs_freep(skt);
    srs_close_stfd(stfd);
}

int SrsTcpConnection::get_fd()
{
    return srs_netfd_fileno(stfd);
}

srs_error_t SrsTcpConnection::set_tcp_nodelay(bool v)
{
    srs_error_t err = srs_success;

    int r0 = 0;
    socklen_t nb_v = sizeof(int);
    int fd = srs_netfd_fileno(stfd);

    int ov = 0;
    if ((r0 = getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &ov, &nb_v)) != 0) {
        return srs_error_new(ERROR_SOCKET_NO_NODELAY, "getsockopt fd=%d, r0=%d", fd, r0);
    }

#ifndef SRS_PERF_TCP_NODELAY
    srs_warn("ignore TCP_NODELAY, fd=%d, ov=%d", fd, ov);
    return err;
#endif

    int iv = (v? 1:0);
    if ((r0 = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &iv, nb_v)) != 0) {
        return srs_error_new(ERROR_SOCKET_NO_NODELAY, "setsockopt fd=%d, r0=%d", fd, r0);
    }
    if ((r0 = getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &iv, &nb_v)) != 0) {
        return srs_error_new(ERROR_SOCKET_NO_NODELAY, "getsockopt fd=%d, r0=%d", fd, r0);
    }

    srs_trace("set fd=%d TCP_NODELAY %d=>%d", fd, ov, iv);

    return err;
}

void SrsTcpConnection::set_recv_timeout(srs_utime_t tm)
{
    skt->set_recv_timeout(tm);
}

srs_utime_t SrsTcpConnection::get_recv_timeout()
{
    return skt->get_recv_timeout();
}

srs_error_t SrsTcpConnection::read_fully(void* buf, size_t size, ssize_t* nread)
{
    return skt->read_fully(buf, size, nread);
}

int64_t SrsTcpConnection::get_recv_bytes()
{
    return skt->get_recv_bytes();
}

int64_t SrsTcpConnection::get_send_bytes()
{
    return skt->get_send_bytes();
}

srs_error_t SrsTcpConnection::read(void* buf, size_t size, ssize_t* nread)
{
    return skt->read(buf, size, nread);
}

srs_error_t SrsTcpConnection::peek(void* buf, size_t size, ssize_t* nread)
{
    return skt->peek(buf, size, nread);
}

void SrsTcpConnection::set_send_timeout(srs_utime_t tm)
{
    skt->set_send_timeout(tm);
}

srs_utime_t SrsTcpConnection::get_send_timeout()
{
    return skt->get_send_timeout();
}

srs_error_t SrsTcpConnection::write(void* buf, size_t size, ssize_t* nwrite)
{
    return skt->write(buf, size, nwrite);
}

srs_error_t SrsTcpConnection::writev(const iovec *iov, int iov_size, ssize_t* nwrite)
{
    return skt->writev(iov, iov_size, nwrite);
}


ISrsExpire::ISrsExpire()
{
}

ISrsExpire::~ISrsExpire()
{
}

SrsSslConnection::SrsSslConnection(ISrsProtocolReadWriter* c)
{
    transport = c;
    ssl_ctx = NULL;
    ssl = NULL;
}

SrsSslConnection::~SrsSslConnection()
{
    if (ssl) {
        // this function will free bio_in and bio_out
        SSL_free(ssl);
        ssl = NULL;
    }

    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
    }
}

int SrsSslConnection::get_fd()
{
    return transport->get_fd();
}

srs_error_t SrsSslConnection::handshake(string key_file, string crt_file)
{
    srs_error_t err = srs_success;

    // For HTTPS, try to connect over security transport.
#if (OPENSSL_VERSION_NUMBER < 0x10002000L) // v1.0.2
    ssl_ctx = SSL_CTX_new(TLS_method());
#else
    ssl_ctx = SSL_CTX_new(TLSv1_2_method());
    // ssl_ctx = SSL_CTX_new(TLS_server_method());
#endif
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
    srs_assert(SSL_CTX_set_cipher_list(ssl_ctx, "ALL") == 1);

    // TODO: Setup callback, see SSL_set_ex_data and SSL_set_info_callback
    if ((ssl = SSL_new(ssl_ctx)) == NULL) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "SSL_new ssl");
    }

    if ((bio_in = BIO_new(BIO_s_mem())) == NULL) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_new in");
    }

    if ((bio_out = BIO_new(BIO_s_mem())) == NULL) {
        BIO_free(bio_in);
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_new out");
    }

    SSL_set_bio(ssl, bio_in, bio_out);

    // SSL setup active, as server role.
    SSL_set_accept_state(ssl);
    SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);

    uint8_t* data = NULL;
    int r0, r1, size;

    // Setup the key and cert file for server.
    if ((r0 = SSL_use_certificate_file(ssl, crt_file.c_str(), SSL_FILETYPE_PEM)) != 1) {
        return srs_error_new(ERROR_HTTPS_KEY_CRT, "use cert %s", crt_file.c_str());
    }

    if ((r0 = SSL_use_RSAPrivateKey_file(ssl, key_file.c_str(), SSL_FILETYPE_PEM)) != 1) {
        return srs_error_new(ERROR_HTTPS_KEY_CRT, "use key %s", key_file.c_str());
    }

    if ((r0 = SSL_check_private_key(ssl)) != 1) {
        return srs_error_new(ERROR_HTTPS_KEY_CRT, "check key %s with cert %s",
            key_file.c_str(), crt_file.c_str());
    }
    srs_info("ssl: use key %s and cert %s", key_file.c_str(), crt_file.c_str());

    // Receive ClientHello
    while (true) {
        char buf[1024]; ssize_t nn = 0;
        if ((err = transport->read(buf, sizeof(buf), &nn)) != srs_success) {
            return srs_error_wrap(err, "handshake: read");
        }

        if ((r0 = BIO_write(bio_in, buf, nn)) <= 0) {
            // TODO: 0 or -1 maybe block, use BIO_should_retry to check.
            return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_write r0=%d, data=%p, size=%d", r0, buf, nn);
        }

        r0 = SSL_do_handshake(ssl); r1 = SSL_get_error(ssl, r0);
        if (r0 != -1 || r1 != SSL_ERROR_WANT_READ) {
            return srs_error_new(ERROR_HTTPS_HANDSHAKE, "handshake r0=%d, r1=%d", r0, r1);
        }

        if ((size = BIO_get_mem_data(bio_out, &data)) > 0) {
            // OK, reset it for the next write.
            if ((r0 = BIO_reset(bio_in)) != 1) {
                return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_reset r0=%d", r0);
            }
            break;
        }
    }

    srs_info("https: ClientHello done");

    // Send ServerHello, Certificate, Server Key Exchange, Server Hello Done
    size = BIO_get_mem_data(bio_out, &data);
    if (!data || size <= 0) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "handshake data=%p, size=%d", data, size);
    }
    if ((err = transport->write(data, size, NULL)) != srs_success) {
        return srs_error_wrap(err, "handshake: write data=%p, size=%d", data, size);
    }
    if ((r0 = BIO_reset(bio_out)) != 1) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_reset r0=%d", r0);
    }

    srs_info("https: ServerHello done");

    // Receive Client Key Exchange, Change Cipher Spec, Encrypted Handshake Message
    while (true) {
        char buf[1024]; ssize_t nn = 0;
        if ((err = transport->read(buf, sizeof(buf), &nn)) != srs_success) {
            return srs_error_wrap(err, "handshake: read");
        }

        if ((r0 = BIO_write(bio_in, buf, nn)) <= 0) {
            // TODO: 0 or -1 maybe block, use BIO_should_retry to check.
            return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_write r0=%d, data=%p, size=%d", r0, buf, nn);
        }

        r0 = SSL_do_handshake(ssl); r1 = SSL_get_error(ssl, r0);
        if (r0 == 1 && r1 == SSL_ERROR_NONE) {
            break;
        }

        if (r0 != -1 || r1 != SSL_ERROR_WANT_READ) {
            return srs_error_new(ERROR_HTTPS_HANDSHAKE, "handshake r0=%d, r1=%d", r0, r1);
        }

        if ((size = BIO_get_mem_data(bio_out, &data)) > 0) {
            // OK, reset it for the next write.
            if ((r0 = BIO_reset(bio_in)) != 1) {
                return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_reset r0=%d", r0);
            }
            break;
        }
    }

    srs_info("https: Client done");

    // Send New Session Ticket, Change Cipher Spec, Encrypted Handshake Message
    size = BIO_get_mem_data(bio_out, &data);
    if (!data || size <= 0) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "handshake data=%p, size=%d", data, size);
    }
    if ((err = transport->write(data, size, NULL)) != srs_success) {
        return srs_error_wrap(err, "handshake: write data=%p, size=%d", data, size);
    }
    if ((r0 = BIO_reset(bio_out)) != 1) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_reset r0=%d", r0);
    }

    srs_info("https: Server done");

    return err;
}

srs_error_t SrsSslConnection::handshake(X509* cert, EVP_PKEY* key)
{
    srs_error_t err = srs_success;
    int r0;
    // For HTTPS, try to connect over security transport.
#if (OPENSSL_VERSION_NUMBER < 0x10002000L) // v1.0.2
    ssl_ctx = SSL_CTX_new(TLS_method());
#else
    ssl_ctx = SSL_CTX_new(TLSv1_2_method());
    // ssl_ctx = SSL_CTX_new(TLS_server_method());
#endif
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_NONE, NULL);
    srs_assert(SSL_CTX_set_cipher_list(ssl_ctx, "ALL") == 1);

    // Setup the key and cert file for server.
    if ((r0 = SSL_CTX_use_certificate(ssl_ctx, cert)) != 1) {
        return srs_error_new(ERROR_HTTPS_KEY_CRT, "SSL_CTX_use_certificate");
    }

    if ((r0 = SSL_CTX_use_PrivateKey(ssl_ctx, key)) != 1) {
        return srs_error_new(ERROR_HTTPS_KEY_CRT, "SSL_CTX_use_PrivateKey");
    }

    if ((r0 = SSL_CTX_check_private_key(ssl_ctx)) != 1) {
        return srs_error_new(ERROR_HTTPS_KEY_CRT, "SSL_check_private_key");
    }

    // TODO: Setup callback, see SSL_set_ex_data and SSL_set_info_callback
    if ((ssl = SSL_new(ssl_ctx)) == NULL) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "SSL_new ssl");
    }

    if ((bio_in = BIO_new(BIO_s_mem())) == NULL) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_new in");
    }

    if ((bio_out = BIO_new(BIO_s_mem())) == NULL) {
        BIO_free(bio_in);
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_new out");
    }

    SSL_set_bio(ssl, bio_in, bio_out);

    // SSL setup active, as server role.
    SSL_set_accept_state(ssl);
    SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);

    uint8_t* data = NULL;
    int r1, size;

    // srs_info("ssl: use key %s and cert %s", key_file.c_str(), crt_file.c_str());

    // Receive ClientHello
    while (true) {
        char buf[1024]; ssize_t nn = 0;
        if ((err = transport->read(buf, sizeof(buf), &nn)) != srs_success) {
            return srs_error_wrap(err, "handshake: read");
        }

        if ((r0 = BIO_write(bio_in, buf, nn)) <= 0) {
            // TODO: 0 or -1 maybe block, use BIO_should_retry to check.
            return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_write r0=%d, data=%p, size=%d", r0, buf, nn);
        }

        r0 = SSL_do_handshake(ssl); r1 = SSL_get_error(ssl, r0);
        if (r0 != -1 || r1 != SSL_ERROR_WANT_READ) {
            return srs_error_new(ERROR_HTTPS_HANDSHAKE, "handshake r0=%d, r1=%d", r0, r1);
        }

        if ((size = BIO_get_mem_data(bio_out, &data)) > 0) {
            // OK, reset it for the next write.
            if ((r0 = BIO_reset(bio_in)) != 1) {
                return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_reset r0=%d", r0);
            }
            break;
        }
    }

    srs_info("https: ClientHello done");

    // Send ServerHello, Certificate, Server Key Exchange, Server Hello Done
    size = BIO_get_mem_data(bio_out, &data);
    if (!data || size <= 0) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "handshake data=%p, size=%d", data, size);
    }
    if ((err = transport->write(data, size, NULL)) != srs_success) {
        return srs_error_wrap(err, "handshake: write data=%p, size=%d", data, size);
    }
    if ((r0 = BIO_reset(bio_out)) != 1) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_reset r0=%d", r0);
    }

    srs_info("https: ServerHello done");

    // Receive Client Key Exchange, Change Cipher Spec, Encrypted Handshake Message
    while (true) {
        char buf[1024]; ssize_t nn = 0;
        if ((err = transport->read(buf, sizeof(buf), &nn)) != srs_success) {
            return srs_error_wrap(err, "handshake: read");
        }

        if ((r0 = BIO_write(bio_in, buf, nn)) <= 0) {
            // TODO: 0 or -1 maybe block, use BIO_should_retry to check.
            return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_write r0=%d, data=%p, size=%d", r0, buf, nn);
        }

        r0 = SSL_do_handshake(ssl); r1 = SSL_get_error(ssl, r0);
        if (r0 == 1 && r1 == SSL_ERROR_NONE) {
            break;
        }

        if (r0 != -1 || r1 != SSL_ERROR_WANT_READ) {
            return srs_error_new(ERROR_HTTPS_HANDSHAKE, "handshake r0=%d, r1=%d", r0, r1);
        }

        if ((size = BIO_get_mem_data(bio_out, &data)) > 0) {
            // OK, reset it for the next write.
            if ((r0 = BIO_reset(bio_in)) != 1) {
                return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_reset r0=%d", r0);
            }
            break;
        }
    }

    srs_info("https: Client done");

    // Send New Session Ticket, Change Cipher Spec, Encrypted Handshake Message
    size = BIO_get_mem_data(bio_out, &data);
    if (!data || size <= 0) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "handshake data=%p, size=%d", data, size);
    }
    if ((err = transport->write(data, size, NULL)) != srs_success) {
        return srs_error_wrap(err, "handshake: write data=%p, size=%d", data, size);
    }
    if ((r0 = BIO_reset(bio_out)) != 1) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_reset r0=%d", r0);
    }

    srs_info("https: Server done");

    return err;
}

void SrsSslConnection::set_recv_timeout(srs_utime_t tm)
{
    transport->set_recv_timeout(tm);
}

srs_utime_t SrsSslConnection::get_recv_timeout()
{
    return transport->get_recv_timeout();
}

srs_error_t SrsSslConnection::read_fully(void* buf, size_t size, ssize_t* nread)
{
    return transport->read_fully(buf, size, nread);
}

// int64_t SrsSslConnection::get_recv_bytes()
// {
//     return transport->get_recv_bytes();
// }

// int64_t SrsSslConnection::get_send_bytes()
// {
//     return transport->get_send_bytes();
// }

srs_error_t SrsSslConnection::read(void* plaintext, size_t nn_plaintext, ssize_t* nread)
{
    srs_error_t err = srs_success;

    while (true) {
        int r0 = SSL_read(ssl, plaintext, nn_plaintext); int r1 = SSL_get_error(ssl, r0);
        int r2 = BIO_ctrl_pending(bio_in); int r3 = SSL_is_init_finished(ssl);

        // OK, got data.
        if (r0 > 0) {
            srs_assert(r0 <= (int)nn_plaintext);
            if (nread) {
                *nread = r0;
            }
            return err;
        }

        // Need to read more data to feed SSL.
        if (r0 == -1 && r1 == SSL_ERROR_WANT_READ) {
            // TODO: Can we avoid copy?
            int nn_cipher = nn_plaintext;
            char* cipher = new char[nn_cipher];
            SrsAutoFreeA(char, cipher);

            // Read the cipher from SSL.
            ssize_t nn = 0;
            if ((err = transport->read(cipher, nn_cipher, &nn)) != srs_success) {
                return srs_error_wrap(err, "https: read");
            }

            int r0 = BIO_write(bio_in, cipher, nn);
            if (r0 <= 0) {
                // TODO: 0 or -1 maybe block, use BIO_should_retry to check.
                return srs_error_new(ERROR_HTTPS_READ, "BIO_write r0=%d, cipher=%p, size=%d", r0, cipher, nn);
            }
            continue;
        }

        // Fail for error.
        if (r0 <= 0) {
            return srs_error_new(ERROR_HTTPS_READ, "SSL_read r0=%d, r1=%d, r2=%d, r3=%d",
                r0, r1, r2, r3);
        }
    }
}

void SrsSslConnection::set_send_timeout(srs_utime_t tm)
{
    transport->set_send_timeout(tm);
}

srs_utime_t SrsSslConnection::get_send_timeout()
{
    return transport->get_send_timeout();
}

srs_error_t SrsSslConnection::write(void* plaintext, size_t nn_plaintext, ssize_t* nwrite)
{
    srs_error_t err = srs_success;

    for (char* p = (char*)plaintext; p < (char*)plaintext + nn_plaintext;) {
        int left = (int)nn_plaintext - (p - (char*)plaintext);
        int r0 = SSL_write(ssl, (const void*)p, left);
        int r1 = SSL_get_error(ssl, r0);
        if (r0 <= 0) {
            return srs_error_new(ERROR_HTTPS_WRITE, "https: write data=%p, size=%d, r0=%d, r1=%d", p, left, r0, r1);
        }

        // Move p to the next writing position.
        p += r0;
        if (nwrite) {
            *nwrite += (ssize_t)r0;
        }

        uint8_t* data = NULL;
        int size = BIO_get_mem_data(bio_out, &data);
        if ((err = transport->write(data, size, NULL)) != srs_success) {
            return srs_error_wrap(err, "https: write data=%p, size=%d", data, size);
        }
        if ((r0 = BIO_reset(bio_out)) != 1) {
            return srs_error_new(ERROR_HTTPS_WRITE, "BIO_reset r0=%d", r0);
        }
    }

    return err;
}

srs_error_t SrsSslConnection::writev(const iovec *iov, int iov_size, ssize_t* nwrite)
{
    srs_error_t err = srs_success;

    for (int i = 0; i < iov_size; i++) {
        const iovec* p = iov + i;
        if ((err = write((void*)p->iov_base, (size_t)p->iov_len, nwrite)) != srs_success) {
            return srs_error_wrap(err, "write iov #%d base=%p, size=%d", i, p->iov_base, p->iov_len);
        }
    }

    return err;
}

// The return value of verify_callback controls the strategy of the further verification process. If verify_callback
// returns 0, the verification process is immediately stopped with "verification failed" state. If SSL_VERIFY_PEER is
// set, a verification failure alert is sent to the peer and the TLS/SSL handshake is terminated. If verify_callback
// returns 1, the verification process is continued. If verify_callback always returns 1, the TLS/SSL handshake will
// not be terminated with respect to verification failures and the connection will be established. The calling process
// can however retrieve the error code of the last verification error using SSL_get_verify_result(3) or by maintaining
// its own error storage managed by verify_callback.
// @see https://www.openssl.org/docs/man1.0.2/man3/SSL_CTX_set_verify.html
int srs_verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
    // Always OK, we don't check the certificate of client,
    // because we allow client self-sign certificate.
    return 1;
}

SrsSslClient::SrsSslClient(SrsTcpClient* tcp)
{
    transport = tcp;
    ssl_ctx = NULL;
    ssl = NULL;
}

SrsSslClient::~SrsSslClient()
{
    if (ssl) {
        // this function will free bio_in and bio_out
        SSL_free(ssl);
        ssl = NULL;
    }

    if (ssl_ctx) {
        SSL_CTX_free(ssl_ctx);
        ssl_ctx = NULL;
    }

}

srs_error_t SrsSslClient::handshake()
{
    srs_error_t err = srs_success;

    // For HTTPS, try to connect over security transport.
#if (OPENSSL_VERSION_NUMBER < 0x10002000L) // v1.0.2
    ssl_ctx = SSL_CTX_new(TLS_method());
#else
    ssl_ctx = SSL_CTX_new(TLSv1_2_method());
    // ssl_ctx = SSL_CTX_new(TLS_client_method());
#endif
    SSL_CTX_set_verify(ssl_ctx, SSL_VERIFY_PEER, srs_verify_callback);
    srs_assert(SSL_CTX_set_cipher_list(ssl_ctx, "ALL") == 1);

    // TODO: Setup callback, see SSL_set_ex_data and SSL_set_info_callback
    if ((ssl = SSL_new(ssl_ctx)) == NULL) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "SSL_new ssl");
    }
	//add SNI extension
	SSL_set_tlsext_host_name(ssl, sni_.c_str());


    if ((bio_in = BIO_new(BIO_s_mem())) == NULL) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_new in");
    }

    if ((bio_out = BIO_new(BIO_s_mem())) == NULL) {
        BIO_free(bio_in);
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_new out");
    }

    SSL_set_bio(ssl, bio_in, bio_out);

    // SSL setup active, as client role.
    SSL_set_connect_state(ssl);
    SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);

    // Send ClientHello.
    int r0 = SSL_do_handshake(ssl); int r1 = SSL_get_error(ssl, r0);
    if (r0 != -1 || r1 != SSL_ERROR_WANT_READ) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "handshake r0=%d, r1=%d", r0, r1);
    }

    uint8_t* data = NULL;
    int size = BIO_get_mem_data(bio_out, &data);
    if (!data || size <= 0) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "handshake data=%p, size=%d", data, size);
    }
    if ((err = transport->write(data, size, NULL)) != srs_success) {
        return srs_error_wrap(err, "handshake: write data=%p, size=%d", data, size);
    }
    if ((r0 = BIO_reset(bio_out)) != 1) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_reset r0=%d", r0);
    }

    srs_info("https: ClientHello done");

    // Receive ServerHello, Certificate, Server Key Exchange, Server Hello Done
    while (true) {
        char buf[512]; ssize_t nn = 0;
        if ((err = transport->read(buf, sizeof(buf), &nn)) != srs_success) {
            return srs_error_wrap(err, "handshake: read");
        }

        if ((r0 = BIO_write(bio_in, buf, nn)) <= 0) {
            // TODO: 0 or -1 maybe block, use BIO_should_retry to check.
            return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_write r0=%d, data=%p, size=%d", r0, buf, nn);
        }

        if ((r0 = SSL_do_handshake(ssl)) != -1 || (r1 = SSL_get_error(ssl, r0)) != SSL_ERROR_WANT_READ) {
            return srs_error_new(ERROR_HTTPS_HANDSHAKE, "handshake r0=%d, r1=%d", r0, r1);
        }

        if ((size = BIO_get_mem_data(bio_out, &data)) > 0) {
            // OK, reset it for the next write.
            if ((r0 = BIO_reset(bio_in)) != 1) {
                return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_reset r0=%d", r0);
            }
            break;
        }
    }

    srs_info("https: ServerHello done");

    // Send Client Key Exchange, Change Cipher Spec, Encrypted Handshake Message
    if ((err = transport->write(data, size, NULL)) != srs_success) {
        return srs_error_wrap(err, "handshake: write data=%p, size=%d", data, size);
    }
    if ((r0 = BIO_reset(bio_out)) != 1) {
        return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_reset r0=%d", r0);
    }

    srs_info("https: Client done");

    // Receive New Session Ticket, Change Cipher Spec, Encrypted Handshake Message
    while (true) {
        char buf[128];
        ssize_t nn = 0;
        if ((err = transport->read(buf, sizeof(buf), &nn)) != srs_success) {
            return srs_error_wrap(err, "handshake: read");
        }

        if ((r0 = BIO_write(bio_in, buf, nn)) <= 0) {
            // TODO: 0 or -1 maybe block, use BIO_should_retry to check.
            return srs_error_new(ERROR_HTTPS_HANDSHAKE, "BIO_write r0=%d, data=%p, size=%d", r0, buf, nn);
        }

        r0 = SSL_do_handshake(ssl); r1 = SSL_get_error(ssl, r0);
        if (r0 == 1 && r1 == SSL_ERROR_NONE) {
            break;
        }

        if (r0 != -1 || r1 != SSL_ERROR_WANT_READ) {
            return srs_error_new(ERROR_HTTPS_HANDSHAKE, "handshake r0=%d, r1=%d", r0, r1);
        }
    }

    srs_info("https: Server done");

    return err;
}

srs_error_t SrsSslClient::read(void* plaintext, size_t nn_plaintext, ssize_t* nread)
{
    srs_error_t err = srs_success;

    while (true) {
        int r0 = SSL_read(ssl, plaintext, nn_plaintext); int r1 = SSL_get_error(ssl, r0);
        int r2 = BIO_ctrl_pending(bio_in); int r3 = SSL_is_init_finished(ssl);

        // OK, got data.
        if (r0 > 0) {
            srs_assert(r0 <= (int)nn_plaintext);
            if (nread) {
                *nread = r0;
            }
            return err;
        }

        // Need to read more data to feed SSL.
        if (r0 == -1 && r1 == SSL_ERROR_WANT_READ) {
            // TODO: Can we avoid copy?
            int nn_cipher = nn_plaintext;
            char* cipher = new char[nn_cipher];
            SrsAutoFreeA(char, cipher);

            // Read the cipher from SSL.
            ssize_t nn = 0;
            if ((err = transport->read(cipher, nn_cipher, &nn)) != srs_success) {
                return srs_error_wrap(err, "https: read");
            }

            int r0 = BIO_write(bio_in, cipher, nn);
            if (r0 <= 0) {
                // TODO: 0 or -1 maybe block, use BIO_should_retry to check.
                return srs_error_new(ERROR_HTTPS_READ, "BIO_write r0=%d, cipher=%p, size=%d", r0, cipher, nn);
            }
            continue;
        }

        // Fail for error.
        if (r0 <= 0) {
            return srs_error_new(ERROR_HTTPS_READ, "SSL_read r0=%d, r1=%d, r2=%d, r3=%d",
                r0, r1, r2, r3);
        }
    }
}

srs_error_t SrsSslClient::write(void* plaintext, size_t nn_plaintext, ssize_t* nwrite)
{
    srs_error_t err = srs_success;

    for (char* p = (char*)plaintext; p < (char*)plaintext + nn_plaintext;) {
        int left = (int)nn_plaintext - (p - (char*)plaintext);
        int r0 = SSL_write(ssl, (const void*)p, left);
        int r1 = SSL_get_error(ssl, r0);
        if (r0 <= 0) {
            return srs_error_new(ERROR_HTTPS_WRITE, "https: write data=%p, size=%d, r0=%d, r1=%d", p, left, r0, r1);
        }

        // Move p to the next writing position.
        p += r0;
        if (nwrite) {
            *nwrite += (ssize_t)r0;
        }

        uint8_t* data = NULL;
        int size = BIO_get_mem_data(bio_out, &data);
        if ((err = transport->write(data, size, NULL)) != srs_success) {
            return srs_error_wrap(err, "https: write data=%p, size=%d", data, size);
        }
        if ((r0 = BIO_reset(bio_out)) != 1) {
            return srs_error_new(ERROR_HTTPS_WRITE, "BIO_reset r0=%d", r0);
        }
    }

    return err;
}

srs_error_t SrsSslClient::prepare_resign_endpoint(X509 *fake_x509, EVP_PKEY* server_key)
{
	X509 *server_x509 = SSL_get_peer_certificate(ssl);
	if(server_x509 != nullptr)
	{
		X509_NAME * issuer = X509_get_issuer_name(server_x509);
		char issuer_name[2048] = { 0 };
		X509_NAME_oneline(issuer, issuer_name, sizeof(issuer_name) - 1);
		issuer_name[sizeof(issuer_name) - 1] = 0;

		X509_NAME * subject = X509_get_subject_name(server_x509);
		char subject_name[2048] = { 0 };
				X509_NAME_oneline(subject, subject_name, sizeof(subject_name) - 1);
				subject_name[sizeof(subject_name) - 1] = 0;
		srs_trace("server certificate subject: %s ",subject_name);
		srs_trace("server certificate issuer : %s",issuer_name);

	}
	else
	{
		srs_trace("server_x509 == null");
	}

	//X509 *fake_x509 = X509_new();
	X509_set_version(fake_x509, 2);//v3
    if(X509_get_serialNumber(server_x509) != NULL)
    {
        X509_set_serialNumber(fake_x509, X509_get_serialNumber(server_x509));
    }
    else
    {
        srs_trace("serial is null");
    }
	
	X509_NAME *issuer = X509_NAME_new();
	X509_NAME_add_entry_by_txt(issuer, "CN", MBSTRING_ASC, (const unsigned char*)"www.xx.com", -1, -1, 0);
	X509_NAME_add_entry_by_txt(issuer, "O", MBSTRING_ASC, (const unsigned char*)"XX (Pty) Ltd.", -1, -1, 0);
	X509_NAME_add_entry_by_txt(issuer, "OU", MBSTRING_ASC, (const unsigned char*)"XX CA", -1,-1, 0);
	X509_NAME_add_entry_by_txt(issuer, "L", MBSTRING_ASC, (const unsigned char*)"Boston", -1,-1, 0);
	X509_NAME_add_entry_by_txt(issuer, "ST", MBSTRING_ASC, (const unsigned char*)"MA", -1,-1, 0);
	X509_NAME_add_entry_by_txt(issuer, "C", MBSTRING_ASC, (const unsigned char*)"US", -1,-1, 0);
	X509_set_issuer_name(fake_x509, issuer);
	X509_set_subject_name(fake_x509, X509_get_subject_name(server_x509));

    X509_NAME_free(issuer);

	ASN1_UTCTIME *s = ASN1_UTCTIME_new();
	int days = 365;
	X509_gmtime_adj(s, (long)60*60*24*days*(-1));

	X509_set_notBefore(fake_x509, s);
	X509_gmtime_adj(s, (long)60*60*24*days*2);
	X509_set_notAfter(fake_x509, s);
	ASN1_UTCTIME_free(s);
	int key_length = 2048;
	RSA *new_cert_rsa = get_new_cert_rsa(key_length);
	//EVP_PKEY* server_key = EVP_PKEY_new();
	EVP_PKEY_set1_RSA(server_key, new_cert_rsa);
	X509_set_pubkey(fake_x509, server_key);

    RSA_free(new_cert_rsa);

	//get subject alternative name (SAN) extension
	std::vector<int> ext_id_list = {NID_subject_alt_name};
	for(auto ext_id: ext_id_list)
	{
		X509_EXTENSION *ext_to_add = NULL;

		int extcount = X509_get_ext_count(server_x509);
		for (int i = 0; i < extcount; i++)
		{
			X509_EXTENSION * ext = X509_get_ext(server_x509, i);
			if (OBJ_obj2nid(X509_EXTENSION_get_object(ext)) == ext_id)
			{
				ext_to_add = ext;
				break;
			}
		}

		if (ext_to_add != NULL)
		{
			X509_add_ext(fake_x509, ext_to_add, -1);
		}
	}
	//end of get san

	int nid = NID_sha256WithRSAEncryption;
	X509_sign(fake_x509, ca_key, EVP_get_digestbynid(nid));
    X509_free(server_x509);
}

RSA* SrsSslClient::get_new_cert_rsa(int key_length)
 {
 	RSA *rsa = RSA_new();
 	BIGNUM *bn = BN_new();

 	if (BN_set_word(bn, RSA_F4) <= 0 ||
 			RSA_generate_key_ex(rsa, key_length, bn, NULL) <= 0)
 	{
 		abort();
 	}

 	BN_free(bn);
 	return rsa;
}

srs_error_t SrsSslClient::set_SNI(std::string sni)
{
    sni_ = sni;
}