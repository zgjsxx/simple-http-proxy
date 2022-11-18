#include <srs_protocol_async_dns.hpp>
#include <srs_kernel_log.hpp>
#include <srs_protocol_st.hpp>

void srs_sock_state_cb(void* data, int fd, int readable, int writable)
{
    srs_trace("fd = %d", fd);
    SrsAsyncDns* pDns = (SrsAsyncDns*)data;
    if(readable == 0 && writable == 0)
    {
        pDns->pds.fd = 0;
        srs_trace("no event, dns look up is finish");
        return;
    }
    pDns->pds.fd = fd;
    pDns->pds.events = 0;
    if(readable != 0)
    {
        srs_trace("wait readable");
        pDns->pds.events |= POLLIN;
    }
    if(writable != 0)
    {
        srs_trace("wait writable");
        pDns->pds.events |= POLLOUT;
    }
}

void dns_callback (void* arg, int status, int timeouts, struct hostent* hptr)
{
    srs_trace("dns call back");
    struct sockaddr_in *server_addr = (struct sockaddr_in *)arg;

    if(status == ARES_SUCCESS)
    {
        srs_trace("lookup success");
		for(char **pptr = hptr->h_addr_list; *pptr != NULL; pptr++)
        {         
            char ipv4_str[32] = {0};
            inet_ntop(AF_INET, *pptr, ipv4_str, sizeof(ipv4_str)); 
            srs_trace("server ip address = %s",ipv4_str);
            server_addr->sin_family = AF_INET;
            memcpy(&server_addr->sin_addr.s_addr, hptr->h_addr, hptr->h_length);
            // lookup_success = true;
            break;
        } 
    }
    else
    {
        // lookup_success = false;
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
	// A basic init call will read configuration from /etc/resolv.conf
    if((res = ares_init(&channel)) != ARES_SUCCESS) { 
        // std::cout << "ares feiled: " << res << std::endl;
        return;
    }

	if (ARES_SUCCESS != res)
	{
		return;
	}

	{
		struct ares_addr_node* servers = NULL;
		ares_get_servers(channel, &servers);

		struct ares_addr_node* tmpNode = NULL;
		tmpNode = servers;
		while (tmpNode)
		{
			if (tmpNode->family == AF_INET)
            {
                char ipv4_str[32] = {0};
                inet_ntop(AF_INET, &tmpNode->addr.addr4.s_addr, ipv4_str, sizeof(ipv4_str)); 
                srs_trace("server ip address = %s",ipv4_str);   
                srs_trace("dns server: %s", ipv4_str);
                dns_server_ip = ipv4_str;            
            }

			tmpNode = tmpNode->next;
		}

		ares_free_data(servers);
	}
    pds.fd = 0;
    lookup_success = false;
}

void SrsAsyncDns::do_resolve(string hostname, int port, struct sockaddr_in* server_addr)
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

    srs_trace("ares_init_options suc");
	//set dns server
    ares_set_servers_csv(theChannel, dns_server_ip.c_str());
    //get host by name
    ares_gethostbyname(theChannel, hostname.c_str(), AF_INET, dns_callback, server_addr);
 
    while (true) 
    {  
        if(pds.fd == 0)
        {
            srs_trace("fd = 0, lookup finish");
            break;
        }
        pds.revents = 0;
        srs_trace("fd = %d", pds.fd);
		if (srs_poll(&pds, 1, 1 * SRS_UTIME_SECONDS) <= 0)
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

bool SrsAsyncDns::getLookupResult()
{
    return lookup_success;
}