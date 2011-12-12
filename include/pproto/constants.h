#ifndef __PTP_CONSTANTS_H__
#define __PTP_CONSTANTS_H__

/* implementation specific constants */
#define PP_DEFAULT_INBOUND_LATENCY		0	/* in nsec */
#define PP_DEFAULT_OUTBOUND_LATENCY		0	/* in nsec */
#define PP_DEFAULT_NO_RESET_CLOCK		0
#define PP_DEFAULT_DOMAIN_NUMBER		0
#define PP_DEFAULT_DELAY_MECHANISM		P2P
#define PP_DEFAULT_AP				10
#define PP_DEFAULT_AI				1000
#define PP_DEFAULT_DELAY_S			6
#define PP_DEFAULT_ANNOUNCE_INTERVAL		1	/* 0 in 802.1AS */
#define PP_DEFAULT_UTC_OFFSET			0
#define PP_DEFAULT_UTC_VALID			0
#define PP_DEFAULT_PDELAYREQ_INTERVAL		1	/* -4 in 802.1AS */
#define PP_DEFAULT_DELAYREQ_INTERVAL		3
#define PP_DEFAULT_SYNC_INTERVAL		0	/* -7 in 802.1AS */
#define PP_DEFAULT_SYNC_RECEIPT_TIMEOUT		3
#define PP_DEFAULT_ANNOUNCE_RECEIPT_TIMEOUT	6	/* 3 by default */
#define PP_DEFAULT_QUALIFICATION_TIMEOUT	2
#define PP_DEFAULT_FOREIGN_MASTER_TIME_WINDOW	4
#define PP_DEFAULT_FOREIGN_MASTER_THRESHOLD	2
#define PP_DEFAULT_CLOCK_CLASS			248
#define PP_DEFAULT_CLOCK_ACCURACY		0xFE
#define PP_DEFAULT_PRIORITY1			248
#define PP_DEFAULT_PRIORITY2			248
#define PP_DEFAULT_CLOCK_VARIANCE		-4000 /* To be determined in
						       * 802.1AS. We use the
						       * same value as in ptpdv1
						       */
#define PP_DEFAULT_MAX_FOREIGN_RECORDS		5
#define PP_DEFAULT_PARENTS_STATS		0

#define PP_TIMER_PDELAYREQ_INTERVAL 	0
#define PP_TIMER_DELAYREQ_INTERVAL 	1
#define PP_TIMER_SYNC_INTERVAL		2
#define PP_TIMER_ANNOUNCE_RECEIPT 	3
#define PP_TIMER_ANNOUNCE_INTERVAL 	4
#define PP_TIMER_ARRAY_SIZE		5

#define PP_TWO_STEP_FLAG		2
#define PP_VERSION_PTP			2

#define PP_HEADER_LENGTH		34
#define PP_ANNOUNCE_LENGTH		64
#define PP_SYNC_LENGTH			44
#define PP_FOLLOW_UP_LENGTH		44
#define PP_PDELAY_REQ_LENGTH		54
#define PP_DELAY_REQ_LENGTH		44
#define PP_DELAY_RESP_LENGTH		54
#define PP_PDELAY_RESP_LENGTH		54
#define PP_PDELAY_RESP_FOLLOW_UP_LENGTH	54
#define PP_MANAGEMENT_LENGTH		48

#endif /* __PTP_CONSTANTS_H__ */
