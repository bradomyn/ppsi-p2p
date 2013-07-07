/*
 * Alessandro Rubini for CERN, 2011 -- public domain
 * (Bathos code heavily based on bare code)
 */

/* Socket interface for bathos. Currently only enc28 device */
#include <bathos/bathos.h>
#include <bathos/enc28j60.h>
#include <arch/gpio.h>
#include <arch/spi.h>

#include <ppsi/ppsi.h>
#include "ptpdump.h"

/*
 * SPI configuration: we need to declare both the config and the
 * device since we have no malloc in this crappy OS.
 * Note: The hardware configuration is hardwired here.
 */
#define GPIO_CS			10
#define GPIO_OTHER_CS		 9 /* turn this off (high) */

static const struct spi_cfg spi_cfg = {
        .gpio_cs = GPIO_CS,
        .pol = 0,
        .phase = 0,
        .devn = 0,
};

static struct spi_dev spi_dev = {
        .cfg = &spi_cfg,
};

/* ENC28 configuration */
static const struct enc28_cfg enc28_cfg = {
        /* This is an IP address in my network */
        .ipaddr = {192, 168,16, 201},
        /* And this is in the private range */
        .macaddr = {0x02, 0x33, 0x44, 0x55, 0x66, 0x77},
};

static struct enc28_dev enc28_dev = {
        .spi_dev = &spi_dev,
        .cfg = &enc28_cfg,
};

static int enc28_init(void)
{
        /* My board has another device on SPI: disable its CS on other_cs */
        gpio_dir_af(GPIO_OTHER_CS, 1, 1, 0);
        gpio_set(GPIO_OTHER_CS, 1);
        /* This is our gpio 10 */
        gpio_dir_af(GPIO_CS, 1, 1, 0);
        gpio_set(GPIO_CS, 1);

        enc28_create(&enc28_dev);
        return 0;
}

static void enc28_exit(void)
{
        enc28_destroy(&enc28_dev);
}


/* To open a channel we open the enc28, and that's it */
static int bathos_open_ch(struct pp_instance *ppi, char *ifname)
{
	if (!ppi->ethernet_mode) {
		pp_printf("%s: can't init UDP channels yet\n", __func__);
		return -1;
	}
	pp_printf("%s: opening %s\n", __func__, ifname);

	enc28_init();

	memcpy(NP(ppi)->ch[PP_NP_GEN].addr, enc28_cfg.macaddr, 6);
	memcpy(NP(ppi)->ch[PP_NP_EVT].addr, enc28_cfg.macaddr, 6);

	NP(ppi)->ch[PP_NP_GEN].custom = &enc28_dev;
	NP(ppi)->ch[PP_NP_EVT].custom = &enc28_dev;

	return 0;
}

static int bathos_net_exit(struct pp_instance *ppi)
{
	if (NP(ppi)->ch[PP_NP_EVT].custom)
		enc28_exit();
	NP(ppi)->ch[PP_NP_EVT].custom = NULL;
	NP(ppi)->ch[PP_NP_GEN].custom = NULL;
	return 0;
}

/* This function must be able to be called twice, and clean-up internally */
static int bathos_net_init(struct pp_instance *ppi)
{

	bathos_net_exit(ppi);

	/* The buffer is inside ppi, but we need to set pointers and align */
	pp_prepare_pointers(ppi);

	if (!ppi->ethernet_mode) {
		pp_printf("%s: can't init UDP channels yet\n", __func__);
		return -1;
	}

	pp_diag(ppi, frames, 1, "%s: Ethernet mode\n", __func__);

	/* raw sockets implementation always use gen socket */
	return bathos_open_ch(ppi, ppi->iface_name);
}

static int bathos_net_recv(struct pp_instance *ppi, void *pkt, int len,
			    TimeInternal *t)
{
	int ret;

	ret = enc28_recv(NP(ppi)->ch[PP_NP_GEN].custom, pkt, len);
	if (t && ret > 0)
		ppi->t_ops->get(ppi, t);

	if (ret > 0 && pp_diag_allow(ppi, frames, 2))
		dump_1588pkt("recv: ", pkt, ret, t);
	return ret;
}

static int bathos_net_send(struct pp_instance *ppi, void *pkt, int len,
			    TimeInternal *t, int chtype, int use_pdelay_addr)
{
	struct ethhdr {
		unsigned char   h_dest[6];
		unsigned char   h_source[6];
		uint16_t        h_proto;
	} __attribute__((packed)) *hdr = pkt;
	int ret;

	hdr->h_proto = htons(ETH_P_1588);
	memcpy(hdr->h_dest, PP_MCAST_MACADDRESS, 6);
	/* raw socket implementation always uses gen socket */
	memcpy(hdr->h_source, NP(ppi)->ch[PP_NP_GEN].addr, 6);

	if (t)
		ppi->t_ops->get(ppi, t);

	ret = enc28_send(NP(ppi)->ch[chtype].custom, pkt, len);
	if (ret > 0 && pp_diag_allow(ppi, frames, 2))
		dump_1588pkt("send: ", pkt, len, t);
	return ret;
}

struct pp_network_operations bathos_net_ops = {
	.init = bathos_net_init,
	.exit = bathos_net_exit,
	.recv = bathos_net_recv,
	.send = bathos_net_send,
};
