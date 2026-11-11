const http = require('http');
const fs = require('fs');
const path = require('path');

// Usage: node ota_upload.js [path/to/firmware.bin]
const TARGET_IP = '192.168.10.11';
const TARGET_PORT = 80;
const DEFAULT_BIN = path.join(__dirname, '.pio', 'build', 'blackpill_f411ce', 'firmware.bin');
const action = process.argv[2] || DEFAULT_BIN;

function sendHttpRequest(path, method, headers, payloadData) {
    const options = {
        hostname: TARGET_IP,
        port: TARGET_PORT,
        path: path,
        method: method,
        headers: headers
    };

    const req = http.request(options, (res) => {
        let responseString = '';
        res.on('data', (chunk) => { responseString += chunk; });
        res.on('end', () => {
            console.log(`\n📥 Response [HTTP ${res.statusCode}]: ${responseString.trim()}`);
            if (res.statusCode !== 200) {
                process.exitCode = 1;
            }
        });
    });

    req.on('error', (err) => {
        console.error(`\n❌ Network error:`, err.message);
        process.exitCode = 1;
    });

    req.write(payloadData);
    req.end();
}

// --- FIRMWARE IMAGE TRANSFER (OTA) ---
if (!fs.existsSync(action)) {
    console.error(`❌ Error: Firmware file '${action}' missing. Build first with 'pio run'.`);
    process.exit(1);
}

const binaryData = fs.readFileSync(action);
const fileSizeInBytes = binaryData.length;
const fileSizeInKB = (fileSizeInBytes / 1024).toFixed(2);

console.log(`\n📦 Firmware Target Details:`);
console.log(`----------------------------------------`);
console.log(`📄 File Name   : ${action}`);
console.log(`📊 Binary Size : ${fileSizeInBytes} bytes (${fileSizeInKB} KB)`);
console.log(`----------------------------------------`);

// STM32F411 flash size: 512 KB
// Warn at 30% (157,286 bytes), abort at 46% (241,172 bytes)
if (fileSizeInBytes > 241172) {
    console.error(`❌ Firmware size ${fileSizeInKB} KB exceeds 46% of STM32F411 flash (512 KB). Upload cancelled.`);
    process.exit(1);
}
if (fileSizeInBytes > 157286) {
    console.warn(`⚠️ Warning: Firmware size ${fileSizeInKB} KB exceeds 30% of STM32F411 flash (512 KB). Proceed with caution.`);
}

console.log(`🚀 Streaming flashing data out to http://${TARGET_IP}:${TARGET_PORT}/ota...`);

sendHttpRequest('/ota', 'POST', {
    'Content-Type': 'application/octet-stream',
    'Content-Length': fileSizeInBytes
}, binaryData);