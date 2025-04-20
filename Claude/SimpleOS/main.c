/*
 * SimpleOS - A minimal operating system implementation
 * 
 * This minimal OS includes:
 * - A basic kernel
 * - Simple memory management
 * - Process management
 * - File system operations
 * - Basic command shell
 */

 #include <stdint.h>
 #include <stdbool.h>
 
 /* ======= CONSTANTS ======= */
 #define MAX_PROCESSES 16
 #define MAX_FILES 32
 #define MAX_FILENAME_LEN 32
 #define MAX_PATH_LEN 128
 #define MEMORY_SIZE 65536 // 64KB total system memory
 #define PROCESS_MEMORY_SIZE 4096 // 4KB per process
 #define SHELL_BUFFER_SIZE 256
 
 /* ======= DATA STRUCTURES ======= */
 
 // Process states
 typedef enum {
     PROCESS_READY,
     PROCESS_RUNNING,
     PROCESS_BLOCKED,
     PROCESS_TERMINATED
 } ProcessState;
 
 // Process Control Block
 typedef struct {
     uint8_t id;
     ProcessState state;
     uint16_t memory_start;
     uint16_t memory_size;
     uint16_t program_counter;
     char name[32];
 } Process;
 
 // File system entry
 typedef struct {
     char filename[MAX_FILENAME_LEN];
     uint16_t start_block;
     uint16_t size;
     bool in_use;
 } FileEntry;
 
 // OS state
 typedef struct {
     // Memory
     uint8_t memory[MEMORY_SIZE];
     bool memory_map[MEMORY_SIZE / PROCESS_MEMORY_SIZE];
     
     // Process management
     Process processes[MAX_PROCESSES];
     uint8_t current_process;
     uint8_t process_count;
     
     // File system
     FileEntry file_table[MAX_FILES];
     
     // System state
     bool system_running;
 } OS;
 
 /* ======= GLOBAL VARIABLES ======= */
 OS simple_os;
 
 /* ======= MEMORY MANAGEMENT ======= */
 
 // Initialize memory
 void memory_init() {
     for (int i = 0; i < MEMORY_SIZE / PROCESS_MEMORY_SIZE; i++) {
         simple_os.memory_map[i] = false; // Mark all memory blocks as free
     }
 }
 
 // Allocate memory block for a process
 uint16_t memory_allocate() {
     for (int i = 0; i < MEMORY_SIZE / PROCESS_MEMORY_SIZE; i++) {
         if (!simple_os.memory_map[i]) {
             simple_os.memory_map[i] = true; // Mark as used
             return i * PROCESS_MEMORY_SIZE;
         }
     }
     return 0xFFFF; // No memory available
 }
 
 // Free memory used by a process
 void memory_free(uint16_t start_address) {
     uint16_t block = start_address / PROCESS_MEMORY_SIZE;
     if (block < MEMORY_SIZE / PROCESS_MEMORY_SIZE) {
         simple_os.memory_map[block] = false;
     }
 }
 
 /* ======= PROCESS MANAGEMENT ======= */
 
 // Initialize process management
 void process_init() {
     simple_os.process_count = 0;
     simple_os.current_process = 0;
     
     // Mark all processes as terminated initially
     for (int i = 0; i < MAX_PROCESSES; i++) {
         simple_os.processes[i].state = PROCESS_TERMINATED;
     }
 }
 
 // Create a new process
 uint8_t process_create(const char* name) {
     if (simple_os.process_count >= MAX_PROCESSES) {
         return 0xFF; // Error: max processes reached
     }
     
     // Find available process slot
     uint8_t pid = 0;
     while (pid < MAX_PROCESSES && simple_os.processes[pid].state != PROCESS_TERMINATED) {
         pid++;
     }
     
     if (pid >= MAX_PROCESSES) {
         return 0xFF; // Error: no free process slots
     }
     
     // Allocate memory for the process
     uint16_t mem_start = memory_allocate();
     if (mem_start == 0xFFFF) {
         return 0xFF; // Error: out of memory
     }
     
     // Setup process
     Process* p = &simple_os.processes[pid];
     p->id = pid;
     p->state = PROCESS_READY;
     p->memory_start = mem_start;
     p->memory_size = PROCESS_MEMORY_SIZE;
     p->program_counter = 0;
     
     // Copy process name
     int i = 0;
     while (name[i] != '\0' && i < 31) {
         p->name[i] = name[i];
         i++;
     }
     p->name[i] = '\0';
     
     simple_os.process_count++;
     return pid;
 }
 
 // Terminate a process
 void process_terminate(uint8_t pid) {
     if (pid >= MAX_PROCESSES || simple_os.processes[pid].state == PROCESS_TERMINATED) {
         return; // Invalid PID or already terminated
     }
     
     // Free memory
     memory_free(simple_os.processes[pid].memory_start);
     
     // Mark process as terminated
     simple_os.processes[pid].state = PROCESS_TERMINATED;
     simple_os.process_count--;
 }
 
 // Schedule next process to run
 void process_schedule() {
     if (simple_os.process_count == 0) {
         return; // No processes to schedule
     }
     
     // Simple round-robin scheduling
     uint8_t next_process = (simple_os.current_process + 1) % MAX_PROCESSES;
     while (next_process != simple_os.current_process) {
         if (simple_os.processes[next_process].state == PROCESS_READY) {
             // Set current process to ready if it was running
             if (simple_os.processes[simple_os.current_process].state == PROCESS_RUNNING) {
                 simple_os.processes[simple_os.current_process].state = PROCESS_READY;
             }
             
             // Set next process to running
             simple_os.current_process = next_process;
             simple_os.processes[next_process].state = PROCESS_RUNNING;
             return;
         }
         next_process = (next_process + 1) % MAX_PROCESSES;
     }
 }
 
 /* ======= FILE SYSTEM ======= */
 
 // Initialize file system
 void fs_init() {
     for (int i = 0; i < MAX_FILES; i++) {
         simple_os.file_table[i].in_use = false;
     }
 }
 
 // Create a new file
 int fs_create(const char* filename) {
     // Find free file entry
     int file_id = -1;
     for (int i = 0; i < MAX_FILES; i++) {
         if (!simple_os.file_table[i].in_use) {
             file_id = i;
             break;
         }
     }
     
     if (file_id == -1) {
         return -1; // No free file slots
     }
     
     // Check if filename already exists
     for (int i = 0; i < MAX_FILES; i++) {
         if (simple_os.file_table[i].in_use) {
             bool match = true;
             for (int j = 0; j < MAX_FILENAME_LEN; j++) {
                 if (simple_os.file_table[i].filename[j] != filename[j]) {
                     match = false;
                     break;
                 }
                 if (filename[j] == '\0') {
                     break;
                 }
             }
             if (match) {
                 return -2; // File already exists
             }
         }
     }
     
     // Create the file
     FileEntry* file = &simple_os.file_table[file_id];
     int i = 0;
     while (filename[i] != '\0' && i < MAX_FILENAME_LEN - 1) {
         file->filename[i] = filename[i];
         i++;
     }
     file->filename[i] = '\0';
     file->start_block = 0; // Allocate actual storage as needed
     file->size = 0;
     file->in_use = true;
     
     return file_id;
 }
 
 // Delete a file
 bool fs_delete(const char* filename) {
     // Find the file
     for (int i = 0; i < MAX_FILES; i++) {
         if (simple_os.file_table[i].in_use) {
             bool match = true;
             for (int j = 0; j < MAX_FILENAME_LEN; j++) {
                 if (simple_os.file_table[i].filename[j] != filename[j]) {
                     match = false;
                     break;
                 }
                 if (filename[j] == '\0') {
                     break;
                 }
             }
             if (match) {
                 simple_os.file_table[i].in_use = false;
                 return true;
             }
         }
     }
     return false; // File not found
 }
 
 /* ======= SHELL ======= */
 
 // Process a shell command
 void shell_process_command(const char* command) {
     // Compare with "help" command
     if (command[0] == 'h' && command[1] == 'e' && command[2] == 'l' && command[3] == 'p' && 
         (command[4] == '\0' || command[4] == ' ')) {
         printf("SimpleOS Commands:\n");
         printf("  help                 - Display this help message\n");
         printf("  ps                   - List all processes\n");
         printf("  run [program]        - Run a program\n");
         printf("  kill [pid]           - Terminate a process\n");
         printf("  ls                   - List all files\n");
         printf("  touch [filename]     - Create a new file\n");
         printf("  rm [filename]        - Delete a file\n");
         printf("  exit                 - Shut down the system\n");
         return;
     }
     
     // Compare with "ps" command
     if (command[0] == 'p' && command[1] == 's' && (command[2] == '\0' || command[2] == ' ')) {
         printf("PID  STATE     NAME\n");
         printf("---  --------  ----------------\n");
         for (int i = 0; i < MAX_PROCESSES; i++) {
             if (simple_os.processes[i].state != PROCESS_TERMINATED) {
                 const char* state_str = "UNKNOWN";
                 switch (simple_os.processes[i].state) {
                     case PROCESS_READY: state_str = "READY"; break;
                     case PROCESS_RUNNING: state_str = "RUNNING"; break;
                     case PROCESS_BLOCKED: state_str = "BLOCKED"; break;
                     case PROCESS_TERMINATED: state_str = "TERM"; break;
                 }
                 printf("%3d  %-8s  %s\n", i, state_str, simple_os.processes[i].name);
             }
         }
         return;
     }
     
     // Compare with "run" command
     if (command[0] == 'r' && command[1] == 'u' && command[2] == 'n' && command[3] == ' ') {
         const char* program_name = &command[4];
         uint8_t pid = process_create(program_name);
         if (pid != 0xFF) {
             printf("Started process %d: %s\n", pid, program_name);
         } else {
             printf("Failed to start process\n");
         }
         return;
     }
     
     // Compare with "kill" command
     if (command[0] == 'k' && command[1] == 'i' && command[2] == 'l' && command[3] == 'l' && command[4] == ' ') {
         int pid = 0;
         int i = 5;
         while (command[i] >= '0' && command[i] <= '9') {
             pid = pid * 10 + (command[i] - '0');
             i++;
         }
         if (pid >= 0 && pid < MAX_PROCESSES && simple_os.processes[pid].state != PROCESS_TERMINATED) {
             process_terminate(pid);
             printf("Terminated process %d\n", pid);
         } else {
             printf("Invalid process ID\n");
         }
         return;
     }
     
     // Compare with "ls" command
     if (command[0] == 'l' && command[1] == 's' && (command[2] == '\0' || command[2] == ' ')) {
         int file_count = 0;
         printf("FILES:\n");
         printf("---------------------\n");
         for (int i = 0; i < MAX_FILES; i++) {
             if (simple_os.file_table[i].in_use) {
                 printf("%s (%d bytes)\n", simple_os.file_table[i].filename, simple_os.file_table[i].size);
                 file_count++;
             }
         }
         if (file_count == 0) {
             printf("No files found\n");
         }
         return;
     }
     
     // Compare with "touch" command
     if (command[0] == 't' && command[1] == 'o' && command[2] == 'u' && command[3] == 'c' && 
         command[4] == 'h' && command[5] == ' ') {
         const char* filename = &command[6];
         int result = fs_create(filename);
         if (result >= 0) {
             printf("Created file: %s\n", filename);
         } else if (result == -1) {
             printf("Failed: File system full\n");
         } else {
             printf("Failed: File already exists\n");
         }
         return;
     }
     
     // Compare with "rm" command
     if (command[0] == 'r' && command[1] == 'm' && command[2] == ' ') {
         const char* filename = &command[3];
         if (fs_delete(filename)) {
             printf("Deleted file: %s\n", filename);
         } else {
             printf("Failed: File not found\n");
         }
         return;
     }
     
     // Compare with "exit" command
     if (command[0] == 'e' && command[1] == 'x' && command[2] == 'i' && command[3] == 't' && 
         (command[4] == '\0' || command[4] == ' ')) {
         printf("Shutting down SimpleOS...\n");
         simple_os.system_running = false;
         return;
     }
     
     // Command not recognized
     printf("Unknown command: %s\n", command);
     printf("Type 'help' for available commands\n");
 }
 
 // Run the shell
 void shell_run() {
     char input_buffer[SHELL_BUFFER_SIZE];
     
     printf("\nSimpleOS v0.1\n");
     printf("Type 'help' for available commands\n\n");
     
     while (simple_os.system_running) {
         printf("SimpleOS> ");
         
         // Get input (simplified - in a real OS, this would handle input properly)
         int i = 0;
         char c;
         while ((c = getchar()) != '\n' && i < SHELL_BUFFER_SIZE - 1) {
             input_buffer[i++] = c;
         }
         input_buffer[i] = '\0';
         
         // Process the command
         shell_process_command(input_buffer);
         
         // Schedule processes (in a real OS, this would happen via interrupts)
         process_schedule();
     }
 }
 
 /* ======= SYSTEM INITIALIZATION AND MAIN FUNCTION ======= */
 
 // Initialize the OS
 void os_init() {
     // Initialize subsystems
     memory_init();
     process_init();
     fs_init();
     
     // Set system as running
     simple_os.system_running = true;
     
     printf("SimpleOS initialized successfully\n");
 }
 
 // Main function - OS entry point
 int main() {
     // Initialize the OS
     os_init();
     
     // Run the shell
     shell_run();
     
     // OS shutdown
     printf("SimpleOS shutdown complete\n");
     return 0;
 }