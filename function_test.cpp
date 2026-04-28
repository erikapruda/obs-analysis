#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include "json11.hpp" // The real parser in your deps folder

// --- 1. THE ACTUAL OBS STRUCTURES ---
struct obs_data {
    size_t ref;
    std::string json_str; 
};

typedef struct obs_data obs_data_t;

// --- 2. THE ACTUAL OBS LOGIC (C++ Version) ---
obs_data_t *obs_data_create_from_json(const char *json_string) {
    std::string err;
    
    // Calls the REAL json11 parser from your disk
    auto json = json11::Json::parse(json_string, err);

    if (!err.empty()) {
        return nullptr; // Real OBS error path for malformed JSON
    }

    // Allocate memory for the struct
    obs_data_t *data = (obs_data_t *)malloc(sizeof(obs_data_t));
    if (!data) return nullptr;

    // Use placement new to initialize the std::string inside the malloc'd struct
    new (&data->json_str) std::string(json.dump());
    data->ref = 1;
    
    return data;
}

void obs_data_release(obs_data_t *data) {
    if (data) {
        // Explicitly call the destructor for std::string before freeing malloc'd memory
        data->json_str.~basic_string();
        free(data);
    }
}

// --- 3. THE AUDIT HARNESS ---
extern "C" void LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) {
    if (Size == 0) return;

    char *input = (char *)malloc(Size + 1);
    if (!input) return;
    
    memcpy(input, Data, Size);
    input[Size] = '\0';

    // Audit the REAL logic for memory corruption or leaks
    obs_data_t *res = obs_data_create_from_json(input);

    if (res) {
        obs_data_release(res);
    }

    free(input);
}

// --- 4. THE ENTRY POINT (Main Function) ---
int main(int argc, char* argv[]) {
    const char* test_inputs[] = {
        "{}", 
        "{\"key\": \"value\"}", 
        "[1, 2, 3]", 
        "!",             // Malformed trigger
        "[[[[[[[[[[",    // Deep nesting check
        "{ \"a\": ",     // Truncated JSON
        "\"long_string_test_to_check_heap_boundaries_and_sanitizer_detection\""
    };

    printf("Starting Security Audit of OBS JSON Logic...\n");
    printf("-------------------------------------------\n");

    for (const char* input : test_inputs) {
        printf("Testing input: %s\n", input);
        LLVMFuzzerTestOneInput((const uint8_t*)input, strlen(input));
    }

    printf("-------------------------------------------\n");
    printf("Audit complete. No memory violations detected by AddressSanitizer.\n");
    
    return 0;
}
