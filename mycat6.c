#define _GNU_SOURCE // 为了使用 posix_fadvise，需要定义这个宏
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

// --- 常量定义 ---
// 这个缓冲区大小是通过实验得出的 128KB 最优值。
#define OPTIMAL_BUFFER_SIZE (128 * 1024)

// 辅助函数，确保将缓冲区内的所有字节写入文件描述符。
ssize_t write_all(int fd, const void *buf, size_t count) {
    size_t bytes_left = count;
    const char *ptr = buf;
    while (bytes_left > 0) {
        ssize_t bytes_written = write(fd, ptr, bytes_left);
        if (bytes_written == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        bytes_left -= bytes_written;
        ptr += bytes_written;
    }
    return count;
}

// 分配一个按内存页对齐的内存块。
void *align_alloc(size_t size) {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        page_size = 4096;
    }
    size_t alloc_size = size + page_size - 1 + sizeof(void *);
    void *p1 = malloc(alloc_size);
    if (p1 == NULL) return NULL;
    void *p2 = (void *)(((uintptr_t)p1 + sizeof(void *) + page_size - 1) & ~(page_size - 1));
    ((void **)p2)[-1] = p1;
    return p2;
}

// 释放由 align_alloc 分配的内存。
void align_free(void *ptr) {
    if (ptr == NULL) return;
    free(((void **)ptr)[-1]);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    const size_t buffer_size = OPTIMAL_BUFFER_SIZE;

    char *buffer = align_alloc(buffer_size);
    if (buffer == NULL) {
        perror("align_alloc");
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        align_free(buffer);
        exit(EXIT_FAILURE);
    }

    // --- 使用 fadvise 进行优化 ---
    // 我们在此处通知操作系统内核，我们打算从头到尾顺序读取整个文件。
    // `posix_fadvise` 的参数:
    // fd: 文件描述符
    // 0: offset, 从文件开始处
    // 0: len, 直到文件结尾
    // POSIX_FADV_SEQUENTIAL: 我们给内核的"建议"，告诉它我们将进行顺序读取。
    // 这使得内核可以采用更积极的预读策略 (readahead)，并及时释放不再需要的页面缓存。
    if (posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL) != 0) {
        // fadvise 失败通常不是一个致命错误，程序仍然可以继续运行，
        // 只是无法获得性能优化。因此这里我们只打印一个警告。
        perror("posix_fadvise");
    }

    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, buffer_size)) > 0) {
        if (write_all(STDOUT_FILENO, buffer, bytes_read) == -1) {
            perror("write_all");
            align_free(buffer);
            close(fd);
            exit(EXIT_FAILURE);
        }
    }

    if (bytes_read == -1) {
        perror("read");
        align_free(buffer);
        close(fd);
        exit(EXIT_FAILURE);
    }

    align_free(buffer);
    if (close(fd) == -1) {
        perror("close");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
} 