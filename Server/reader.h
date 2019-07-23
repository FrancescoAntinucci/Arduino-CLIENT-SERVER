

int open_serial_port(const char * device, uint32_t baud_rate);

void scrivi(uint8_t *buffer,FILE* file,size_t received);

ssize_t read_port(int fd,char* file, uint8_t * buffer, size_t size);

int set();
