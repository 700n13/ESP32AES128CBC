#include <WiFi.h>
#include <HTTPClient.h>
#include <CryptoAES_CBC.h>
#include <AES.h>
#include <string.h>

// Wi-Fi credentials - must match sender's AP
const char* ssid = "ultra_wifi";
const char* password = "12345678";

// AES key and IV (must match sender)
byte key[16] = { 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
                 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C };
                 
byte iv[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };

// Decryption variables
AES128 aes128;
byte decryptedtext[128];

// Server URL (will be set after connecting to WiFi)
String serverURL;

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

// Convert hex string to byte array
int hexStringToByteArray(String hexString, byte* data) {
    String cleanHex = "";
    for (int i = 0; i < hexString.length(); i++) {
        if (isxdigit(hexString.charAt(i))) {
            cleanHex += hexString.charAt(i);
        }
    }
    
    int length = cleanHex.length() / 2;
    for (int i = 0; i < length; i++) {
        String byteString = cleanHex.substring(i * 2, i * 2 + 2);
        data[i] = (byte)strtol(byteString.c_str(), NULL, 16);
    }
    
    return length;
}

void decryptMessage(byte* ciphertext, int length) {
    Serial.println("\nStarting decryption process");
    printBytes("Encrypted data", ciphertext, length);
    
    // Perform CBC decryption
    byte prev_block[16];
    memcpy(prev_block, iv, 16);  // First block uses IV
    
    for (int i = 0; i < length; i += 16) {
        byte temp[16];
        
        // Save current ciphertext block
        byte current_block[16];
        memcpy(current_block, &ciphertext[i], 16);
        
        // Decrypt block
        aes128.decryptBlock(temp, current_block);
        
        // XOR with previous ciphertext block (or IV for first block)
        for (int j = 0; j < 16; j++) {
            decryptedtext[i + j] = temp[j] ^ prev_block[j];
        }
        
        // Save current block for next iteration
        memcpy(prev_block, current_block, 16);
    }
    
    // Remove PKCS7 padding
    int padding = decryptedtext[length - 1];
    if (padding > 0 && padding <= 16) {
        bool valid_padding = true;
        for (int i = length - padding; i < length; i++) {
            if (decryptedtext[i] != padding) {
                valid_padding = false;
                break;
            }
        }
        if (valid_padding) {
            length -= padding;
        }
    }
    
    // Print decrypted message
    Serial.print("Decrypted message: ");
    for (int i = 0; i < length; i++) {
        Serial.write(decryptedtext[i]);
    }
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    
    // Initialize AES
    aes128.setKey(key, 16);
    
    // Connect to sender's AP
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    
    // Construct server URL using gateway IP (sender's IP)
    serverURL = "http://" + WiFi.gatewayIP().toString() + "/";
    Serial.print("Server URL: ");
    Serial.println(serverURL);
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin(serverURL);
        int httpResponseCode = http.GET();
        
        if (httpResponseCode > 0) {
            String response = http.getString();
            response.trim();
            Serial.println("\nReceived encrypted data: " + response);
            
            // Convert hex string to bytes and decrypt
            byte ciphertext[128];
            int length = hexStringToByteArray(response, ciphertext);
            
            if (length > 0) {
                decryptMessage(ciphertext, length);
            }
        } else {
            Serial.print("HTTP error: ");
            Serial.println(httpResponseCode);
        }
        
        http.end();
    } else {
        Serial.println("WiFi disconnected, reconnecting...");
        WiFi.begin(ssid, password);
    }
    
    delay(5000);  // Wait 5 seconds between requests
}