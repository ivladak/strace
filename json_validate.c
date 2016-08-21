#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>


#include "json.h"

enum EXIT_CODES {
	EXIT_OK,
	EXIT_PARSE_FAILURE,
	EXIT_OPEN_FAILURE,
	EXIT_SEEK_FAILURE,
	EXIT_MMAP_FAILURE,
};

int main(int argc, char *argv[])
{
	off_t size;
	char *addr;
	int fd;
	int i;
	const char *cur;
	const char *end;

	for (i = 1; i < argc; i++) {
		fd = open(argv[i], O_RDONLY);

		if (fd < 0) {
			perror("open");
			return EXIT_OPEN_FAILURE;
		}

		size = lseek(fd, 0, SEEK_END);

		if (size == (off_t)-1) {
			perror("lseek");
			return EXIT_SEEK_FAILURE;
		}

		addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE | MAP_POPULATE,
			fd, 0);

		if (addr == MAP_FAILED) {
			perror("mmap");
			return EXIT_MMAP_FAILURE;
		}

		cur = addr;
		end = addr + size;

		json_skip_space(&cur, end);

		while (cur < end) {
			if (!json_parse_object(&cur, end, NULL)) {
				fprintf(stderr, "JSON parsing failed on file "
					"\"%s\" on object starting from offset "
					"%zu\n", argv[i], (size_t)(cur - addr));

				return EXIT_PARSE_FAILURE;
			}

			json_skip_space(&cur, end);
		}

		munmap(addr, size);
	}

	return EXIT_OK;
}
