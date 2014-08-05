/*
 * Copyright (C) 2012 CERN (www.cern.ch)
 * Author: Aurelio Colosimo
 * Based on ptp-noposix project (see AUTHORS for details)
 *
 * Released to the public domain
 */

#include <ppsi/ppsi.h>
#include "wr-api.h"

int wr_resp_calib_req(struct pp_instance *ppi, unsigned char *pkt, int plen)
{
	struct wr_dsport *wrp = WR_DSPOR(ppi);
	int e = 0;
	MsgSignaling wrsig_msg;

	if (ppi->is_new_state) {
		if (wrp->otherNodeCalSendPattern) {
			wrp->ops->calib_pattern_enable(ppi, 0, 0, 0);
			pp_timeout_set(ppi, PP_TO_EXT_0,
			       wrp->otherNodeCalPeriod / 1000);
		}

	}

	if ((wrp->otherNodeCalSendPattern) &&
	    (pp_timeout_z(ppi, PP_TO_EXT_0))) {
		wrp->ops->calib_pattern_disable(ppi);
		wr_handshake_timeout(ppi);
		goto out;
	}

	if (plen == 0)
		goto out;

	if (ppi->received_ptp_header.messageType == PPM_SIGNALING) {

		msg_unpack_wrsig(ppi, pkt, &wrsig_msg,
			 &(wrp->msgTmpWrMessageID));

		if (wrp->msgTmpWrMessageID == CALIBRATED) {
			if (wrp->otherNodeCalSendPattern)
				wrp->ops->calib_pattern_disable(ppi);
			if (wrp->wrMode == WR_MASTER)
				ppi->next_state = WRS_WR_LINK_ON;
			else
				ppi->next_state = WRS_CALIBRATION;

		}
	}

out:
	ppi->next_delay = wrp->wrStateTimeout;
	return e;
}
