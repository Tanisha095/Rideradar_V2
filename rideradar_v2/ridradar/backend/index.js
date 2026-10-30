const path = require('path');
// Load environment variables from backend/.env
require('dotenv').config({ path: path.join(__dirname, '.env') });

const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');
const axios = require('axios');

// CONFIGURATION FROM ENVIRONMENT (No keys directly in code)
const COM_PORT = process.env.COM_PORT || 'COM4';
const BAUD_RATE = parseInt(process.env.BAUD_RATE || '115200', 10);
function resolveSupabaseEndpoint(rawUrl) {
    if (!rawUrl) return null;
    let url = rawUrl.trim().replace(/\/+$/, '');
    if (url.endsWith('/rest/v1/buses')) return url;
    if (url.endsWith('/rest/v1')) return `${url}/buses`;
    return `${url}/rest/v1/buses`;
}

const SUPABASE_URL = resolveSupabaseEndpoint(process.env.SUPABASE_URL);
const SUPABASE_SERVICE_KEY = process.env.SUPABASE_SERVICE_KEY;
const SIMULATE_IF_NO_PORT = process.env.SIMULATE !== 'false';
const isMockMode = process.argv.includes('--mock') || process.argv.includes('--simulate') || process.env.MOCK === 'true';

console.log("==========================================");
console.log("       RideRadar Telemetry Bridge         ");
console.log("==========================================");
console.log(`Mode:               ${isMockMode ? "MOCK / SIMULATION" : "HARDWARE SERIAL"}`);
console.log(`Target Serial Port: ${COM_PORT} @ ${BAUD_RATE} baud`);
console.log(`Supabase Endpoint:  ${SUPABASE_URL || "(Not configured in backend/.env)"}`);
console.log(`Service Key Status: ${SUPABASE_SERVICE_KEY && SUPABASE_SERVICE_KEY !== 'your_supabase_service_role_key_here' ? "Loaded from backend/.env" : "NOT SET (will run in local simulation only)"}`);
console.log("------------------------------------------");

async function sendToSupabase(lat, lng, deviceId = "tbeam_001", extra = {}) {
    if (!SUPABASE_URL || !SUPABASE_SERVICE_KEY || SUPABASE_SERVICE_KEY === 'your_supabase_service_role_key_here') {
        console.log(`[Telemetry Local] Device: ${deviceId} -> lat=${lat.toFixed(6)}, lng=${lng.toFixed(6)} (Add SUPABASE_SERVICE_KEY in backend/.env to sync to cloud)`);
        return;
    }

    try {
        const payload = {
            id: deviceId,
            lat: parseFloat(lat),
            lng: parseFloat(lng),
            updated_at: new Date().toISOString(),
            ...extra
        };

        const response = await axios.post(
            SUPABASE_URL,
            payload,
            {
                headers: {
                    apikey: SUPABASE_SERVICE_KEY,
                    Authorization: `Bearer ${SUPABASE_SERVICE_KEY}`,
                    'Content-Type': 'application/json',
                    Prefer: 'resolution=merge-duplicates'
                },
                timeout: 5000
            }
        );
        console.log(`[Supabase Synced] Device: ${deviceId} -> lat=${lat}, lng=${lng}, speed=${extra.speed ?? 'N/A'}, sats=${extra.satellites ?? 'N/A'} (HTTP ${response.status})`);
    } catch (err) {
        console.error(`[Supabase Error] ${err.response ? JSON.stringify(err.response.data) : err.message}`);
    }
}

function parseGpsPayload(raw) {
    const text = String(raw).trim();
    if (!text) return null;

    // Format 1: "LAT:21.164800,LNG:72.784500,SAT:9,SPEED:25.4"
    if (text.includes("LAT:") && text.includes("LNG:")) {
        const latMatch = text.match(/LAT:([-+]?[0-9]*\.?[0-9]+)/i);
        const lngMatch = text.match(/LNG:([-+]?[0-9]*\.?[0-9]+)/i);
        const satMatch = text.match(/SAT(?:ELLITES)?:([0-9]+)/i);
        const speedMatch = text.match(/SPEED:([-+]?[0-9]*\.?[0-9]+)/i);
        if (latMatch && lngMatch) {
            return {
                lat: parseFloat(latMatch[1]),
                lng: parseFloat(lngMatch[1]),
                satellites: satMatch ? parseInt(satMatch[1], 10) : undefined,
                speed: speedMatch ? parseFloat(speedMatch[1]) : undefined
            };
        }
    }

    // Format 2: JSON format {"lat": 21.1648, "lng": 72.7845, ...}
    if (text.startsWith("{") && text.endsWith("}")) {
        try {
            const data = JSON.parse(text);
            if (data.lat && data.lng) {
                return {
                    lat: parseFloat(data.lat),
                    lng: parseFloat(data.lng),
                    speed: data.speed != null ? parseFloat(data.speed) : undefined,
                    satellites: data.satellites != null || data.sat != null ? parseInt(data.satellites || data.sat, 10) : undefined,
                    heading: data.heading != null ? parseFloat(data.heading) : undefined
                };
            }
        } catch (_) {}
    }

    // Format 3: CSV format "21.164800,72.784500" or "21.164800,72.784500,25.4,9"
    const parts = text.split(',');
    if (parts.length >= 2) {
        const lat = parseFloat(parts[0].trim());
        const lng = parseFloat(parts[1].trim());
        const speed = parts.length >= 3 && !isNaN(parseFloat(parts[2].trim())) ? parseFloat(parts[2].trim()) : undefined;
        const satellites = parts.length >= 4 && !isNaN(parseInt(parts[3].trim(), 10)) ? parseInt(parts[3].trim(), 10) : undefined;
        if (!isNaN(lat) && !isNaN(lng) && lat >= -90 && lat <= 90 && lng >= -180 && lng <= 180) {
            return { lat, lng, speed, satellites };
        }
    }

    return null;
}

// SIMULATION ROUTE: SVNIT Campus -> Surat Athwa Gate -> Ring Road
const SIMULATED_COORDINATES = [
    { lat: 21.1648, lng: 72.7845, speed: 24.2, satellites: 9, heading: 85 },
    { lat: 21.1672, lng: 72.7890, speed: 28.5, satellites: 9, heading: 88 },
    { lat: 21.1702, lng: 72.7950, speed: 32.1, satellites: 10, heading: 92 },
    { lat: 21.1735, lng: 72.8010, speed: 26.4, satellites: 10, heading: 95 },
    { lat: 21.1760, lng: 72.8120, speed: 29.8, satellites: 8, heading: 100 },
    { lat: 21.1780, lng: 72.8250, speed: 34.0, satellites: 9, heading: 105 },
    { lat: 21.1702, lng: 72.8311, speed: 22.0, satellites: 8, heading: 110 },
];

let simIndex = 0;
let simulationInterval = null;

function emitSimulatedPoint() {
    const point = SIMULATED_COORDINATES[simIndex];
    simIndex = (simIndex + 1) % SIMULATED_COORDINATES.length;
    console.log(`[Simulator] GPS tick: lat=${point.lat.toFixed(6)}, lng=${point.lng.toFixed(6)}, speed=${point.speed} km/h, sats=${point.satellites}`);
    sendToSupabase(point.lat, point.lng, "tbeam_001", {
        speed: point.speed,
        satellites: point.satellites,
        heading: point.heading
    });
}

function startSimulation(reason) {
    if (simulationInterval) return;
    console.log(`[Simulator] Activating GPS simulator (${reason})...`);
    console.log("[Simulator] Emitting Surat SVNIT route coordinates every 3 seconds.");
    
    // Immediate first tick so the map updates without delay
    emitSimulatedPoint();
    simulationInterval = setInterval(emitSimulatedPoint, 3000);
}

function initSerial() {
    let port;
    try {
        port = new SerialPort({ path: COM_PORT, baudRate: BAUD_RATE, autoOpen: false });
    } catch (e) {
        console.error(`[SerialPort Init Error] ${e.message}`);
        if (SIMULATE_IF_NO_PORT) startSimulation("SerialPort initialization failed");
        return;
    }

    port.on('error', (err) => {
        console.warn(`[SerialPort Warning] Port ${COM_PORT} error: ${err.message}`);
        if (SIMULATE_IF_NO_PORT) startSimulation(`Hardware port ${COM_PORT} unavailable`);
    });

    port.open((err) => {
        if (err) {
            console.warn(`[SerialPort] Could not open ${COM_PORT}: ${err.message}`);
            if (SIMULATE_IF_NO_PORT) {
                startSimulation(`Cannot open ${COM_PORT}`);
            }
            // Retry opening every 10 seconds if disconnected
            setTimeout(initSerial, 10000);
            return;
        }

        console.log(`[SerialPort] Successfully opened ${COM_PORT}. Listening for GPS data...`);
        if (simulationInterval) {
            clearInterval(simulationInterval);
            simulationInterval = null;
            console.log("[Simulator] Real hardware connected; stopped simulation.");
        }

        const parser = port.pipe(new ReadlineParser({ delimiter: '\n' }));
        parser.on('data', (raw) => {
            const trimmed = raw.trim();
            console.log("[Raw Serial Received]:", trimmed);

            const parsed = parseGpsPayload(trimmed);
            if (parsed) {
                console.log(`[GPS Parsed] lat=${parsed.lat}, lng=${parsed.lng}, speed=${parsed.speed ?? 'N/A'}, sats=${parsed.satellites ?? 'N/A'}`);
                sendToSupabase(parsed.lat, parsed.lng, "tbeam_001", {
                    speed: parsed.speed,
                    satellites: parsed.satellites,
                    heading: parsed.heading
                });
            }
        });

        port.on('close', () => {
            console.warn(`[SerialPort] Connection to ${COM_PORT} closed.`);
            if (SIMULATE_IF_NO_PORT) startSimulation("Hardware disconnected");
            setTimeout(initSerial, 10000);
        });
    });
}

// Check run mode
if (isMockMode) {
    startSimulation("CLI / Mock flag enabled");
} else {
    initSerial();
}