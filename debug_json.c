#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE 512

// Debug version to understand exactly where parsing fails
static long find_pattern_debug(FILE *file, long start_pos, const char *pattern, long max_search) {
    size_t pattern_len = strlen(pattern);
    if (pattern_len == 0) return -1;
    
    char *search_buffer = malloc(BUFFER_SIZE + pattern_len);
    if (!search_buffer) return -1;
    
    long current_pos = start_pos;
    long searched = 0;
    size_t buffer_overlap = 0;
    int iteration = 0;
    
    while (searched < max_search) {
        iteration++;
        
        // Seek to current position
        if (fseek(file, current_pos, SEEK_SET) != 0) {
            printf("[DEBUG] fseek failed at pos %ld\n", current_pos);
            free(search_buffer);
            return -1;
        }
        
        // Read new data after any overlap from previous buffer
        size_t bytes_to_read = BUFFER_SIZE;
        size_t bytes_read = fread(search_buffer + buffer_overlap, 1, bytes_to_read, file);
        
        if (bytes_read == 0) {
            printf("[DEBUG] End of file reached at iteration %d, pos %ld\n", iteration, current_pos);
            break;
        }
        
        // Null terminate for string operations
        size_t total_bytes = buffer_overlap + bytes_read;
        search_buffer[total_bytes] = '\0';
        
        // Search for pattern in current buffer
        char *found = strstr(search_buffer, pattern);
        if (found) {
            long found_pos = current_pos - buffer_overlap + (found - search_buffer);
            printf("[DEBUG] Found pattern at pos %ld (iteration %d)\n", found_pos, iteration);
            free(search_buffer);
            return found_pos;
        }
        
        // If we read less than requested, we're at end of file
        if (bytes_read < bytes_to_read) {
            printf("[DEBUG] Partial read (%zu/%zu bytes) at iteration %d, pos %ld - end of file\n", 
                   bytes_read, bytes_to_read, iteration, current_pos);
            break;
        }
        
        // Prepare overlap for next iteration
        if (total_bytes >= pattern_len) {
            memmove(search_buffer, search_buffer + total_bytes - (pattern_len - 1), pattern_len - 1);
            buffer_overlap = pattern_len - 1;
        } else {
            buffer_overlap = total_bytes;
        }
        
        // Move forward by the amount we actually processed
        current_pos += bytes_read;
        searched += bytes_read;
        
        if (iteration % 200 == 0) {
            printf("[DEBUG] Iteration %d: pos %ld, searched %ld bytes\n", iteration, current_pos, searched);
        }
    }
    
    printf("[DEBUG] Pattern not found after %d iterations, searched %ld bytes\n", iteration, searched);
    free(search_buffer);
    return -1;
}

int count_objects_debug(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        printf("Failed to open file\n");
        return -1;
    }
    
    // Find allFiles array
    long all_files_pos = find_pattern_debug(file, 0, "\"allFiles\":", 1000000);
    if (all_files_pos == -1) {
        printf("Could not find allFiles array\n");
        fclose(file);
        return -1;
    }
    
    printf("Found allFiles at position %ld\n", all_files_pos);
    
    // Find opening bracket
    long bracket_pos = find_pattern_debug(file, all_files_pos, "[", 1000);
    if (bracket_pos == -1) {
        printf("Could not find opening bracket\n");
        fclose(file);
        return -1;
    }
    
    printf("Found opening bracket at position %ld\n", bracket_pos);
    
    // Count objects
    long current_pos = bracket_pos + 1;
    int count = 0;
    
    printf("Starting to count objects from position %ld\n", current_pos);
    
    while (count < 1000) { // Limit for debugging
        long object_pos = find_pattern_debug(file, current_pos, "{", 10000000);
        if (object_pos == -1) {
            printf("No more objects found after %d objects\n", count);
            break;
        }
        
        // Check if we've hit the end of the array
        long closing_bracket = find_pattern_debug(file, current_pos, "]", object_pos - current_pos + 10);
        if (closing_bracket != -1 && closing_bracket < object_pos) {
            printf("Found closing bracket at %ld before next object at %ld\n", closing_bracket, object_pos);
            break;
        }
        
        count++;
        current_pos = object_pos + 1;
        
        if (count % 50 == 0) {
            printf("Found %d objects so far...\n", count);
        }
    }
    
    fclose(file);
    return count;
}

int main() {
    printf("Debug: Counting objects in large_index_500.json\n");
    int count = count_objects_debug("large_index_500.json");
    printf("Final count: %d\n", count);
    return 0;
}
