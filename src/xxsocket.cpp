#include "xxsocket.h"
#include <fcntl.h>

using namespace purelib;
using namespace purelib::net;

#if defined( _WIN32 )  && !defined(_WINSTORE)
extern LPFN_ACCEPTEX __accept_ex;
extern LPFN_GETACCEPTEXSOCKADDRS __get_accept_ex_sockaddrs;
#endif

namespace compat {
    ///////////// inet_ntop ///////////////
    static const char *
        inet_ntop_v4(const void *src, char *dst, size_t size)
    {
        const char digits[] = "0123456789";
        int i;
        struct in_addr *addr = (struct in_addr *)src;
        u_long a = ntohl(addr->s_addr);
        const char *orig_dst = dst;

        if (size < INET_ADDRSTRLEN) {
            errno = ENOSPC;
            return NULL;
        }
        for (i = 0; i < 4; ++i) {
            int n = (a >> (24 - i * 8)) & 0xFF;
            int non_zerop = 0;

            if (non_zerop || n / 100 > 0) {
                *dst++ = digits[n / 100];
                n %= 100;
                non_zerop = 1;
            }
            if (non_zerop || n / 10 > 0) {
                *dst++ = digits[n / 10];
                n %= 10;
                non_zerop = 1;
            }
            *dst++ = digits[n];
            if (i != 3)
                *dst++ = '.';
        }
        *dst++ = '\0';
        return orig_dst;
    }

    static const char *
        inet_ntop_v6(const void *src, char *dst, size_t size)
    {
        const char xdigits[] = "0123456789abcdef";
        int i;
        const struct in6_addr *addr = (struct in6_addr *)src;
        const u_char *ptr = addr->s6_addr;
        const char *orig_dst = dst;
        int compressed = 0;

        if (size < INET6_ADDRSTRLEN) {
            errno = ENOSPC;
            return NULL;
        }
        for (i = 0; i < 8; ++i) {
            int non_zerop = 0;

            if (compressed == 0 &&
                ptr[0] == 0 && ptr[1] == 0 &&
                i <= 5 &&
                ptr[2] == 0 && ptr[3] == 0 &&
                ptr[4] == 0 && ptr[5] == 0) {

                compressed = 1;

                if (i == 0)
                    *dst++ = ':';
                *dst++ = ':';

                for (ptr += 6, i += 3;
                    i < 8 && ptr[0] == 0 && ptr[1] == 0;
                    ++i, ptr += 2);

                if (i >= 8)
                    break;
            }

            if (non_zerop || (ptr[0] >> 4)) {
                *dst++ = xdigits[ptr[0] >> 4];
                non_zerop = 1;
            }
            if (non_zerop || (ptr[0] & 0x0F)) {
                *dst++ = xdigits[ptr[0] & 0x0F];
                non_zerop = 1;
            }
            if (non_zerop || (ptr[1] >> 4)) {
                *dst++ = xdigits[ptr[1] >> 4];
                non_zerop = 1;
            }
            *dst++ = xdigits[ptr[1] & 0x0F];
            if (i != 7)
                *dst++ = ':';
            ptr += 2;
        }
        *dst++ = '\0';
        return orig_dst;
    }

    const char *
        inet_ntop(int af, const void *src, char *dst, size_t size)
    {
        switch (af) {
        case AF_INET:
            return inet_ntop_v4(src, dst, size);
        case AF_INET6:
            return inet_ntop_v6(src, dst, size);
        default:
            errno = EAFNOSUPPORT;
            return NULL;
        }
    }

    /////////////////// inet_pton ///////////////////

       /*%
       35  * WARNING: Don't even consider trying to compile this on a system where
       36  * sizeof(int) < 4.  sizeof(int) > 4 is fine; all the world's not a VAX.
       37  */

    static int      inet_pton4(const char *src, u_char *dst);
    //static int      inet_pton6(const char *src, u_char *dst);

    /* int
      43  * inet_pton(af, src, dst)
      44  *      convert from presentation format (which usually means ASCII printable)
      45  *      to network format (which is usually some kind of binary format).
      46  * return:
      47  *      1 if the address was valid for the specified address family
      48  *      0 if the address wasn't valid (`dst' is untouched in this case)
      49  *      -1 if some other error occurred (`dst' is untouched in this case, too)
      50  * author:
      51  *      Paul Vixie, 1996.
      52  */
    int
        inet_pton(int af, const char *src, void *dst)
    {
        switch (af) {
        case AF_INET:
            return (inet_pton4(src, (u_char*)dst));
        case AF_INET6:
            return (inet_pton6(src, (u_char*)dst));
        default:
            return (-1);

        }
        /* NOTREACHED */
    }

    /* int
      68  * inet_pton4(src, dst)
      69  *      like inet_aton() but without all the hexadecimal and shorthand.
      70  * return:
      71  *      1 if `src' is a valid dotted quad, else 0.
      72  * notice:
      73  *      does not touch `dst' unless it's returning 1.
      74  * author:
      75  *      Paul Vixie, 1996.
      76  */
    static int
        inet_pton4(const char *src, u_char *dst)
    {
        static const char digits[] = "0123456789";
        int saw_digit, octets, ch;
#define NS_INADDRSZ     4
        u_char tmp[NS_INADDRSZ], *tp;

        saw_digit = 0;
        octets = 0;
        *(tp = tmp) = 0;
        while ((ch = *src++) != '\0') {
            const char *pch;

            if ((pch = strchr(digits, ch)) != NULL) {
                u_int newv = *tp * 10 + (pch - digits);

                if (saw_digit && *tp == 0)
                    return (0);
                if (newv > 255)
                    return (0);
                *tp = newv;
                if (!saw_digit) {
                    if (++octets > 4)
                        return (0);
                    saw_digit = 1;

                }

            }
            else if (ch == '.' && saw_digit) {
                if (octets == 4)
                    return (0);
                *++tp = 0;
                saw_digit = 0;

            }
            else
                return (0);

        }
        if (octets < 4)
            return (0);
        memcpy(dst, tmp, NS_INADDRSZ);
        return (1);
    }

};

int xxsocket::getinetpv(void)
{
    int flags = 0;
    /*char hostname[256] = { 0 };
    gethostname(hostname, sizeof(hostname));*/

    // ipv4 & ipv6
    addrinfo hint, *ailist = nullptr;
    memset(&hint, 0x0, sizeof(hint));
    hint.ai_flags = AI_PASSIVE;
    // hint.ai_family = AF_UNSPEC;
    // hint.ai_socktype = 0; // SOCK_STREAM;
    getaddrinfo(nullptr/*hostname*/, "", &hint, &ailist);

    if (ailist != nullptr) {
        for (auto aip = ailist; aip != NULL; aip = aip->ai_next)
        {
            switch (aip->ai_family) {
            case AF_INET:
                flags |= sip_ipv4;
                break;
            case AF_INET6:
                flags |= sip_ipv6;
                break;
            }
        }
        freeaddrinfo(ailist);
    }
    else {
        // gai_strerror(xxsocket::get_last_errno())
    }

    return flags;
}

int xxsocket::pconnect(const char* hostname, u_short port)
{
    auto ep = xxsocket::resolve(hostname, port);
    if (this->reopen(ep.af()))
    {
        return this->connect(ep);
    }
    return -1;
}

int xxsocket::pconnect_n(const char* hostname, u_short port, long timeout_sec)
{
    auto ep = xxsocket::resolve(hostname, port);
    if (this->reopen(ep.af()))
    {
        return this->connect_n(ep, timeout_sec);
    }
    return -1;
}

int xxsocket::pserv(const char* addr, u_short port)
{
    ip::endpoint local(addr, port);

    if (!this->reopen(local.af())) {
        return -1;
    }

    int n = this->bind(local);
    if (n != 0)
        return n;

    return this->listen();
}

ip::endpoint xxsocket::resolve(const char* hostname, unsigned short port)
{
    ip::endpoint ep;

    addrinfo hint;
    memset(&hint, 0x0, sizeof(hint));

    addrinfo* answer = nullptr;
    getaddrinfo(hostname, nullptr, &hint, &answer);

    if (answer == nullptr)
        return std::move(ep);

    memcpy(&ep, answer->ai_addr, answer->ai_addrlen);
    switch (answer->ai_family)
    {
    case AF_INET:
        ep.in4_.sin_port = htons(port);
        break;
    case AF_INET6:
        ep.in6_.sin6_port = htons(port);
        break;
    default:;
    }

    freeaddrinfo(answer);

    return std::move(ep);
}

ip::endpoint xxsocket::resolve_v6(const char* hostname, unsigned short port)
{
    ip::endpoint ep;

    struct addrinfo hint;
    memset(&hint, 0x0, sizeof(hint));
    hint.ai_family = AF_INET6;
    hint.ai_flags = AI_V4MAPPED;

    addrinfo* answer = nullptr;
    getaddrinfo(hostname, nullptr, &hint, &answer);

    if (answer == nullptr)
        return std::move(ep);

    memcpy(&ep, answer->ai_addr, answer->ai_addrlen);
    switch (answer->ai_family)
    {
    case AF_INET6:
        ep.in6_.sin6_port = htons(port);
        break;
    default:;
    }

    freeaddrinfo(answer);

    return std::move(ep);
}

xxsocket::xxsocket(void) : fd(bad_sock)
{
}

xxsocket::xxsocket(socket_native_type h) : fd(h)
{
}

xxsocket::xxsocket(xxsocket&& right) : fd(bad_sock)
{
    swap(right);
}

xxsocket& xxsocket::operator=(socket_native_type handle)
{
    if (!this->is_open()) {
        this->fd = handle;
    }
    return *this;
}

xxsocket& xxsocket::operator=(xxsocket&& right)
{
    return swap(right);
}

xxsocket::xxsocket(int af, int type, int protocol) : fd(bad_sock)
{
    open(af, type, protocol);
}

xxsocket::~xxsocket(void)
{
    close();
}

xxsocket& xxsocket::swap(xxsocket& who)
{
    // avoid fd missing
    if (!is_open()) {
        this->fd = who.fd;
        who.fd = bad_sock;
    }
    return *this;
}

bool xxsocket::open(int af, int type, int protocol)
{
    if (bad_sock == this->fd)
    {
        this->fd = ::socket(af, type, protocol);
    }
    return is_open();
}

bool xxsocket::reopen(int af, int type, int protocol)
{
    this->close();
    return this->open(af, type, protocol);
}

#if defined(_WIN32) && !defined(_WINSTORE)
bool xxsocket::open_ex(int af, int type, int protocol)
{
#if !defined(WP8)
    if (bad_sock == this->fd)
    {
        this->fd = ::WSASocket(af, type, protocol, nullptr, 0, WSA_FLAG_OVERLAPPED);

        DWORD dwBytes = 0;
        if (nullptr == __accept_ex)
        {
            GUID guidAcceptEx = WSAID_ACCEPTEX;
            (void)WSAIoctl(
                this->fd,
                SIO_GET_EXTENSION_FUNCTION_POINTER,
                &guidAcceptEx,
                sizeof(guidAcceptEx),
                &__accept_ex,
                sizeof(__accept_ex),
                &dwBytes,
                nullptr,
                nullptr);
        }

        if (nullptr == __get_accept_ex_sockaddrs)
        {
            GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;
            (void)WSAIoctl(
                this->fd,
                SIO_GET_EXTENSION_FUNCTION_POINTER,
                &guidGetAcceptExSockaddrs,
                sizeof(guidGetAcceptExSockaddrs),
                &__get_accept_ex_sockaddrs,
                sizeof(__get_accept_ex_sockaddrs),
                &dwBytes,
                nullptr,
                nullptr);
        }
    }
    return is_open();
#endif
    return false;
}

#if !defined(WP8)
bool xxsocket::accept_ex(
    __in SOCKET sockfd_listened,
    __in SOCKET sockfd_prepared,
    __in PVOID lpOutputBuffer,
    __in DWORD dwReceiveDataLength,
    __in DWORD dwLocalAddressLength,
    __in DWORD dwRemoteAddressLength,
    __out LPDWORD lpdwBytesReceived,
    __inout LPOVERLAPPED lpOverlapped)
{
    return __accept_ex(sockfd_listened,
        sockfd_prepared,
        lpOutputBuffer,
        dwReceiveDataLength,
        dwLocalAddressLength,
        dwRemoteAddressLength,
        lpdwBytesReceived,
        lpOverlapped) != FALSE;
}

void xxsocket::translate_sockaddrs(
    PVOID lpOutputBuffer,
    DWORD dwReceiveDataLength,
    DWORD dwLocalAddressLength,
    DWORD dwRemoteAddressLength,
    sockaddr **LocalSockaddr,
    LPINT LocalSockaddrLength,
    sockaddr **RemoteSockaddr,
    LPINT RemoteSockaddrLength)
{
    __get_accept_ex_sockaddrs(
        lpOutputBuffer,
        dwReceiveDataLength,
        dwLocalAddressLength,
        dwRemoteAddressLength,
        LocalSockaddr,
        LocalSockaddrLength,
        RemoteSockaddr,
        RemoteSockaddrLength);
}
#endif

#endif

bool xxsocket::is_open(void) const
{
    return this->fd != bad_sock;
}

socket_native_type xxsocket::release(void)
{
    socket_native_type result = this->fd;
    this->fd = bad_sock;
    return result;
}

socket_native_type xxsocket::native_handle(void) const
{
    return this->fd;
}

int xxsocket::set_nonblocking(bool nonblocking) const
{
    return set_nonblocking(this->fd, nonblocking);
}

int xxsocket::set_nonblocking(socket_native_type s, bool nonblocking)
{
    u_long argp = nonblocking;
    return ::ioctlsocket(s, FIONBIO, &argp);
}

int xxsocket::bind(const char* addr, unsigned short port) const
{
    ip::endpoint local(addr, port);

    return ::bind(this->fd, &local.intri_, sizeof(local));
}

int xxsocket::bind(const ip::endpoint& endpoint)
{
    return ::bind(this->fd, &endpoint.intri_, sizeof(endpoint));
}

int xxsocket::listen(int backlog) const
{
    return ::listen(this->fd, backlog);
}

xxsocket xxsocket::accept(socklen_t)
{
    return ::accept(this->fd, nullptr, nullptr);
}

xxsocket xxsocket::accept_n(timeval* timeout)
{
    xxsocket result;

    set_nonblocking(true);

    fd_set fds_rd;
    FD_ZERO(&fds_rd);
    FD_SET(this->fd, &fds_rd);

    if (::select(1, &fds_rd, 0, 0, timeout) > 0)
    {
        result = this->accept();
    }

    set_nonblocking(false);

    return result;
}

int xxsocket::connect(const char* addr, u_short port)
{
    return connect(ip::endpoint(addr, port));
}

int xxsocket::connect(const ip::endpoint& ep)
{
    return xxsocket::connect(fd, ep);
}

int xxsocket::connect(socket_native_type s, const char* addr, u_short port)
{
    ip::endpoint peer(addr, port);

    return xxsocket::connect(s, peer);
}

int xxsocket::connect(socket_native_type s, const ip::endpoint& ep)
{
    return ::connect(s, &ep.intri_, ep.af() == AF_INET ? sizeof(ep.in4_) : sizeof(ep.in6_));
}

int xxsocket::connect_n(const char* addr, u_short port, long timeout_sec)
{
    auto timeout = make_tv(timeout_sec);
    return connect_n(addr, port, &timeout);
}

int xxsocket::connect_n(const ip::endpoint& ep, long timeout_sec)
{
    auto timeout = make_tv(timeout_sec);
    return connect_n(ep, &timeout);
}

int xxsocket::connect_n(const char* addr, u_short port, timeval* timeout)
{
    return connect_n(ip::endpoint(addr, port), timeout);
}

int xxsocket::connect_n(const ip::endpoint& ep, timeval* timeout)
{
    if (xxsocket::connect_n(this->fd, ep, timeout) != 0) {
        this->fd = bad_sock;
        return -1;
    }
    return 0;
}

int xxsocket::connect_n(socket_native_type s, const char* addr, u_short port, timeval* timeout)
{
    return connect_n(s, ip::endpoint(addr, port), timeout);
}

int xxsocket::connect_n(socket_native_type s, const ip::endpoint& ep, timeval* timeout)
{
    fd_set rset, wset;
    int n, error = 0;
#ifdef _WIN32
    set_nonblocking(s, true);
#else
    int flags = ::fcntl(s, F_GETFL, 0);
    ::fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
    if ((n = xxsocket::connect(s, ep)) < 0) {
        error = xxsocket::get_last_errno();
        if (error != EINPROGRESS && error != EWOULDBLOCK)
            return -1;
    }

    /* Do whatever we want while the connect is taking place. */
    if (n == 0)
        goto done; /* connect completed immediately */

    FD_ZERO(&rset);
    FD_SET(s, &rset);
    wset = rset;

    if ((n = ::select(s + 1, &rset, &wset, NULL, timeout)) == 0) {
        ::closesocket(s);  /* timeout */
        xxsocket::set_last_errno(ETIMEDOUT);
        return (-1);
    }

    if (FD_ISSET(s, &rset) || FD_ISSET(s, &wset)) {
        socklen_t len = sizeof(error);
        if (::getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&error, &len) < 0)
            return (-1);  /* Solaris pending error */
    }
    else
        return -1;
done:
#ifdef _MSC_VER
    set_nonblocking(s, false);
#else
    ::fcntl(s, F_SETFL, flags);  /* restore file status flags */
#endif

    if (error != 0) {
        ::closesocket(s); /* just in case */
        xxsocket::set_last_errno(error);
        return (-1);
    }

    return (0);
}

int xxsocket::send(const void* buf, int len, int flags) const
{
    int bytes_transferred = 0;
    int n = 0;
    do
    {
        bytes_transferred +=
            (n = ::send(this->fd,
            (char*)buf + bytes_transferred,
                len - bytes_transferred,
                flags
            ));
    } while (bytes_transferred < len && n > 0);
    return bytes_transferred;
}

int xxsocket::send_n(const void* buf, int len, long timeout_sec, int flags)
{
    auto timeout = make_tv(timeout_sec);
    return send_n(this->fd, buf, len, &timeout, flags);
}

int xxsocket::send_n(const void* buf, int len, timeval* timeout, int flags)
{
    return xxsocket::send_n(this->fd, buf, len, timeout, flags);
}

int xxsocket::send_n(socket_native_type s, const void* buf, int len, timeval* timeout, int flags)
{
    int bytes_transferred;
    int n;
    int errcode = 0;
    int send_times = 0;

    for (bytes_transferred = 0;
        bytes_transferred < len;
        bytes_transferred += n)
    {
        // Try to transfer as much of the remaining data as possible.
        // Since the socket is in non-blocking mode, this call will not
        // block.
        n = xxsocket::send_i(s,
            (char *)buf + bytes_transferred,
            len - bytes_transferred,
            flags);
        //++send_times;
        // Check for errors.
        if (n <= 0)
        {
            // Check for possible blocking.
#ifdef _WIN32
            errcode = WSAGetLastError();
#else
            errcode = errno;
#endif
            if (n == -1 &&
                (errcode == EAGAIN || errcode == EWOULDBLOCK || errcode == ENOBUFS || errcode == EINTR))
            {
                // Wait upto <timeout> for the blocking to subside.
                int const rtn = handle_write_ready(s, timeout);

                // Did select() succeed?
                if (rtn != -1)
                {
                    // Blocking subsided in <timeout> period.  Continue
                    // data transfer.
                    n = 0;
                    continue;
                }
            }

            // Wait in select() timed out or other data transfer or
            // select() failures.
            return n;
        }
    }

    return bytes_transferred;
}

int xxsocket::recv(void* buf, int len, int flags) const
{
    int bytes_transfrred = 0;
    int n = 0;
    do
    {
        bytes_transfrred +=
            (n = ::recv(this->fd,
            (char*)buf + bytes_transfrred,
                len - bytes_transfrred,
                flags
            ));

    } while (bytes_transfrred < len && n > 0);
    return bytes_transfrred;
}

int xxsocket::recv_n(void* buf, int len, long timeout_sec, int flags) const
{
    auto timeout = make_tv(timeout_sec);
    return recv_n(this->fd, buf, len, &timeout, flags);
}

int xxsocket::recv_n(void* buf, int len, timeval* timeout, int flags) const
{
    return recv_n(this->fd, buf, len, timeout, flags);
}

int xxsocket::recv_n(socket_native_type s, void* buf, int len, timeval* timeout, int flags)
{
    int bytes_transferred;
    int n;
    int ec = 0;

    for (bytes_transferred = 0;
        bytes_transferred < len;
        bytes_transferred += n)
    {
        // Try to transfer as much of the remaining data as possible.
        // Since the socket is in non-blocking mode, this call will not
        // block.
        n = recv_i(s,
            static_cast <char *> (buf) + bytes_transferred,
            len - bytes_transferred,
            flags);

        // Check for errors.
        if (n <= 0)
        {
            // Check for possible blocking.
#ifdef _WIN32
            ec = WSAGetLastError();
#else
            ec = errno; // socket errno
#endif
            if (n == -1 &&
                (ec == EAGAIN
                    || ec == EINTR
                    || ec == EWOULDBLOCK
                    || ec == EINPROGRESS))
            {
                // Wait upto <timeout> for the blocking to subside.
                int const rtn = handle_read_ready(s, timeout);

                // Did select() succeed?
                if (rtn != -1)
                {
                    // Blocking subsided in <timeout> period.  Continue
                    // data transfer.
                    n = 0;
                    continue;
                }
            }

            // Wait in select() timed out or other data transfer or
            // select() failures.
            return n;
        }
    }

    return bytes_transferred;
}

bool xxsocket::read_until(std::string& buffer, const char delim)
{
    return read_until(buffer, &delim, sizeof(delim));
}

bool xxsocket::read_until(std::string& buffer, const std::string& delims)
{
    return read_until(buffer, delims.c_str(), delims.size());
}

bool xxsocket::read_until(std::string& buffer, const char* delims, int len)
{
    if (len == -1)
        len = strlen(delims);

    bool ok = false;
    char buf[128];
    int retry = 3; // retry three times
    int n = 0;
    for (; retry > 0; )
    {
        memset(buf, 0, sizeof(buf));
        n = recv_i(buf, sizeof(buf));
        if (n <= 0)
        {
            auto error = xxsocket::get_last_errno();
            if (n == -1 &&
                (error == EAGAIN
                    || error == EINTR
                    || error == EWOULDBLOCK
                    || error == EINPROGRESS))
            {
                timeval tv = { 3, 500000 };
                int rtn = handle_read_ready(&tv);

                if (rtn != -1)
                { // read ready
                    continue;
                }
            }

            // read not ready, retry.
            --retry;
            continue;
        }

        buffer.append(buf, n);
        if (static_cast<int>(buffer.size()) >= len)
        {
            auto eof = &buffer[buffer.size() - len];
            if (0 == memcmp(eof, delims, len))
            {
                ok = true;
                break;
            }
        }
    }

    return ok;
}

int xxsocket::send_i(const void* buf, int len, int flags) const
{
    return ::send(
        this->fd,
        (const char*)buf,
        len,
        flags);
}

int xxsocket::send_i(socket_native_type s, const void* buf, int len, int flags)
{
    return ::send(s,
        (const char*)buf,
        len,
        flags);
}

int xxsocket::recv_i(void* buf, int len, int flags) const
{
    return recv_i(this->fd, buf, len, flags);
}

int xxsocket::recv_i(socket_native_type s, void* buf, int len, int flags)
{
    return ::recv(
        s,
        (char*)buf,
        len,
        flags);
}

int xxsocket::recvfrom_i(void* buf, int len, ip::endpoint& from, int flags) const
{
    socklen_t addrlen = sizeof(from);
    return ::recvfrom(this->fd,
        (char*)buf,
        len,
        flags,
        &from.intri_,
        &addrlen
    );
}

int xxsocket::sendto_i(const void* buf, int len, ip::endpoint& to, int flags) const
{
    return ::sendto(this->fd,
        (const char*)buf,
        len,
        flags,
        &to.intri_,
        sizeof(to)
    );
}


int xxsocket::handle_write_ready(timeval* timeo) const
{
    return handle_write_ready(this->fd, timeo);
}

int xxsocket::handle_write_ready(socket_native_type s, timeval* timeo)
{
    fd_set fds_wr;
    FD_ZERO(&fds_wr);
    FD_SET(s, &fds_wr);
    int ret = ::select(s + 1, nullptr, &fds_wr, nullptr, timeo);
    return ret;
}

int xxsocket::handle_connect_ready(socket_native_type s, timeval* timeo)
{
    fd_set fds_wr;
    FD_ZERO(&fds_wr);
    FD_SET(s, &fds_wr);

    if (::select(0, nullptr, &fds_wr, nullptr, timeo) > 0 && FD_ISSET(s, &fds_wr))
    { // connect successfully
        return 0;
    }
    return -1;
}

int xxsocket::handle_read_ready(timeval* timeo) const
{
    /*fd_set fds_rd;
    FD_ZERO(&fds_rd);
    FD_SET(this->fd, &fds_rd);
    int ret = ::select(this->fd + 1, &fds_rd, nullptr, nullptr, timeo);
    return ret;*/
    return handle_read_ready(this->fd, timeo);
}

int xxsocket::handle_read_ready(socket_native_type s, timeval* timeo)
{
    fd_set fds_rd;
    FD_ZERO(&fds_rd);
    FD_SET(s, &fds_rd);
    int ret = ::select(s + 1, &fds_rd, nullptr, nullptr, timeo);
    return ret;
}

ip::endpoint xxsocket::local_endpoint(void) const
{
    return local_endpoint(this->fd);
}

ip::endpoint xxsocket::local_endpoint(socket_native_type fd)
{
    ip::endpoint ep;
    socklen_t socklen = sizeof(ep);
    getsockname(fd, &ep.intri_, &socklen);
    return ep;
}

ip::endpoint xxsocket::peer_endpoint(void) const
{
    return peer_endpoint(this->fd);
}

ip::endpoint xxsocket::peer_endpoint(socket_native_type fd)
{
    ip::endpoint ep;
    socklen_t socklen = sizeof(ep);
    getpeername(fd, &ep.intri_, &socklen);
    return ep;
}

int xxsocket::set_keepalive(int flag, int idle, int interval, int probes)
{
    return set_keepalive(this->fd, flag, idle, interval, probes);
}

int xxsocket::set_keepalive(socket_native_type s, int flag, int idle, int interval, int probes)
{
#if defined(_WIN32) && !defined(WP8) && !defined(WINRT)
    tcp_keepalive buffer_in;
    buffer_in.onoff = flag;
    buffer_in.keepalivetime = idle;
    buffer_in.keepaliveinterval = interval;

    return WSAIoctl(s,
        SIO_KEEPALIVE_VALS,
        &buffer_in,
        sizeof(buffer_in),
        nullptr,
        0,
        (DWORD*)&probes,
        nullptr,
        nullptr);
#else
    int errcnt = 0;
    errcnt += set_optval(s, SOL_SOCKET, SO_KEEPALIVE, flag);
    //errcnt += set_optval(s, SOL_TCP, TCP_KEEPIDLE, idle);
    //errcnt += set_optval(s, SOL_TCP, TCP_KEEPINTVL, interval);
    //errcnt += set_optval(s, SOL_TCP, TCP_KEEPCNT, probes);
    return errcnt;
#endif
}

//int xxsocket::ioctl(long cmd, u_long* argp) const
//{
//    return ::ioctlsocket(this->fd, cmd, argp);
//}

xxsocket::operator socket_native_type(void) const
{
    return this->fd;
}

bool xxsocket::alive(void) const
{
    return this->send_i("", 0) != -1;
}

int xxsocket::shutdown(int how) const
{
    return ::shutdown(this->fd, how);
}

void xxsocket::close(void)
{
    if (is_open())
    {
        ::closesocket(this->fd);
        this->fd = bad_sock;
    }
}

#if defined(_WINSTORE) || defined(WINRT)
#undef _naked_mark
#define _naked_mark
#endif
void _naked_mark xxsocket::init_ws32_lib(void)
{
#if defined(_WIN32) && !defined(_WIN64) && !defined(WINRT)
    _asm ret;
#else
    return;
#endif
}

int xxsocket::get_last_errno(void)
{
#if defined(_WIN32)
    return ::WSAGetLastError();
#else
    return errno;
#endif
}

void xxsocket::set_last_errno(int error)
{
#if defined(_WIN32)
    ::WSASetLastError(error);
#else
    errno = error;
#endif
}

const char* xxsocket::get_error_msg(int error)
{
#if defined(_MSC_VER) && (WINAPI_FAMILY_APP == WINAPI_FAMILY_DESKTOP_APP)
    static char error_msg[256];
    /*LPVOID lpMsgBuf = nullptr;*/
    ::FormatMessageA(
        /*FORMAT_MESSAGE_ALLOCATE_BUFFER |*/
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), // english language
        error_msg,
        sizeof(error_msg),
        NULL
    );

    /*if (lpMsgBuf != nullptr) {
        strcpy(error_msg, (const char*)lpMsgBuf);
        ::LocalFree(lpMsgBuf);
    }*/
    return error_msg;
#else
    return strerror(error);
#endif
}

// initialize win32 socket library
#if defined(_WIN32 ) && !defined(_WINSTORE)
LPFN_ACCEPTEX __accept_ex = nullptr;
LPFN_GETACCEPTEXSOCKADDRS __get_accept_ex_sockaddrs = nullptr;
#endif

#ifdef _WIN32
namespace {

    struct ws2_32_gc
    {
        ws2_32_gc(void)
        {
            WSADATA dat;
            WSAStartup(0x0202, &dat);
        }
        ~ws2_32_gc(void)
        {
            WSACleanup();
        }
    };

    ws2_32_gc __ws32_lib_gc;
};
#endif

/* select usage:
char dat;
fd_set fds_rd;
FD_ZERO(&fds_rd);
FD_SET(fd, &fds_rd);
timeval timeo;
timeo.sec = 5;
timeo.usec = 500000;
switch( ::select(fd + 1, &fds_rd, nullptr, nullptr, &timeo) )
{
case -1:  // select error
break;
case 0:   // select timeout
break;
default:  // can read
if(sock.recv_i(&dat, sizeof(dat), MSG_PEEK) < 0)
{
return -1;
}
;
}
*/

