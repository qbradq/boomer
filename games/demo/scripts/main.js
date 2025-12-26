import * as console from "console";

console.log("Main Script Starting...");

// Load the test map
if (loadMap("test.json")) {
    console.log("Map 'test.json' loaded successfully.");
} else {
    console.error("Failed to load map 'test.json'.");
}
