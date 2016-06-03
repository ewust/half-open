

half-open: half-open.c
	$(CC) $^ -o $@ -O3 -lssl -lcrypto -levent -levent_extra -levent_openssl
