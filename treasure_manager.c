#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>

#define MAX_STRING 512
#define MAX_CLUE 1024
#define MAX_TREASURES 100
#define MAX_LOG_DETAILS 1024 // Increased buffer size for log details

// Structure that holds the treasure's info
typedef struct{
  int ID;
  char username[MAX_STRING];
  double latitude, longitude;
  char clue[MAX_CLUE];
  int value;
}Treasure;

// Structure that holds the hunt's info
typedef struct{
  char hunt_id[MAX_STRING];
  Treasure treasures[MAX_TREASURES];
  int treasure_count;
}Hunt;

// Function declarations
void add_treasure(const char *hunt_id);
void list_treasures(const char *hunt_id);
void view_treasure(const char *hunt_id, int treasure_id);
void create_hunt_directory(const char *hunt_id);
char *get_treasure_file_path(const char *hunt_id);
void save_treasures(const char *hunt_id, Hunt *hunt);
Hunt *load_treasures(const char *hunt_id);
void log_operation(const char *hunt_id, const char *operation, const char *details);
void merge_hunt_logs();

// Helper to write a string to a file descriptor
void write_str(int fd, const char *str) {
    write(fd, str, strlen(str));
}

// Function to merge hunt logs into a single file
void merge_hunt_logs()
{
    FILE *output_file = open("hunt_log.txt", O_WRONLY | O_CREAT | O_APPEND, 0644); // Open in append mode
    if (output_file == -1)
    {
        perror("Error opening hunt_log.txt");
        return;
    }

    DIR *hunt_dir = opendir("hunt");
    if (hunt_dir == NULL)
    {
        perror("Error opening hunt directory");
        close(output_file);
        return;
    }

    struct dirent *entry;
    struct stat st;
    char log_path[MAX_STRING];
    char buffer[1024];

       struct dirent *entry;
    struct stat st;
    char log_path[MAX_STRING];
    char buffer[1024];
    ssize_t bytes;

    while ((entry = readdir(hunt_dir)) != NULL) {
        if (entry->d_name[0] == '.') continue; // Skip "." and ".."

        int len = snprintf(log_path, sizeof(log_path), "hunt/%s", entry->d_name);
        if (len < 0 || len >= sizeof(log_path)) continue;

        if (stat(log_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            len = snprintf(log_path, sizeof(log_path), "hunt/%s/logged_hunt.txt", entry->d_name);
            if (len < 0 || len >= sizeof(log_path)) continue;

            int input_fd = open(log_path, O_RDONLY);
            if (input_fd != -1) {
                char header[256];
                int hlen = snprintf(header, sizeof(header), "=== Log for Hunt: %s ===\n", entry->d_name);
                write(output_fd, header, hlen);

                while ((bytes = read(input_fd, buffer, sizeof(buffer))) > 0) {
                    write(output_fd, buffer, bytes);
                }

                write(output_fd, "\n", 1);
                close(input_fd);
            }
        }
    }

    closedir(hunt_dir);
    close(output_fd);
    write_str(STDOUT_FILENO, "Hunt logs merged successfully into hunt_log.txt\n");
}

// log_operation using only low-level I/O
void log_operation(const char *hunt_id, const char *operation, const char *details) {
    char dir_path[MAX_STRING];
    char log_path[MAX_STRING];

    int dlen = snprintf(dir_path, sizeof(dir_path), "hunt/hunt%s", hunt_id);
    if (dlen < 0 || dlen >= sizeof(dir_path)) return;
    mkdir(dir_path, 0755); // create directory if not exists

    int llen = snprintf(log_path, sizeof(log_path), "%s/logged_hunt.txt", dir_path);
    if (llen < 0 || llen >= sizeof(log_path)) return;

    int fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        perror("Error opening log file");
        return;
    }

    time_t now = time(NULL);
    char timestamp[26];
    if (ctime_r(&now, timestamp) == NULL) {
        perror("Error generating timestamp");
        close(fd);
        return;
    }
    timestamp[24] = '\0'; // remove newline

    char entry[MAX_LOG_DETAILS + 256];
    int elen = snprintf(entry, sizeof(entry), "[%s] %s: %s\n", timestamp, operation, details);
    if (elen > 0 && elen < sizeof(entry)) {
        write(fd, entry, elen);
    }

    close(fd);
    merge_hunt_logs();
}

// Function to add a new treasure
void add_treasure(const char *hunt_id) {
    create_hunt_directory(hunt_id);
    Hunt *hunt = load_treasures(hunt_id);

    if (hunt->treasure_count >= MAX_TREASURES) {
        write_str(STDOUT_FILENO, "Max treasures reached\n");
        return;
    }

    Treasure *t = &hunt->treasures[hunt->treasure_count];
    t->id = hunt->treasure_count + 1;

    printf("Enter username: ");
    scanf("%s", t->username);
    printf("Enter latitude: ");
    scanf("%lf", &t->latitude);
    printf("Enter longitude: ");
    scanf("%lf", &t->longitude);
    printf("Enter clue: "); getchar();
    fgets(t->clue, MAX_CLUE, stdin);
    t->clue[strcspn(t->clue, "\n")] = 0;
    printf("Enter value: ");
    scanf("%d", &t->value);

    hunt->treasure_count++;
    save_treasures(hunt_id, hunt);

    char details[MAX_LOG_DETAILS];
    sprintf(details, "Added treasure ID: %d, Username: %s, Value: %d", t->id, t->username, t->value);
    log_operation(hunt_id, "ADD", details);

    printf("Treasure added successfully with ID: %d\n", t->id);
}

// Function to list all treasures from a hunt
void list_treasures(const char *hunt_id) {
    Hunt *hunt = load_treasures(hunt_id);
    if (hunt->treasure_count == 0) {
        printf("No treasures found.\n");
        return;
    }

    printf("\nTreasures in hunt %s:\n", hunt_id);
    for (int i = 0; i < hunt->treasure_count; ++i) {
        Treasure *t = &hunt->treasures[i];
        printf("ID: %d\nUsername: %s\nLocation: %.6f, %.6f\nClue: %s\nValue: %d\n\n",
               t->id, t->username, t->latitude, t->longitude, t->clue, t->value);
    }

    char details[MAX_LOG_DETAILS];
    sprintf(details, "Listed %d treasures", hunt->treasure_count);
    log_operation(hunt_id, "LIST", details);
}

// Function to view a specific treasure
void view_treasure(const char *hunt_id, int id) {
    Hunt *hunt = load_treasures(hunt_id);
    for (int i = 0; i < hunt->treasure_count; ++i) {
        Treasure *t = &hunt->treasures[i];
        if (t->id == id) {
            printf("\nTreasure Details:\nID: %d\nUsername: %s\nLocation: %.6f, %.6f\nClue: %s\nValue: %d\n",
                   t->id, t->username, t->latitude, t->longitude, t->clue, t->value);

            char details[MAX_LOG_DETAILS];
            sprintf(details, "Viewed treasure ID: %d, Username: %s", t->id, t->username);
            log_operation(hunt_id, "VIEW", details);
            return;
        }
    }
    printf("Treasure with ID %d not found.\n", id);
}

// Function to remove a treasure from a hunt
void remove_treasure(const char *hunt_id, int treasure_id) {
    Hunt *hunt = load_treasures(hunt_id);
    int index = -1;
    for (int i = 0; i < hunt->treasure_count; i++) {
        if (hunt->treasures[i].id == treasure_id) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        printf("Treasure with ID %d not found in hunt %s\n", treasure_id, hunt_id);
        char log_details[MAX_LOG_DETAILS];
        snprintf(log_details, sizeof(log_details), "Failed to remove treasure ID: %d (not found)", treasure_id);
        log_operation(hunt_id, "REMOVE", log_details);
        return;
    }

    for (int i = index; i < hunt->treasure_count - 1; i++) {
        hunt->treasures[i] = hunt->treasures[i + 1];
    }
    hunt->treasure_count--;
    save_treasures(hunt_id, hunt);

    char log_details[MAX_LOG_DETAILS];
    snprintf(log_details, sizeof(log_details), "Removed treasure ID: %d", treasure_id);
    log_operation(hunt_id, "REMOVE", log_details);
    printf("Treasure with ID %d removed successfully.\n", treasure_id);
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("Usage: %s <command> <hunt_id> [treasure_id]\n", argv[0]);
        printf("Commands:\n");
        printf("  add <hunt_id> - Add a new treasure\n");
        printf("  list <hunt_id> - List all treasures\n");
        printf("  view <hunt_id> <treasure_id> - View specific treasure\n");
        return 1;
    }

    char *command = argv[1];
    char *hunt_id = argv[2];
    int treasure_id = (argc > 3) ? atoi(argv[3]) : 0;

    if (strcmp(command, "add") == 0)
    {
        add_treasure(hunt_id);
    }
    else if (strcmp(command, "list") == 0)
    {
        list_treasures(hunt_id);
    }
    else if (strcmp(command, "view") == 0)
    {
        view_treasure(hunt_id, treasure_id);
    }
    else
    {
        printf("Unknown command: %s\n", command);
        return 1;
    }

    return 0;
}