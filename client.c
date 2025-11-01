#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>
#include "lab3.h"

void invertion(char* str, int length) {
    char* start = str;
    char* end = str + length - 1;
    while (start < end) {
        char temp = *start;
        *start = *end;
        *end = temp;
        start++;
        end--;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        const char msg[] = "error\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        _exit(EXIT_FAILURE);
    }

    const char *shm_name = argv[1];
    const char *sem_name = argv[2];
    const char *output_sem_name = argv[3];
    int client_id = atoi(argv[4]);
    const char *filename = argv[5];

    int file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (file == -1) {
        const char msg[] = "error: can't open the file\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        exit(EXIT_FAILURE);
    }
    int shm_fd = shm_open(shm_name, O_RDWR, 0);
    if (shm_fd == -1) {
		const char msg[] = "error: failed to open SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}
    shared_data_t *shm_data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_data == MAP_FAILED) {
		const char msg[] = "error: failed to map SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}
    sem_t *data_sem = sem_open(sem_name, O_RDWR);
    if (data_sem == SEM_FAILED) {
		const char msg[] = "error: failed to create semaphore\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}
    sem_t *output_sem = sem_open(output_sem_name, O_RDWR);
    if (output_sem == SEM_FAILED) {
		const char msg[] = "error: failed to create semaphore\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}

    bool running = true;
    while (running) {
        sem_wait(data_sem);

        if (shm_data->server_finished) {
            bool all_processed = true;
            for (int i = 0; i < shm_data->count; i++) {
                if (!shm_data->strings[i].processed) {
                    all_processed = false;
                    break;
                }
            }
            if (all_processed) {
                running = false;
                sem_post(data_sem);
                break;
            }
        }

        bool found = false;
        for (int i = 0; i < shm_data->count; i++) {
            string_data_t *str_data = &shm_data->strings[i];
            
            if (!str_data->processed) {
                int len = str_data->length;
                
                if ((client_id == 1 && len <= 10) || (client_id == 2 && len > 10)) {
                    char local_buf[MAX_STRING_SIZE];
                    memcpy(local_buf, str_data->text, len);
                    invertion(local_buf, len);
                    str_data->processed = true;
                    found = true;

                    write(file, local_buf, len);
                    write(file, "\n", 1);

                    sem_wait(output_sem);
                    const char prefix[] = "Result: ";
                    write(STDOUT_FILENO, prefix, sizeof(prefix) - 1);
                    write(STDOUT_FILENO, local_buf, len);
                    write(STDOUT_FILENO, "\n", 1);
                    sem_post(output_sem);
                    
                    break;
                }
            }
        }

        sem_post(data_sem);
        
        if (!found) {
            usleep(100000);
        }
    }

    close(file);
    sem_close(data_sem);
    sem_close(output_sem);
    munmap(shm_data, sizeof(shared_data_t));
    close(shm_fd);

    return 0;
}