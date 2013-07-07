
#include <bathos/bathos.h>
#include <bathos/jiffies.h>
#include <bathos/io.h>

#include <ppsi/ppsi.h>

/* Again, allow env-based setup */
#ifndef PP_DIAG_VERBOSITY
#define PP_DIAG_VERBOSITY "0"
#endif

const int pp_diag_verbosity; /* not really used by now, to be removed */

static struct pp_globals	ppg_static;
static struct pp_instance	ppi_static;

/* ppi fields */
static DSDefault		defaultDS;
static DSCurrent		currentDS;
static DSParent			parentDS;
static DSPort			portDS;
static DSTimeProperties		timePropertiesDS;
static struct pp_servo		servo;


/* At boot, initialize the status for this task */
static int ppsi_init(void *status)
{
	struct pp_instance *ppi = status;
	struct pp_globals *ppg = &ppg_static;

	printf("%s: verbosity string: \"%s\"\n", __func__,
	       PP_DIAG_VERBOSITY);

	ppi->flags		= pp_diag_parse(PP_DIAG_VERBOSITY);

	ppi->glbs		= ppg;
	ppg->defaultDS		= &defaultDS;
	ppg->currentDS		= &currentDS;
	ppg->parentDS		= &parentDS;
	ppi->portDS		= &portDS;
	ppg->timePropertiesDS	= &timePropertiesDS;
	ppg->servo		= &servo;
	ppg->arch_data		= NULL;
	ppi->n_ops		= &DEFAULT_NET_OPS;
	ppi->t_ops		= &DEFAULT_TIME_OPS;

	ppi->ethernet_mode	= PP_DEFAULT_ETHERNET_MODE;
	NP(ppi)->ptp_offset	= 14 /*ETH_HLEN */;

	ppi->iface_name		= "eth0"; /* unused by bathos */
	ppg->rt_opts		= &default_rt_opts;
	ppi->is_new_state	= 1;
	ppi->state		= PPS_INITIALIZING;

	pp_open_globals(ppg);
        /* The actual sockets are opened in state-initializing */

	return 0;
}

/* This runs periodically */
static void *ppsi_job(void *arg)
{
	struct pp_instance *ppi = arg;
	struct ethhdr {
		unsigned char   h_dest[6];
		unsigned char   h_source[6];
		uint16_t        h_proto;
	} *hdr = ppi->rx_frame;
	int i, minsize = sizeof(*hdr), delay_ms;
	static unsigned long next_to; /* bad: not multi-task */

	/* If the network is not inited, don't even try to receive */
	if (NP(ppi)->ch[PP_NP_EVT].custom == NULL) {
		delay_ms = pp_state_machine(ppi, NULL, 0);
		goto out_calc;
	}

	/* We have no such thing as select(), so poll for a frame */
	do {
		i = ppi->n_ops->recv(ppi, ppi->rx_frame,
				     PP_MAX_FRAME_LENGTH - 4,
				     &ppi->last_rcv_time);
	} while ((i > minsize) && (ntohs(hdr->h_proto) != ETH_P_1588));

	/* If no frame and no timeout yet, return */
	if (i < minsize && next_to && time_before(jiffies, next_to))
	    return arg;

	if (i > minsize) /* State machine with a frame */
		delay_ms = pp_state_machine(ppi, ppi->rx_ptp,
					    i - NP(ppi)->ptp_offset);
	else /* State machine without a frame */
		delay_ms = pp_state_machine(ppi, NULL, 0);

out_calc:
	next_to = ppi->t_ops->calc_timeout(ppi, delay_ms);
	return arg;
}

static struct bathos_task __task t_pwm = {
	.name		= "ppsi",
	.period		= HZ / 10,
	.init		= ppsi_init,
	.job		= ppsi_job,
	.arg		= &ppi_static,
	.release	= 2 * HZ,
};

