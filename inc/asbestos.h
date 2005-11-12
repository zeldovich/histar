#ifndef ASBESTOS_H
#define ASBESTOS_H
#include <machine/atomic.h>

#define CHECK_LABEL_ERROR_DEBUG_LEVEL 0

// Types that the syscall takes.
#define REPLY		1

#define LOOKUP		2
#define LOOKUP_R	(LOOKUP | REPLY)
#define READ		4
#define READ_R		(READ | REPLY)
#define WRITE		6
#define WRITE_R		(WRITE | REPLY)
#define CONTROL         8
#define CONTROL_R       (CONTROL | REPLY)
#define SELECT          10
#define SELECT_R        (SELECT | REPLY)
#define PEEK		12
#define PEEK_R		(PEEK | REPLY)
#define DEAD		14

#define NOTIFY_DELIVER	16
#define NOTIFY_FAIL	18

#define SET_MAYBE	20

// maps X to X_R
#define REPLY_TYPE(X)   ((X) | REPLY)

#define RESULT_MAYBE 0
#define RESULT_SENT 1

//#define NON_BLOCKING_CALL (1<<31)

typedef struct vnode vnode_t;

typedef struct msgqueue {
	uint32_t mq_flags;
	handle_t mq_queueh;	// 0 to create new area
	size_t mq_size;
} msgqueue_t;

#define MQ_CLOSE_ON_FORK	0x10
#define MQ_NOTIFY_DEAD		0x20
#define MQ_NOTIFY_DELIVER	0x40
#define MQ_NOTIFY_FAIL		0x80


// Labels

typedef struct label {
	size_t size;
	level_t default_level;
	handle_t *handles;
} label_t;

typedef uint32_t msgtype_t;

typedef struct msg {       		//   sending	receiving
	handle_t m_dest;       		//	U	    K*
	void *m_dest_user_data;		//	-	    K
	msgtype_t m_type;      		//	U	    K*
	off_t m_code;          		//	U	    K
	int m_id;              		//	U	    K*
	handle_t m_reply;      		//	U	    K
	
	void *m_data;          		//	U	    K
	size_t m_len;          		//	U	    K
	size_t m_full_len;     		//	-	    K

	uint32_t m_uflags;     		//	-	    U
	void *m_buf;           		//	-	    U
	size_t m_capacity;     		//	-	    U
	
	label_t m_verify;     		//	-	    K
} msg_t;

#define M_LOCAL		0x02
#define M_TRUNC		0x04
#define M_MATCH_DEST	0x10
#define M_MATCH_TYPE	0x20
#define M_MATCH_ID	0x40
#define M_NO_VERIFY 0x80

typedef struct msglabel {
	label_t ml_cs;
	label_t ml_ds;
	label_t ml_v;
	label_t ml_dr;
#define ml_cr	ml_v
} msglabel_t;

#define	ML_CS		0x01
#define ML_DS		0x02
#define ML_V		0x04
#define ML_CR		0x04
#define ML_DR		0x08
#define ML_V_LOCAL	0x10 /* XXX not yet understood */
#define	ML_CS_DEST	0x20	// contaminate self's P_S by dest

// If M_MATCH_ID, then only the message with m_id may be delivered.
// Similarly for M_MATCH_DEST and M_MATCH_ID.
// If M_TRUNC, then m_len may be > m_capacity, but at most m_capacity
//   bytes are written into m_buf.  Otherwise, messages with
//   len > m_capacity are rejected.
// If M_MMAP, then the user has provided PGSIZE bytes of data starting at
//   the msg_t.  The kernel is allowed to use mmap() to transfer the msg_t.
// If M_LOCAL, then M_MMAP must be true.  The kernel may choose to put the
//   message data in free space following the msg_t, rather than in m_buf.
// No verify handles are reported unless M_MMAP is true.

#define MSG_INITR(u, buf, capacity)	do { msg_t *__u = (u); __u->m_uflags = M_TRUNC; __u->m_buf = (buf); __u->m_capacity = (capacity); } while (0)
#define MSG_INITS(u, data, len)		do { msg_t *__u = (u); __u->m_data = (void *) (data); __u->m_len = (len); } while (0)
#define MSGLABEL_CLEAR(ml)		do { msglabel_t *__ml = (ml); __ml->ml_cs.default_level = __ml->ml_dr.default_level = LSTAR; __ml->ml_ds.default_level = __ml->ml_v.default_level = LTHREE; __ml->ml_cs.size = __ml->ml_ds.size = __ml->ml_v.size = __ml->ml_dr.size = 0; } while (0)
#define MSGLABEL_INIT(ml)		do { *(ml) = empty_msglabel; } while (0)


#endif /* ASBESTOS_H */
