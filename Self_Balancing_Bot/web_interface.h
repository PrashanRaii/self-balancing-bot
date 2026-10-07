#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

// Starts the WiFi access point, HTTP dashboard, and WebSocket telemetry.
// Call once from setup().
void initWebInterface();

// Retained for compatibility with the current sketch loop. The dashboard
// implementation services HTTP and WebSockets on its own FreeRTOS task.
void webInterfaceLoop();

#endif
