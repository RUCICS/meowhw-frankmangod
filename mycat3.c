#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// 辅助函数，确保将缓冲区内的所有字节写入文件描述符。
// write() 系统调用不保证一次能写入所有请求的字节，因此需要循环。
ssize_t write_all(int fd, const void *buf, size_t count) {
    size_t bytes_left = count;
    const char *ptr = buf;
    while (bytes_left > 0) {
        ssize_t bytes_written = write(fd, ptr, bytes_left);
        if (bytes_written == -1) {
            if (errno == EINTR) continue; // 如果被信号中断，则重试
            return -1; // 其他错误则返回-1
        }
        bytes_left -= bytes_written;
        ptr += bytes_written;
    }
    return count; // 成功则返回写入的总字节数
}

// 分配一个按内存页对齐的内存块。
void *align_alloc(size_t size) {
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        perror("sysconf");
        return NULL;
    }

    // 为了保证对齐，我们需要分配比请求的 size 更大的内存。
    // 额外空间用于：
    // 1. 存储原始 malloc 返回的指针（方便 free）。
    // 2. 保证有足够的空间来移动指针以实现对齐。
    size_t alloc_size = size + page_size - 1 + sizeof(void *);
    void *p1 = malloc(alloc_size);
    if (p1 == NULL) {
        return NULL;
    }

    // 计算对齐后的指针 p2。
    // 1. (char *)p1 + sizeof(void *): 在原始指针后留出空间来存储 p1 自身。
    // 2. + page_size - 1: 确保地址加上这个值后再进行屏蔽操作时，能"上取整"到最近的页边界。
    // 3. & ~(page_size - 1): 将地址的低位清零，使其对齐到 page_size 的倍数。
    void *p2 = (void *)(((uintptr_t)p1 + sizeof(void *) + page_size - 1) & ~(page_size - 1));

    // 在对齐后地址 p2 的正前方，存下原始的 malloc 指针 p1。
    // 我们将 p2 转换为 (void **)，然后访问它前面的一个位置 [-1]。
    ((void **)p2)[-1] = p1;

    // 返回对齐后的指针。
    return p2;
}

// 释放由 align_alloc 分配的内存。
void align_free(void *ptr) {
    if (ptr == NULL) return;
    // 从对齐指针 ptr 的正前方取回原始的 malloc 指针。
    void *p1 = ((void **)ptr)[-1];
    // 释放原始的内存块。
    free(p1);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1) {
        perror("sysconf");
        page_size = 4096; // 若获取失败，则使用一个常见默认值
    }

    // 使用我们自定义的对齐分配函数来创建缓冲区。
    char *buffer = align_alloc(page_size);
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
    while ((bytes_read = read(fd, buffer, page_size)) > 0) {
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