#ifndef _SMS_H_
#define _SMS_H_

struct incoming_sms_message {
	char *sender;
	char *message;
	int   tp_pid;
	int   tp_dcs;
	int   tp_scts[7];
	int   tp_udl;
};

int parse_sms_message(const char *, struct incoming_sms_message *);
char *generate_sms_message(const char *, const char *, const char *);

#endif	/* !_SMS_H_ */
