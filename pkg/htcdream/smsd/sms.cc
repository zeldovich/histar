#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>

#include "sms.h"

static unsigned int
hexbyte_to_int(const char *str)
{
	char buf[3];
	memcpy(buf, str, 2);	
	buf[2] = '\0';
	return (strtoul(buf, NULL, 16));
}

static char *
desmsify_message(const char *msg, int length)
{
	int len = strlen(msg);
	int maxchars;
	int i, j;
	char *data, *str;

	if ((len % 2) != 0)
		return (NULL);

	data = (char *)malloc(len / 2);
	if (data == NULL)
		return (NULL);

	// convert from hex string
	for (i = 0; i < len; i += 2)
		data[i/2] = hexbyte_to_int(&msg[i]);
	
	maxchars = ((len / 2) * 8) / 7;
	str = (char *)malloc(maxchars + 1);
	if (str == NULL) {
		free(str);
		return (NULL);
	}

	// decode the message
	for (i = j = 0; i < (len / 2) && j < length; i++) {
		assert(j < maxchars);

		switch (i % 7) {
		case 0:
			str[j++] = (data[i] & 0x7f);
			break;
		case 1:
			str[j++] = (((data[i - 1] & 0x80) >> 7) | ((data[i] & 0x3f) << 1));
			break;
		case 2:
			str[j++] = (((data[i - 1] & 0xc0) >> 6) | ((data[i] & 0x1f) << 2));
			break;
		case 3:
			str[j++] = (((data[i - 1] & 0xe0) >> 5) | ((data[i] & 0x0f) << 3));
			break;
		case 4:
			str[j++] = (((data[i - 1] & 0xf0) >> 4) | ((data[i] & 0x07) << 4));
			break;
		case 5:
			str[j++] = (((data[i - 1] & 0xf8) >> 3) | ((data[i] & 0x03) << 5));
			break;
		case 6:
			str[j++] = (((data[i - 1] & 0xfc) >> 2) | ((data[i] & 0x01) << 6));
			str[j++] = (data[i] & 0xfe) >> 1;
			break;
		default:
			assert(0);
		}
	} 
	str[j] = '\0';

	return (str);
}

static char *
smsify_message(const char *msg)
{
	int i, j;
	int len = strlen(msg);
	int bits = len * 7;
	int bytes = (bits + 7) / 8;
	char *data, *str;

	if (len == 0 || len > 160)
		return (NULL);

	data = (char *)malloc(bytes);
	if (data == NULL)
		return (NULL);

	str  = (char *)malloc((bytes * 2) + 1);
	if (str == NULL) {
		free(data);
		return (NULL);
	}

	memset(data, 0, bytes);

	// make sure it's all 7-bit
	for (i = 0; i < len; i++) {
		if (msg[i] & 0x80) {
			fprintf(stderr, "invalid character 0x%02x in message\n", msg[i]);
			return (NULL);
		}
	}

	// encode the message
	// i'm sure there's a more clever/succinct way of doing this, but this is simple
	for (i = j = 0; i < len; i++) {
		unsigned char next = (i == (len - 1)) ? 0 : msg[i + 1];

		assert(j < bytes);

		switch (i % 8) {
		case 0:
			data[j++] = (((msg[i] & 0x7f) >> 0) | (next & 0x01) << 7);
			break;
		case 1:
			data[j++] = (((msg[i] & 0x7e) >> 1) | (next & 0x03) << 6);
			break;
		case 2:
			data[j++] = (((msg[i] & 0x7c) >> 2) | (next & 0x07) << 5);
			break;
		case 3:
			data[j++] = (((msg[i] & 0x78) >> 3) | (next & 0x0f) << 4);
			break;
		case 4:
			data[j++] = (((msg[i] & 0x70) >> 4) | (next & 0x1f) << 3);
			break;
		case 5:
			data[j++] = (((msg[i] & 0x60) >> 5) | (next & 0x3f) << 2);
			break;
		case 6:
			data[j++] = (((msg[i] & 0x40) >> 6) | (next & 0x7f) << 1);
			break;
		case 7:
			// just let i increment
			break;
		default:
			assert(0);
		}
	}

	// convert to a hex string
	for (i = 0; i < bytes; i++) {
		sprintf(str + (i * 2), "%02X", data[i] & 0xff);
	}
	str[bytes * 2] = '\0';

	return (str);
}

static char *
desmsify_number(const char *num)
{
	int len = strlen(num);
	int i;
	char *str;

	str = (char *)malloc(len + 1);
	if (str == NULL)
		return (NULL);

	for (i = 0; i < len; i += 2) {
		str[i] = num[i + 1];
		str[i + 1] = num[i];
	}
	if (!isdigit((int)str[i - 1]))
		str[i - 1] = '\0';
	str[i] = '\0';

	return (str);
}

static char *
smsify_number(const char *num)
{
	int len = strlen(num);
	int i;
	char *str;

	str = (char *)malloc(len + 2);
	if (str == NULL)
		return (NULL);
	
	strcpy(str, num);
	if ((len % 2) != 0) {
		str[len++] = 'F';
		str[len] = '\0';
	}

	for (i = 0; i < len; i += 2) {
		char tmp = str[i];
		str[i] = str[i + 1];
		str[i + 1] = tmp;
	}

	return (str);
}

int
parse_sms_message(const char *sms, struct incoming_sms_message *ism)
{
	int len = strlen((const char *)sms);
	int i, octetoff;

	if (len < 2 || ism == NULL)
		return (-1);

	int smsc_length = hexbyte_to_int(sms);
	if (smsc_length == 7) {
		// XXX - parse out service center number
	}

	octetoff = 1 + smsc_length;
	if (len < 2 * (octetoff + 3))
		return (-1);

	int sms_deliver     = hexbyte_to_int(&sms[2 * (octetoff)]);
	int sms_addr_length = hexbyte_to_int(&sms[2 * (octetoff + 1)]);
	int sms_addr_type   = hexbyte_to_int(&sms[2 * (octetoff + 2)]);

	(void)sms_deliver;
	(void)sms_addr_type;

	octetoff += (3 + (sms_addr_length + 1)/2);
	if (len < 2 * (octetoff + 10))
		return (-1);
 
	if ((sms_addr_length % 2) != 0)
		sms_addr_length++;
	char *number = (char *)malloc(sms_addr_length + 1);
	strncpy(number, &sms[2 * (1 + smsc_length + 3)], sms_addr_length);
	number[sms_addr_length] = '\0';
	ism->sender = desmsify_number(number);
	free(number);

	ism->tp_pid = hexbyte_to_int(&sms[2 * (octetoff)]);
	ism->tp_dcs = hexbyte_to_int(&sms[2 * (octetoff + 1)]);

	octetoff += 2;
	for (i = 0; i < 7; i++) {
		ism->tp_scts[i] = hexbyte_to_int(&sms[2 * (octetoff + i)]);
	}

	octetoff += 7;
	ism->tp_udl = hexbyte_to_int(&sms[2 * (octetoff)]);

	const char *msg_begin = &sms[2 * (octetoff + 1)];
	ism->message = desmsify_message(msg_begin, ism->tp_udl);

	return (0);
}

char *
generate_sms_message(const char *number, const char *message, const char *smsc_optional)
{
	const int sms_max_len = 1024;	// guesswork
	char *sms_message = smsify_message(message);
	char *sms_number  = smsify_number(number);
	char *sms = (char *)malloc(sms_max_len);

	if (strlen(message) == 0 || strlen(number) == 0)
		return (NULL);

	if (sms_message == NULL || sms_number == NULL || sms == NULL)
		return (NULL);

	if (smsc_optional == NULL)
		smsc_optional = "";

	snprintf(sms, sms_max_len, "%s1100%02X91%s0000AA%02X%s",
	    smsc_optional, strlen(number), sms_number,
	    strlen(message), sms_message);
	sms[sms_max_len - 1] = '\0';

	free(sms_message);
	free(sms_number);

	return (sms);
}

#if 0
int
main()
{
	struct incoming_sms_message ism;

	printf("%s\n", generate_sms_message("16502848327", "hello, there", ""));

	parse_sms_message("07912160130300F4040B916105828423F700009070615113518A0CE8329BFD6681E8E8B2BC0C", &ism);
	printf("[%s] says [%s]\n", ism.sender, ism.message);

	parse_sms_message("07917283010010F5040BC87238880900F10000993092516195800AE8329BFD4697D9EC37", &ism);
	printf("[%s] says [%s]\n", ism.sender, ism.message);
	return (0);
}
#endif
