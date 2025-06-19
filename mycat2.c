#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

// A helper function to write all bytes in a buffer.
// The write() system call is not guaranteed to write the number of bytes
// requested, so we need to loop until all bytes are written.
ssize_t write_all(int fd, const void *buf, size_t count) {
    size_t bytes_left = count;
    const char *ptr = buf;
    while (bytes_left > 0) {
        ssize_t bytes_written = write(fd, ptr, bytes_left);
        if (bytes_written == -1) {
            // If write fails, return -1 to indicate an error.
            return -1;
        }
        bytes_left -= bytes_written;
        ptr += bytes_written;
    }
    return count; // Return total bytes written on success.
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Determine the buffer size. We use the system's page size as a starting
    // point for an efficient buffer size.
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        perror("sysconf");
        // Fallback to a reasonable default if sysconf fails.
        page_size = 4096;
    }

    // Dynamically allocate memory for the buffer.
    char *buffer = malloc(page_size);
    if (buffer == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        free(buffer);
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_read;
    // Read from the file into the buffer until the end of the file is reached.
    while ((bytes_read = read(fd, buffer, page_size)) > 0) {
        // Write the entire buffer content to standard output.
        if (write_all(STDOUT_FILENO, buffer, bytes_read) == -1) {
            perror("write_all");
            free(buffer);
            close(fd);
            exit(EXIT_FAILURE);
        }
    }

    if (bytes_read == -1) {
        perror("read");
        free(buffer);
        close(fd);
        exit(EXIT_FAILURE);
    }

    // Clean up: free allocated memory and close the file.
    free(buffer);
    if (close(fd) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
} 