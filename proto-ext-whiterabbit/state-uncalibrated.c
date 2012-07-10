/*
 * Aurelio Colosimo for CERN, 2011 -- GNU LGPL v2.1 or later
 * Based on PTPd project v. 2.1.0 (see AUTHORS for details)
 */

#include <ppsi/ppsi.h>
#include "common-fun.h"

int pp_uncalibrated(struct pp_instance *ppi, unsigned char *pkt, int plen)
{
	int e = 0; /* error var, to check errors in msg handling */

	if (plen == 0)
		goto no_incoming_msg;

	switch (ppi->msg_tmp_header.messageType) {

	case PPM_ANNOUNCE:
		e = st_com_slave_handle_announce(ppi, pkt, plen);
		break;

	case PPM_SYNC:
		e = st_com_slave_handle_sync(ppi, pkt, plen);
		break;

	case PPM_FOLLOW_UP:
		e = st_com_slave_handle_followup(ppi, pkt, plen);
		break;

	default:
		/* disreguard, nothing to do */
		break;
	}

no_incoming_msg:
	if (e == 0)
		e = st_com_execute_slave(ppi, 0);

	if (e != 0)
		ppi->next_state = PPS_FAULTY;

	ppi->next_delay = PP_DEFAULT_NEXT_DELAY_MS;

	return 0;
}
