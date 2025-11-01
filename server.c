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


void int_to_str(int num, char* buffer) {
    char temp[20];
    int i = 0;
    int j = 0;
    
    if (num == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    if (num < 0) {
        buffer[j++] = '-';
        num = -num;
    }
    while (num > 0) {
        temp[i++] = '0' + (num % 10);
        num /= 10;
    }
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    
    buffer[j] = '\0';
}


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

    pid_t pid = getpid();
    char shm_name[256];
    char sem_data_name[256];
    char sem_output_name[256];
    
    char pid_str[20];
    int_to_str(pid, pid_str);
    
    strcpy(shm_name, "/lab3_shm_");
    strcat(shm_name, pid_str);
    
    strcpy(sem_data_name, "/lab3_sem_data_");
    strcat(sem_data_name, pid_str);
    
    strcpy(sem_output_name, "/lab3_sem_output_");
    strcat(sem_output_name, pid_str);

    int shm_fd = shm_open(shm_name, O_RDWR | O_CREAT | O_TRUNC, 0600);
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

    sem_t *data_sem = sem_open(sem_data_name, O_RDWR | O_CREAT | O_TRUNC, 0600, 1);
    if (data_sem == SEM_FAILED) {
		const char msg[] = "error: failed to create semaphore\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}
    sem_t *output_sem = sem_open(sem_output_name, O_RDWR | O_CREAT | O_TRUNC, 0600, 1);
    if (output_sem == SEM_FAILED) {
		const char msg[] = "error: failed to create semaphore\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}

    pid_t client1 = fork();
    if (client1 == 0) {
        execl("./client", "client", shm_name, sem_data_name, sem_output_name, "1", fname1, NULL);
        const char msg[] = "error: failed to start client 1\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        _exit(EXIT_FAILURE);
    }

    pid_t client2 = fork();
    if (client2 == 0) {
        execl("./client", "client", shm_name, sem_data_name, sem_output_name, "2", fname2, NULL);
        const char msg[] = "error: failed to start client 2\n";
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

    sem_unlink(sem_data_name);
    sem_close(data_sem);
    sem_unlink(sem_output_name);
    sem_close(output_sem);
    munmap(shm_data, sizeof(shared_data_t));
    shm_unlink(shm_name);
    close(shm_fd);

    return 0;
}