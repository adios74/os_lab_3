#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <semaphore.h>
#include "lab3.h"

const char SHM_NAME[] = "/lab3_shm";
const char SEM_NAME[] = "/lab3_sem";
const char SEM_OUTPUT_NAME[] = "/lab3_output_sem";


int main() {
    char fname1[256];
    char fname2[256];
    
    const char line1[] = "Enter filename for process №1: ";
    write(STDOUT_FILENO, line1, sizeof(line1) - 1);
    ssize_t length1 = read(STDIN_FILENO, fname1, sizeof(fname1) - 1);
    if (length1 > 0) {
        fname1[length1 - 1] = '\0';
    }
    
    const char line2[] = "Enter filename for process №2: ";
    write(STDOUT_FILENO, line2, sizeof(line2) - 1);
    ssize_t length2 = read(STDIN_FILENO, fname2, sizeof(fname2) - 1);
    if (length2 > 0) {
        fname2[length2 - 1] = '\0';
    }

    int shm_fd = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (shm_fd == -1 && errno != ENOENT) {
		const char msg[] = "error: failed to open SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}

    if (ftruncate(shm_fd, sizeof(shared_data_t)) == -1) {
        const char msg[] = "error: failed to resize SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
    }

    shared_data_t *shm_data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_data == MAP_FAILED) {
		const char msg[] = "error: failed to map SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}

    memset(shm_data, 0, sizeof(shared_data_t));

    sem_t *data_sem = sem_open(SEM_NAME, O_RDWR | O_CREAT | O_TRUNC, 0600, 1);
    if (data_sem == SEM_FAILED) {
		const char msg[] = "error: failed to create semaphore\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}
    sem_t *output_sem = sem_open(SEM_OUTPUT_NAME, O_RDWR | O_CREAT | O_TRUNC, 0600, 1);
    if (output_sem == SEM_FAILED) {
		const char msg[] = "error: failed to create semaphore\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}

    pid_t client1 = fork();
    if (client1 == 0) {
        execl("./client", "client", SHM_NAME, SEM_NAME, SEM_OUTPUT_NAME, "1", fname1, NULL);
        const char msg[] = "Error: failed to start client 1\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        _exit(EXIT_FAILURE);
    }

    pid_t client2 = fork();
    if (client2 == 0) {
        execl("./client", "client", SHM_NAME, SEM_NAME, SEM_OUTPUT_NAME, "2", fname2, NULL);
        const char msg[] = "Error: failed to start client 2\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        _exit(EXIT_FAILURE);
    }

    const char prompt[] = "Enter string: ";
    
    sem_wait(output_sem);
    write(STDOUT_FILENO, prompt, sizeof(prompt) - 1);
    sem_post(output_sem);

    while (true) {
        char buf[MAX_STRING_SIZE];
        ssize_t bytes = read(STDIN_FILENO, buf, sizeof(buf));

        if (bytes <= 0) {
            break;
        }

        if (buf[bytes - 1] == '\n') {
            buf[bytes - 1] = '\0';
            bytes--;
        }

        if (bytes == 0) {
            break;
        }

        sem_wait(data_sem);
        
        if (shm_data->count < MAX_STRINGS) {
            string_data_t *str_data = &shm_data->strings[shm_data->count];
            str_data->length = bytes;
            memcpy(str_data->text, buf, bytes);
            str_data->text[bytes] = '\0';
            str_data->processed = false;
            shm_data->count++;
        }
        
        sem_post(data_sem);

        int processed_count = 0;
        int max_wait = 50;
        
        while (processed_count < max_wait) {
            sem_wait(data_sem);
            bool all_processed = true;
            for (int i = 0; i < shm_data->count; i++) {
                if (!shm_data->strings[i].processed) {
                    all_processed = false;
                    break;
                }
            }
            
            sem_post(data_sem);
            
            if (all_processed) {
                break;
            }
            
            usleep(100000);
            processed_count++;
        }

        sem_wait(output_sem);
        write(STDOUT_FILENO, prompt, sizeof(prompt) - 1);
        sem_post(output_sem);
    }

    sem_wait(data_sem);
    shm_data->server_finished = true;
    sem_post(data_sem);

    waitpid(client1, NULL, 0);
    waitpid(client2, NULL, 0);

    sem_unlink(SEM_NAME);
    sem_close(data_sem);
    sem_unlink(SEM_OUTPUT_NAME);
    sem_close(output_sem);
    munmap(shm_data, sizeof(shared_data_t));
    shm_unlink(SHM_NAME);
    close(shm_fd);

    return 0;
}