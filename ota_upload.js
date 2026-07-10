const http = require('http');
const fs = require('fs');

const TARGET_IP = '192.168.10.11';
const action = process.argv[2];

if (!action) {
    console.log("Valid operations:");
    console.log("  node ota_upload.js on                  -> POST /led (JSON structure)");
    console.log("  node ota_upload.js off                 -> POST /led (JSON structure)");
    console.log("  node ota_upload.js stm32_ota.ino.bin   -> POST /ota (Binary upload)");
    process.exit(0);
}

function sendHttpRequest(path, method, headers, payloadData) {
    const options = {
        hostname: TARGET_IP,
        port: 80,
        path: path,
        method: method,
        headers: headers
    };

    const req = http.request(options, (res) => {
        let responseString = '';
        res.on('data', (chunk) => { responseString += chunk; });
        res.on('end', () => {
            console.log(`\n📥 Response [HTTP ${res.statusCode}]: ${responseString.trim()}`);
        });
    });

    req.on('error', (err) => {
        console.error(`\n❌ Network error:`, err.message);
    });

    req.write(payloadData);
    req.end();
}

// --- MODE: PROCESSING LED SWAPPINGS VIA JSON PAYLOAD ---
if (action.toLowerCase() === 'on' || action.toLowerCase() === 'off') {
    const targetStatus = action.toLowerCase() === 'on';
    const jsonBody = JSON.stringify({ status: targetStatus });

    console.log(`🔌 Dispatching JSON structural update to http://${TARGET_IP}/led`);
    
    sendHttpRequest('/led', 'POST', {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(jsonBody)
    }, jsonBody);
} 
// --- MODE: PROCESSING FIRMWARE IMAGE TRANSFER ---
else {
    if (!fs.existsSync(action)) {
        console.error(`❌ Error: Compilation file '${action}' missing.`);
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

    // Guard rail check before sending
    if (fileSizeInBytes > 245760) {
        console.error(`⚠️ Warning: Size exceeds safe STM32F411 partition thresholds (240 KB)! Update might fail.`);
    }

    console.log(`🚀 Streaming flashing data out to http://${TARGET_IP}/ota...`);

    sendHttpRequest('/ota', 'POST', {
        'Content-Type': 'application/octet-stream',
        'Content-Length': fileSizeInBytes
    }, binaryData);
}