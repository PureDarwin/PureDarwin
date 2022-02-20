// BUILD(macos):  $CC foo.c         -o $BUILD_DIR/libfoo.dylib -dynamiclib -install_name $RUN_DIR/lifoo.dylib
// BUILD(macos):  $CC target.c      -o $BUILD_DIR/dyld_usage_target.exe  -DRUN_DIR="$RUN_DIR"
// BUILD(macos):  $CXX main.mm     -o $BUILD_DIR/dyld_usage_json.exe -DRUN_DIR="$RUN_DIR" -std=c++14 -framework Foundation

// BUILD(ios,tvos,watchos,bridgeos):

// RUN:  $SUDO ./dyld_usage_json.exe

#import <Foundation/Foundation.h>

#include <errno.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "test_support.h"

enum class NodeValueType {
    Default,
    String,
    RawValue,
};

struct Node
{
    NodeValueType               type = NodeValueType::Default;
    std::string                 value;
    std::map<std::string, Node> map;
    std::vector<Node>           array;

    inline Node()
    : type(NodeValueType::Default), value(), map(), array() { }

    inline Node(std::string string)
    : type(NodeValueType::String), value(string), map(), array() { }

    inline Node(const char *string) : Node(std::string(string)) { }

    inline Node(bool b)
    : type(NodeValueType::RawValue), value(b ? "true" : "false")
    , map(), array() { }

    inline Node(int64_t i64)
    : type(NodeValueType::RawValue), value(), map(), array()
    {
        std::ostringstream os;
        os << i64;
        value = os.str();
    }

    inline Node(uint64_t u64)
    : type(NodeValueType::RawValue), value(), map(), array()
    {
        std::ostringstream os;
        os << u64;
        value = os.str();
    }
};

static Node parseNode(id jsonObject) {
    __block Node node;

    // NSDictionary -> map
    if ([jsonObject isKindOfClass:[NSDictionary class]]) {
        NSDictionary* dict = (NSDictionary*)jsonObject;

        [dict enumerateKeysAndObjectsUsingBlock:^(id key, id value, BOOL* stop) {
            if (![key isKindOfClass:[NSString class]]) {
                fprintf(stderr, "JSON map key is not of string type\n");
                *stop = true;
                return;
            }
            Node childNode = parseNode(value);

            node.map[[key UTF8String]] = childNode;
        }];

        return node;
    }

    // NSArray -> array
    if ([jsonObject isKindOfClass:[NSArray class]]) {
        NSArray* array = (NSArray*)jsonObject;

        [array enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL * stop) {
            Node childNode = parseNode(obj);
            node.array.push_back(childNode);
        }];

        return node;
    }

    // NSString -> value
    if ([jsonObject isKindOfClass:[NSString class]]) {
        node.value = [(NSString*)jsonObject UTF8String];
        return node;
    }

    fprintf(stderr, "Unknown json deserialized type\n");
    return Node();
}

Node readJSON(const void * contents, size_t length) {
    NSData* data = [NSData dataWithBytes:contents length:length];
    NSError* error = nil;
    id jsonObject = [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingMutableContainers error:&error];
    if (!jsonObject) {
        fprintf(stderr, "Could not deserialize json because '%s'",[[error localizedFailureReason] UTF8String]);
        return Node();
    }

    return parseNode(jsonObject);
}

char* mergeJsonRoots(char* jsonBuffer, size_t size)
{
    char *mergedJson = (char*)malloc((size + 2) * sizeof(char));
    mergedJson[0] = '[';
    mergedJson[size+1] = '\0';
    mergedJson[size+1] = ']';
    for (size_t i = 0; i < size; i++) {
        mergedJson[i+1] = jsonBuffer[i];
        if (jsonBuffer[i] == '\n') {
            if ( i > 0 && i < size - 1 ) {
                if (jsonBuffer[i-1] == '}' && jsonBuffer[i+1] == '{')
                    mergedJson[i+1] = ',';
            }
        }
    }
    return mergedJson;
}

void validateJson(Node json, pid_t pid)
{
    size_t expectedSize = 4;
    if (json.array.size() != expectedSize)
        FAIL("dyld_usage reported number of events is incorrect. Reported %lu instead of %lu", json.array.size(), expectedSize);

    std::string handle = json.array[1].map["event"].map["result"].value;

    for (size_t i = 0; i < json.array.size(); i++) {

        if ( json.array[i].map["command"].value.compare("dyld_usage_target.exe") != 0 )
            FAIL("Incorrect command name for event at index %lu", i);

        int jpid = std::stoi(json.array[i].map["pid"].value);
        if ( jpid != pid)
            FAIL("Incorrect pid for event at index %lu. Reported %d intead of %d (%s)", i, jpid, pid, json.array[i].map["pid"].value.c_str());

        if (i == 0) {
            if ( json.array[i].map["event"].map["type"].value.compare("app_launch") != 0 )
                FAIL("dyld_usage did not report app launch event");
        }

        if (i == 1) {
            if ( json.array[1].map["event"].map["type"].value.compare("dlopen") != 0 )
                FAIL("dyld_usage did not report dlopen event");
            if ( json.array[1].map["event"].map["path"].value.compare(RUN_DIR "/libfoo.dylib") != 0 )
                FAIL("Incorrect dlopen library path");
        }

        if (i == 2) {
            if ( json.array[i].map["event"].map["type"].value.compare("dlsym") != 0 )
                FAIL("dyld_usage did not report dlsym event");
            if ( json.array[i].map["event"].map["symbol"].value.compare("foo") != 0 )
                FAIL("incorrect dlsym symbol reported");
            if ( json.array[i].map["event"].map["handle"].value.compare(handle) != 0 )
                FAIL("dlsym handle does not match dlopen result");
        }

        if (i == 3) {
            if ( json.array[i].map["event"].map["type"].value.compare("dlclose") != 0 )
                FAIL("dyld_usage did not report dlclose event");
            if ( json.array[i].map["event"].map["handle"].value.compare(handle) != 0 )
                FAIL("dlclose handle does not match dlopen result");
        }
    }
}

int main(int argc, const char* argv[], char *env[])
{
    _process dyldUsage;
    dyldUsage.set_executable_path("/usr/local/bin/dyld_usage");
    const char* args[] = { "-j", "dyld_usage_target.exe", NULL };
    dyldUsage.set_args(args);
    __block dispatch_data_t output = NULL;
    dyldUsage.set_stdout_handler(^(int fd) {
        ssize_t size = 0;
        do {
            char buffer[16384] = {0};
            size = read(fd, buffer, 16384);
            if ( size == -1 )
                break;
            dispatch_data_t data = dispatch_data_create(buffer, size, NULL, DISPATCH_DATA_DESTRUCTOR_DEFAULT);
            output = output ? dispatch_data_create_concat(output, data) : data;
        } while ( size > 0 );
    });

    pid_t pid = dyldUsage.launch();
    usleep(2000000);

    // Launch target
    _process target;
    target.set_executable_path(RUN_DIR "/dyld_usage_target.exe");
    pid_t tpid = target.launch();

    usleep(2000000);
    // Kill dyld_usage
    kill(pid, SIGTERM);


    int status;
    if (waitpid(pid, &status, 0) == -1)
        FAIL("waitpid failed");
    if ( !output )
        FAIL("No dyld_usage output");

    const void* buffer;
    size_t size;
    (void)dispatch_data_create_map(output, &buffer, &size);
    char* jsonBuffer = mergeJsonRoots((char*)buffer, size);
    size += 2;
    Node node = readJSON(jsonBuffer, size);
    free(jsonBuffer);
    validateJson(node, tpid);

    PASS("Success");
    return 0;
}
