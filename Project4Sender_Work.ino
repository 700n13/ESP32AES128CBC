#include <WiFi.h>
#include <WebServer.h>
#include <CryptoAES_CBC.h>
#include <AES.h>
#include <string.h>

// Wi-Fi credentials - Access Point mode
const char* ssid = "ultra_wifi";
const char* password = "12345678";

// AES encryption key and IV (must be same on both devices)
byte key[16] = { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
                 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
                 
byte iv[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };

// Encryption variables
AES128 aes128;

byte ciphertext[128];  // Increased buffer size for longer messages
int ciphertextLength = 0;

// Create web server
WebServer server(80);

// Helper function to print bytes
void printBytes(const char* label, byte* data, int length) {
    Serial.print(label);
    Serial.print(": ");
    for (int i = 0; i < length; i++) {
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

// Convert byte array to hex string
String byteArrayToHexString(byte* data, int length) {
    String result = "";
    for (int i = 0; i < length; i++) {
        if (data[i] < 0x10) {
            result += "0";
        }
        result += String(data[i], HEX);
        result += " ";
    }
    return result;
}

void encryptMessage(const char* message) {
    int messageLen = strlen(message);
    byte plaintext[128];
    memcpy(plaintext, message, messageLen);
    
    // Add PKCS7 padding
    int paddingLength = 16 - (messageLen % 16);
    for (int i = 0; i < paddingLength; i++) {
        plaintext[messageLen + i] = paddingLength;
    }
    
    int totalLength = messageLen + paddingLength;
    Serial.print("Message with padding length: ");
    Serial.println(totalLength);
    
    // Perform CBC encryption
    byte prev_block[16];
    memcpy(prev_block, iv, 16);  // First block uses IV
    
    for (int i = 0; i < totalLength; i += 16) {
        byte block[16];
        
        // XOR with previous ciphertext block (or IV for first block)
        for (int j = 0; j < 16; j++) {
            block[j] = plaintext[i + j] ^ prev_block[j];
        }
        
        // Encrypt block
        aes128.encryptBlock(&ciphertext[i], block);
        
        // Save encrypted block for next iteration
        memcpy(prev_block, &ciphertext[i], 16);
    }
    
    ciphertextLength = totalLength;
    
    Serial.println("Original message: " + String(message));
    printBytes("Encrypted data", ciphertext, ciphertextLength);
}

void handleRoot() {
    String hexString = byteArrayToHexString(ciphertext, ciphertextLength);
    server.send(200, "text/plain", hexString);
}

void setup() {
    Serial.begin(115200);
    
    // Initialize AES
    aes128.setKey(key, 16);
    
    // Create Access Point
    WiFi.softAP(ssid, password);
    Serial.println("Access Point Started");
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
    
    // Setup web server
    server.on("/", handleRoot);
    server.begin();
    Serial.println("HTTP server started");
    
    // Initial message encryption
    encryptMessage("Hello from ESP32!");
}

void loop() {
    server.handleClient();
    
    // Check for new messages from Serial
    if (Serial.available()) {
        String message = Serial.readStringUntil('\n');
        encryptMessage(message.c_str());
    }
    
    delay(10);
}