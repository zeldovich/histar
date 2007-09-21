#ifndef JOS_TEST_RAND_HH
#define JOS_TEST_RAND_HH

void	 x_init(const char *s);
uint64_t x_hash(uint64_t input1, uint64_t input2);
uint64_t x_rand(void);
uint64_t x_encrypt(uint64_t v);
uint64_t x_decrypt(uint64_t v);

#endif
