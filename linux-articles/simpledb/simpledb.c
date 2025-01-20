/******************************************************************************
 * simpledb.c
 *
 * A minimal JSON-based command-line database utility in C.
 * 
 * To compile (assuming cJSON is installed via "sudo apt install libcjson-dev"):
 *     gcc -Wall -o simpledb simpledb.c -I/usr/include/cjson -lcjson
 *
 * Usage:
 *     ./simpledb --db-path <PATH> list <table>
 *     ./simpledb --db-path <PATH> get <table> field=value
 *     ./simpledb --db-path <PATH> save <table> field1=value1 field2=value2 ...
 *     ./simpledb --db-path <PATH> delete <table> field=value
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>   // mkdir, etc.
#include <errno.h>
#include <unistd.h>     // for rename, close, etc.
#include <fcntl.h>      // for open
#include "cJSON.h"      // cJSON library header

#define MAX_COMMAND_ARGS 128

typedef struct {
    char key[256];
    char val[1024];
} FieldPair;

/* --------------------------------------------------------------------------
 * Utility: Print usage instructions
 * -------------------------------------------------------------------------- */
static void print_usage(const char* prog_name) {
    fprintf(stderr,
        "Usage:\n"
        "  %s --db-path <PATH> COMMAND [ARGS...]\n\n"
        "Commands:\n"
        "  list <table>\n"
        "  get <table> field=value\n"
        "  save <table> field1=value1 [field2=value2 ...]\n"
        "  delete <table> field=value\n"
        "\n"
        "Options:\n"
        "  --db-path <PATH>   Required. Path to the database directory.\n"
        "\n", prog_name);
}

/* --------------------------------------------------------------------------
 * Utility: Read entire file into a dynamically allocated buffer
 * Returns the pointer to the buffer (caller must free), or NULL on error.
 * -------------------------------------------------------------------------- */
static char* read_file(const char* filename) {
    FILE* fp = fopen(filename, "rb");
    if (!fp) {
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size < 0) {
        fclose(fp);
        return NULL;
    }

    char* content = (char*)malloc(file_size + 1);
    if (!content) {
        fclose(fp);
        return NULL;
    }

    size_t read_size = fread(content, 1, file_size, fp);
    fclose(fp);
    content[read_size] = '\0';  // Null-terminate
    return content;
}

/* --------------------------------------------------------------------------
 * Utility: Write a temporary file, then rename it to ensure atomic updates.
 * Returns 0 on success, non-zero on error.
 * -------------------------------------------------------------------------- */
static int write_file_atomic(const char* filename, const char* data) {
    // Create a temp file name
    char temp_filename[1024];
    snprintf(temp_filename, sizeof(temp_filename), "%s.tmp", filename);

    // Write data to temp file
    FILE* fp = fopen(temp_filename, "wb");
    if (!fp) {
        return -1;
    }
    size_t len = strlen(data);
    if (fwrite(data, 1, len, fp) < len) {
        fclose(fp);
        return -1;
    }
    fclose(fp);

    // Atomically rename the temp file to the actual file
    if (rename(temp_filename, filename) != 0) {
        return -1;
    }
    return 0;
}

/* --------------------------------------------------------------------------
 * Load the JSON array from <table>.json, or create an empty JSON array if file
 * doesn't exist. Return a cJSON pointer, or NULL on error.
 * -------------------------------------------------------------------------- */
static cJSON* load_table(const char* db_path, const char* table_name) {
    // Construct the file path: e.g., db_path/users.json
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", db_path, table_name);

    char* content = read_file(filepath);
    cJSON* root = NULL;

    if (content) {
        // Parse the JSON
        root = cJSON_Parse(content);
        free(content);

        // If parse failed or root is not an array, create a new array
        if (!root || !cJSON_IsArray(root)) {
            if (root) {
                cJSON_Delete(root);
            }
            root = cJSON_CreateArray();
        }
    } else {
        // File not found or not readable; let's assume it's just empty
        root = cJSON_CreateArray();
    }

    return root;
}

/* --------------------------------------------------------------------------
 * Write the JSON array back to <table>.json (atomically).
 * -------------------------------------------------------------------------- */
static int save_table(const char* db_path, const char* table_name, cJSON* root) {
    if (!root) return -1;

    char* print_buffer = cJSON_PrintUnformatted(root);
    if (!print_buffer) {
        return -1;
    }

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s.json", db_path, table_name);

    int ret = write_file_atomic(filepath, print_buffer);
    free(print_buffer);
    return ret;
}

/* --------------------------------------------------------------------------
 * list <table>
 * Print all records in JSON lines format.
 * -------------------------------------------------------------------------- */
static int command_list(const char* db_path, const char* table_name) {
    cJSON* root = load_table(db_path, table_name);
    if (!root) {
        fprintf(stderr, "Error: Could not load or parse table %s\n", table_name);
        return 1;
    }

    // Print each record (object) as one line of JSON
    int array_size = cJSON_GetArraySize(root);
    for (int i = 0; i < array_size; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (cJSON_IsObject(item)) {
            char* line = cJSON_PrintUnformatted(item);
            if (line) {
                printf("%s\n", line);
                free(line);
            }
        }
    }

    cJSON_Delete(root);
    return 0;
}

/* --------------------------------------------------------------------------
 * get <table> field=value
 * Print all records where field matches value.
 * -------------------------------------------------------------------------- */
static int command_get(const char* db_path, const char* table_name, 
                       const char* field, const char* value) {
    cJSON* root = load_table(db_path, table_name);
    if (!root) {
        fprintf(stderr, "Error: Could not load or parse table %s\n", table_name);
        return 1;
    }

    int array_size = cJSON_GetArraySize(root);
    for (int i = 0; i < array_size; i++) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (cJSON_IsObject(item)) {
            cJSON* field_obj = cJSON_GetObjectItemCaseSensitive(item, field);
            if (cJSON_IsString(field_obj) && strcmp(field_obj->valuestring, value) == 0) {
                // Print matching record
                char* line = cJSON_PrintUnformatted(item);
                if (line) {
                    printf("%s\n", line);
                    free(line);
                }
            }
        }
    }

    cJSON_Delete(root);
    return 0;
}

/* --------------------------------------------------------------------------
 * save <table> field1=value1 [field2=value2 ...]
 *   - If a record with the specified unique id (if given) exists, update it.
 *   - If a record with the specified unique id (if given) doesn't exist, create a new one.
 *   - If no id is given, it automatically generates the id
 *
 *   Usually we assume "id" is the unique field, but let's not hardcode it:
 *   We'll check if any of the fields is "id=xxx". If found, we try to locate 
 *   that record first, then update. If not found, we append a new record.
 * -------------------------------------------------------------------------- */
static char* generate_new_id(cJSON* root) {
    // This function returns a dynamically allocated string (caller must free).
    // Strategy: find the highest numeric ID so far and add 1.
    // If no numeric ID is found, default to "1".
    int max_id = 0;
    int size = cJSON_GetArraySize(root);
    for (int i = 0; i < size; i++) {
        cJSON* obj = cJSON_GetArrayItem(root, i);
        if (cJSON_IsObject(obj)) {
            cJSON* id_field = cJSON_GetObjectItemCaseSensitive(obj, "id");
            if (cJSON_IsString(id_field)) {
                // attempt to interpret as integer
                int val = atoi(id_field->valuestring);
                if (val > max_id) {
                    max_id = val;
                }
            }
        }
    }
    max_id++;  // new ID is max_id + 1

    // Convert to string
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", max_id);
    return strdup(buffer);  // Return a copy
}

static int command_save(const char* db_path, const char* table_name,
                        int argc, char** argv) 
{
    // Load table JSON
    cJSON* root = load_table(db_path, table_name);
    if (!root) {
        fprintf(stderr, "Error: Could not load or parse table %s\n", table_name);
        return 1;
    }

    // --------------------------------------------------------------------
    // 1) Parse all fields from argv into an array of FieldPair
    //    We also check if an 'id' was provided and validate it.
    // --------------------------------------------------------------------
    FieldPair fields[MAX_COMMAND_ARGS];
    int fieldCount = 0;

    bool userProvidedId = false;
    long userIdValue = 0; // numeric representation if user provided 'id'

    for (int i = 0; i < argc; i++) {
        // We don't want to modify argv[i] directly in case we need it later;
        // so let's copy it into a buffer.
        char buffer[2048];
        strncpy(buffer, argv[i], sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';

        // Split at '='
        char* eq = strchr(buffer, '=');
        if (!eq) {
            fprintf(stderr, "Error: Invalid field format '%s'. Use field=value.\n", argv[i]);
            cJSON_Delete(root);
            return 1;
        }
        *eq = '\0'; 
        const char* key = buffer;
        const char* val = eq + 1;

        // Store into fields array
        strncpy(fields[fieldCount].key, key, sizeof(fields[fieldCount].key) - 1);
        fields[fieldCount].key[sizeof(fields[fieldCount].key) - 1] = '\0';
        strncpy(fields[fieldCount].val, val, sizeof(fields[fieldCount].val) - 1);
        fields[fieldCount].val[sizeof(fields[fieldCount].val) - 1] = '\0';
        fieldCount++;

        // Check if this is 'id'
        if (strcmp(key, "id") == 0) {
            userProvidedId = true;

            // Validate positive integer
            char* endptr = NULL;
            long val_long = strtol(val, &endptr, 10);
            if (*endptr != '\0' || val_long <= 0) {
                fprintf(stderr, "Error: 'id' must be a positive integer, got '%s'\n", val);
                cJSON_Delete(root);
                return 1;
            }
            userIdValue = val_long;
        }
    }

    // --------------------------------------------------------------------
    // 2) Determine final ID string (either user provided or auto-generated)
    // --------------------------------------------------------------------
    char autoIdBuffer[32];
    const char* finalIdStr = NULL;

    if (userProvidedId) {
        // Reconstruct the numeric ID as a string (just to be sure)
        snprintf(autoIdBuffer, sizeof(autoIdBuffer), "%ld", userIdValue);
        finalIdStr = autoIdBuffer;
    } else {
        // Generate ID automatically
        char* generated = generate_new_id(root);
        if (!generated) {
            fprintf(stderr, "Error: unable to generate new ID.\n");
            cJSON_Delete(root);
            return 1;
        }
        // We'll store this pointer in finalIdStr and later free it
        finalIdStr = generated;
    }

    // --------------------------------------------------------------------
    // 3) Create a new cJSON object, add 'id' first,
    //    then add all other fields in the same order they were listed
    // --------------------------------------------------------------------
    cJSON* new_record = cJSON_CreateObject();
    if (!new_record) {
        // If we auto-generated the ID string, we must free it
        if (!userProvidedId && finalIdStr) {
            free((void*)finalIdStr);
        }
        cJSON_Delete(root);
        return 1;
    }

    // Add 'id' as the first field
    cJSON_AddStringToObject(new_record, "id", finalIdStr);

    // Add all the other fields
    for (int i = 0; i < fieldCount; i++) {
        if (strcmp(fields[i].key, "id") == 0) {
            // already added as first field
            continue;
        }
        cJSON_AddStringToObject(new_record, fields[i].key, fields[i].val);
    }

    // If we generated the ID, free it now
    if (!userProvidedId && finalIdStr) {
        free((void*)finalIdStr);
        finalIdStr = NULL;
    }

    // --------------------------------------------------------------------
    // 4) Check if there's an existing record with this 'id'
    // --------------------------------------------------------------------
    cJSON* existing_record = NULL;
    {
        int size = cJSON_GetArraySize(root);
        // The string we compare must be the same we used for 'id'
        // but now it's stored in new_record->id. Let's just read from new_record.
        cJSON* newIdField = cJSON_GetObjectItemCaseSensitive(new_record, "id");
        if (!cJSON_IsString(newIdField)) {
            // Shouldn't happen, but let's be safe
            cJSON_Delete(new_record);
            cJSON_Delete(root);
            return 1;
        }

        for (int i = 0; i < size; i++) {
            cJSON* item = cJSON_GetArrayItem(root, i);
            if (cJSON_IsObject(item)) {
                cJSON* idField = cJSON_GetObjectItemCaseSensitive(item, "id");
                if (cJSON_IsString(idField) &&
                    strcmp(idField->valuestring, newIdField->valuestring) == 0)
                {
                    existing_record = item;
                    break;
                }
            }
        }
    }

    // --------------------------------------------------------------------
    // 5) If found, update that record. Otherwise, append new_record
    // --------------------------------------------------------------------
    if (existing_record) {
        cJSON* field = NULL;
        cJSON_ArrayForEach(field, new_record) {
            cJSON* dup = cJSON_Duplicate(field, 1);
            cJSON_ReplaceItemInObjectCaseSensitive(existing_record, field->string, dup);
        }
        cJSON_Delete(new_record); 
    } else {
        cJSON_AddItemToArray(root, new_record);
    }

    // --------------------------------------------------------------------
    // 6) Save the updated JSON array to file
    // --------------------------------------------------------------------
    if (save_table(db_path, table_name, root) != 0) {
        fprintf(stderr, "Error: Could not save table %s\n", table_name);
        cJSON_Delete(root);
        return 1;
    }

    // --------------------------------------------------------------------
    // 7) Print the record for user feedback
    // --------------------------------------------------------------------
    cJSON* recordToPrint = existing_record ? existing_record : new_record;
    char* line = cJSON_PrintUnformatted(recordToPrint);
    if (line) {
        printf("%s\n", line);
        free(line);
    }

    cJSON_Delete(root);
    return 0;
}

/* --------------------------------------------------------------------------
 * delete <table> field=value
 * Remove all records that match `field=value`.
 * -------------------------------------------------------------------------- */
static int command_delete(const char* db_path, const char* table_name,
                          const char* field, const char* value) {
    cJSON* root = load_table(db_path, table_name);
    if (!root) {
        fprintf(stderr, "Error: Could not load or parse table %s\n", table_name);
        return 1;
    }

    int i = 0;
    int deleted_count = 0;

    // Iterate in a forward manner but be mindful that removing items changes indices
    while (i < cJSON_GetArraySize(root)) {
        cJSON* item = cJSON_GetArrayItem(root, i);
        if (cJSON_IsObject(item)) {
            cJSON* field_obj = cJSON_GetObjectItemCaseSensitive(item, field);
            if (cJSON_IsString(field_obj) && strcmp(field_obj->valuestring, value) == 0) {
                // Remove this item
                cJSON_DeleteItemFromArray(root, i);
                deleted_count++;
                continue; // Do not increment i, because the array just shifted
            }
        }
        i++;
    }

    if (save_table(db_path, table_name, root) != 0) {
        fprintf(stderr, "Error: Could not save table %s after deletion\n", table_name);
        cJSON_Delete(root);
        return 1;
    }

    cJSON_Delete(root);
    printf("Deleted %d record(s)\n", deleted_count);
    return 0;
}

/* --------------------------------------------------------------------------
 * main
 * Parse arguments, decide which command to run.
 * -------------------------------------------------------------------------- */
int main(int argc, char* argv[]) {
    // Minimal argument parsing:
    // Expect at least: ./simpledb --db-path <PATH> command ...
    if (argc < 4) {
        print_usage(argv[0]);
        return 1;
    }

    const char* db_path = NULL;
    const char* command = NULL;
    const char* table_name = NULL;

    // We'll collect any extra arguments in an array for "save" command
    char* command_args[MAX_COMMAND_ARGS];
    int command_args_count = 0;

    // 1) First parse the --db-path option
    // 2) Then the command, then the rest

    int i = 1;
    for (; i < argc; i++) {
        if (strcmp(argv[i], "--db-path") == 0 || strcmp(argv[i], "-d") == 0) {
            if (i + 1 < argc) {
                db_path = argv[++i];
                continue;
            } else {
                fprintf(stderr, "Error: --db-path requires an argument\n");
                return 1;
            }
        } else {
            // This is likely the command
            command = argv[i];
            i++;
            break;
        }
    }

    if (!db_path || !command) {
        print_usage(argv[0]);
        return 1;
    }

    // Now, the next argument should be <table> at least for most commands
    if (i < argc) {
        table_name = argv[i];
        i++;
    } else {
        print_usage(argv[0]);
        return 1;
    }

    // Collect any remainder as command_args
    command_args_count = 0;
    for (; i < argc && command_args_count < MAX_COMMAND_ARGS; i++) {
        command_args[command_args_count++] = argv[i];
    }

    // Ensure the database directory exists (optional, but let's do a check)
    struct stat st;
    if (stat(db_path, &st) != 0) {
        fprintf(stderr, "Error: Database path '%s' does not exist.\n", db_path);
        return 1;
    }
    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a directory.\n", db_path);
        return 1;
    }

    // Dispatch commands
    if (strcmp(command, "list") == 0) {
        if (command_args_count != 0) {
            // 'list' expects: simpledb --db-path <path> list <table>
            // No extra arguments after <table>
            print_usage(argv[0]);
            return 1;
        }
        return command_list(db_path, table_name);

    } else if (strcmp(command, "get") == 0) {
        // Expects: get <table> field=value
        if (command_args_count != 1) {
            print_usage(argv[0]);
            return 1;
        }
        char* eq = strchr(command_args[0], '=');
        if (!eq) {
            fprintf(stderr, "Error: Invalid get argument '%s'. Use field=value.\n", command_args[0]);
            return 1;
        }
        *eq = '\0';
        const char* field = command_args[0];
        const char* value = eq + 1;
        return command_get(db_path, table_name, field, value);

    } else if (strcmp(command, "save") == 0) {
        // Expects: save <table> field1=value1 [field2=value2 ...]
        if (command_args_count < 1) {
            print_usage(argv[0]);
            return 1;
        }
        return command_save(db_path, table_name, command_args_count, command_args);

    } else if (strcmp(command, "delete") == 0) {
        // Expects: delete <table> field=value
        if (command_args_count != 1) {
            print_usage(argv[0]);
            return 1;
        }
        char* eq = strchr(command_args[0], '=');
        if (!eq) {
            fprintf(stderr, "Error: Invalid delete argument '%s'. Use field=value.\n", command_args[0]);
            return 1;
        }
        *eq = '\0';
        const char* field = command_args[0];
        const char* value = eq + 1;
        return command_delete(db_path, table_name, field, value);

    } else {
        fprintf(stderr, "Error: Unknown command '%s'\n", command);
        print_usage(argv[0]);
        return 1;
    }
}

