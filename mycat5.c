#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>

// --- 常量定义 ---
// 这个缓冲区大小是通过运行 task5_experiment.sh 脚本实验得出的。
// 该脚本使用 'dd' 命令测量了不同块大小下的 I/O 吞吐率。
// 实验结果表明，在当前系统上，当缓冲区大小超过 128KB 后，性能增益变得微乎其微。
// 因此，我们选择 128KB 作为一个固定的、最优的缓冲区大小，
// 以便高效地摊销系统调用的开销。
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
        page_size = 4096; // Fallback
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
    
    // 使用在实验中确定的最优缓冲区大小
    const size_t buffer_size = OPTIMAL_BUFFER_SIZE;

    // 使用我们自定义的对齐分配函数来创建缓冲区
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