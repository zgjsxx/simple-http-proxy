#include <srs_protocol_async_dns.hpp>
#include <srs_kernel_log.hpp>
#include <srs_protocol_st.hpp>

void srs_sock_state_cb(void* data, int fd, int readable, int writable)
{
    srs_trace("fd = %d", fd);
    SrsAsyncDns* pDns = (SrsAsyncDns*)data;
    if(readable == 0 && writable == 0)
    {
        srs_trace("no event");
    }
    pDns->pds.fd = fd;
    pDns->pds.events = 0;
    if(readable != 0)
    {
        srs_trace("readable");
        pDns->pds.events |= POLLIN;
    }
    if(writable != 0)
    {
        srs_trace("writable");
        pDns->pds.events |= POLLOUT;
    }
}

void dns_callback (void* arg, int status, int timeouts, struct hostent* hptr)
{
    IpList *ips = (IpList*)arg;
	if( ips == NULL ) 
    {
        return;
    }
    if(status == ARES_SUCCESS)
    {
		strncpy(ips->host, hptr->h_name, sizeof(ips->host));
        char **pptr = hptr->h_addr_list;
		for(int i = 0; *pptr != NULL && i < 10; pptr++, ++i)
        {
            inet_ntop(hptr->h_addrtype, *pptr, ips->ip[ips->count++], IP_LEN);
        } 
    }
    else
    {
        srs_trace("lookup failed");
    }
}

SrsAsyncDns::SrsAsyncDns()
{
    init();
}

SrsAsyncDns::~SrsAsyncDns()
{

}

void SrsAsyncDns::init()
{
    int res;
    if((res = ares_init(&channel)) != ARES_SUCCESS) {     // ares 对channel 进行初始化
        // std::cout << "ares feiled: " << res << std::endl;
        return;
    }
    pds.fd = 0;
    srs_trace("ares suc");
}

void SrsAsyncDns::do_resolve(string hostname)
{
    srs_trace("begin to look up %s", hostname.c_str());
	ares_channel theChannel = NULL;
    if (ARES_SUCCESS != ares_dup(&theChannel, channel))
    {
        return;
    }
    srs_trace("ares dup suc");

	ares_options op;
	int optmask;

	ares_save_options(theChannel, &op, &optmask);

	op.sock_state_cb = (ares_sock_state_cb)&srs_sock_state_cb;
	op.sock_state_cb_data = this;
	op.timeout = 2000; //ms
	op.tries = 2; //retry time
	optmask |= ARES_OPT_TIMEOUT|ARES_OPT_TRIES|ARES_OPT_SOCK_STATE_CB;

	ares_destroy(theChannel);

	ares_init_options(&theChannel, &op, optmask);
	ares_destroy_options(&op);

	// ares_destroy_options(&op);
    srs_trace("ares_init_options suc");
	// //set dns server
    ares_set_servers_csv(theChannel, "114.114.114.114");
    //get host by name
    IpList iplist;
    ares_gethostbyname(theChannel, hostname.c_str(), AF_INET, dns_callback, &iplist);
 
    while (true) 
    {  
        if(pds.fd == 0)
        {
            srs_trace("fd = 0");
            srs_usleep(10000);
            continue;
        }
        pds.revents = 0;
        srs_trace("fd = %d", pds.fd);
		if (srs_poll(&pds, 1, SRS_UTIME_NO_TIMEOUT) <= 0)
		{
			srs_trace("st_poll error");
			break;
		}

		if (pds.revents & POLLIN)
		{
            srs_trace("client has read events");
            ares_process_fd(theChannel, pds.fd, ARES_SOCKET_BAD); 
		}

		if (pds.revents & POLLOUT)
		{
            srs_trace("server read events");
            ares_process_fd(theChannel, ARES_SOCKET_BAD, pds.fd);
		}

		if (pds.revents & POLLERR)
		{
            srs_trace("error events");
            ares_process_fd(theChannel, pds.fd, pds.fd);
		}
    }
    ares_destroy(theChannel);   
}