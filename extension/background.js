// SoundLink Bridge: finds the SoundLink app on 127.0.0.1 and answers its requests with what this browser
// already has - the YouTube visitor data and the PO token the real player used. Cookies leave the browser
// only if "Share cookies" is on in the popup and the app asks for them.
//
// The app listens, the extension keeps looking: neither needs the other.
//
// manifest.json sets its own CSP: Firefox's default one for MV3 has upgrade-insecure-requests, which would turn
// ws://127.0.0.1 into wss:// that the app does not speak.

const PORTS = [47821, 47822, 47823, 47824, 47825]; // keep in sync with BrowserBridge.cpp
const PROTOCOL = 1;
const PING_MS = 20_000;
const WELCOME_TIMEOUT_MS = 3_000;
const MIN_RETRY_MS = 2_000;
const MAX_RETRY_MS = 30_000;

let socket = null;
let port = 0;
let paired = false;
let needsPairing = false;
let pairError = "";
let connecting = false;
let retryDelay = MIN_RETRY_MS;
let retryTimer = null;
let pingTimer = null;
let lastError = "";

function browserName() {
    const ua = navigator.userAgent;
    if (ua.includes("Firefox/")) return "Firefox";
    if (ua.includes("Edg/")) return "Edge";
    if (ua.includes("OPR/")) return "Opera";
    if (ua.includes("YaBrowser/")) return "Yandex Browser";
    if (ua.includes("Chrome/")) return "Chrome";
    return "Browser";
}

function send(message) {
    if (socket && socket.readyState === WebSocket.OPEN)
        socket.send(JSON.stringify(message));
}

// ─── Finding the app ─────────────────────────────────────

function scheduleRetry() {
    if (retryTimer)
        return;
    retryTimer = setTimeout(() => {
        retryTimer = null;
        connect();
    }, retryDelay);
    retryDelay = Math.min(retryDelay * 2, MAX_RETRY_MS);
}

async function connect() {
    if (socket || connecting)
        return;

    connecting = true;
    for (const candidate of PORTS) {
        if (await tryPort(candidate)) {
            connecting = false;
            retryDelay = MIN_RETRY_MS;
            return;
        }
    }
    connecting = false;
    lastError = `Nothing answered on 127.0.0.1:${PORTS[0]}-${PORTS[PORTS.length - 1]}`;
    scheduleRetry();
}

// Resolves true once something on the port has introduced itself as SoundLink.
function tryPort(candidate) {
    return new Promise((resolve) => {
        let ws;
        try {
            ws = new WebSocket(`ws://127.0.0.1:${candidate}`);
        } catch {
            resolve(false);
            return;
        }

        let settled = false;
        const settle = (value) => {
            if (settled)
                return;
            settled = true;
            clearTimeout(timer);
            resolve(value);
        };
        const timer = setTimeout(() => {
            ws.close();
            settle(false);
        }, WELCOME_TIMEOUT_MS);

        ws.onopen = async () => {
            const { token } = await chrome.storage.local.get("token");
            ws.send(JSON.stringify({ type: "hello", protocol: PROTOCOL, browser: browserName(), token: token || "" }));
        };
        ws.onmessage = (event) => {
            let message;
            try {
                message = JSON.parse(event.data);
            } catch {
                return;
            }
            if (!settled && message.type === "welcome" && message.app === "SoundLink") {
                adopt(ws, candidate, message);
                settle(true);
            }
        };
        ws.onerror = () => {};
        ws.onclose = () => settle(false);
    });
}

async function adopt(ws, candidate, welcome) {
    socket = ws;
    port = candidate;
    paired = welcome.paired === true;
    needsPairing = !paired;
    pairError = "";
    lastError = "";

    // The app no longer knows our token (it was revoked there): pair again.
    if (!paired)
        await chrome.storage.local.remove("token");

    ws.onmessage = (event) => handleMessage(event.data);
    ws.onclose = () => {
        socket = null;
        port = 0;
        paired = false;
        needsPairing = false;
        clearInterval(pingTimer);
        scheduleRetry();
    };

    // Traffic also keeps a Chrome service worker alive.
    clearInterval(pingTimer);
    pingTimer = setInterval(() => send({ type: "ping" }), PING_MS);
}

async function handleMessage(data) {
    let message;
    try {
        message = JSON.parse(data);
    } catch {
        return;
    }

    switch (message.type) {
    case "paired":
        await chrome.storage.local.set({ token: String(message.token || "") });
        paired = true;
        needsPairing = false;
        pairError = "";
        break;
    case "pairFailed":
        pairError = message.attemptsLeft > 0
            ? `Wrong code, ${message.attemptsLeft} attempts left`
            : "Too many attempts: use the new code SoundLink shows";
        break;
    case "request":
        if (paired)
            handleRequest(message);
        break;
    }
}

// ─── Requests from the app ───────────────────────────────

async function handleRequest(request) {
    const reply = (fields) => send({ type: "response", id: request.id, ...fields });

    try {
        if (request.method === "credentials") {
            reply({ result: await getCredentials() });
        } else if (request.method === "cookies") {
            const { shareCookies } = await chrome.storage.local.get("shareCookies");
            if (shareCookies !== true) {
                reply({ error: "Cookie sharing is off in the SoundLink extension" });
                return;
            }
            reply({ result: { cookies: await getYouTubeCookies() } });
        } else {
            reply({ error: `Unknown request: ${request.method}` });
        }
    } catch (error) {
        reply({ error: String(error && error.message || error) });
    }
}

async function getCredentials() {
    const session = await chrome.storage.session.get(["visitorData", "loggedIn", "poToken", "poTokenAt"]);
    return {
        visitorData: session.visitorData || "",
        loggedIn: session.loggedIn === true,
        poToken: session.poToken || "",
        poTokenAgeSec: session.poTokenAt ? Math.round((Date.now() - session.poTokenAt) / 1000) : -1,
    };
}

async function getYouTubeCookies() {
    const cookies = await chrome.cookies.getAll({ domain: "youtube.com" });
    return cookies.map((cookie) => ({
        domain: cookie.domain,
        hostOnly: cookie.hostOnly,
        path: cookie.path,
        secure: cookie.secure,
        expirationDate: cookie.expirationDate || 0,
        name: cookie.name,
        value: cookie.value,
    }));
}

// ─── What the browser's own YouTube session shows ────────

// The player sends its GVS PO token - what YouTube's bot check produced here - with every media request:
// as the "pot" URL parameter for plain format URLs, or inside the protobuf body of SABR requests
// (VideoPlaybackAbrRequest.streamer_context = 19 -> StreamerContext.po_token = 2), which desktop web uses.

function readVarint(bytes, pos) {
    let value = 0;
    for (let shift = 0; pos < bytes.length && shift < 50; shift += 7) {
        const byte = bytes[pos++];
        value += (byte & 0x7f) * 2 ** shift;
        if (!(byte & 0x80))
            return [value, pos];
    }
    return [-1, pos];
}

// The first length-delimited field with this number in a protobuf message, or null.
function findProtoField(bytes, wanted) {
    let pos = 0;
    while (pos < bytes.length) {
        let key;
        [key, pos] = readVarint(bytes, pos);
        if (key < 0)
            return null;

        const field = Math.floor(key / 8);
        const wireType = key % 8;
        if (wireType === 0) {
            [, pos] = readVarint(bytes, pos);
        } else if (wireType === 1) {
            pos += 8;
        } else if (wireType === 5) {
            pos += 4;
        } else if (wireType === 2) {
            let length;
            [length, pos] = readVarint(bytes, pos);
            if (length < 0 || pos + length > bytes.length)
                return null;
            if (field === wanted)
                return bytes.subarray(pos, pos + length);
            pos += length;
        } else {
            return null;
        }
    }
    return null;
}

function toBase64Url(bytes) {
    let binary = "";
    for (const byte of bytes)
        binary += String.fromCharCode(byte);
    return btoa(binary).replace(/\+/g, "-").replace(/\//g, "_");
}

function tokenFromSabrBody(requestBody) {
    const parts = (requestBody && requestBody.raw || []).filter((part) => part.bytes);
    if (parts.length === 0)
        return "";

    const total = parts.reduce((sum, part) => sum + part.bytes.byteLength, 0);
    const body = new Uint8Array(total);
    let offset = 0;
    for (const part of parts) {
        body.set(new Uint8Array(part.bytes), offset);
        offset += part.bytes.byteLength;
    }

    const context = findProtoField(body, 19);
    const token = context && findProtoField(context, 2);
    return token && token.length >= 16 && token.length <= 2048 ? toBase64Url(token) : "";
}

chrome.webRequest.onBeforeRequest.addListener((details) => {
    try {
        let token = new URL(details.url).searchParams.get("pot") || "";
        if (!token && details.method === "POST")
            token = tokenFromSabrBody(details.requestBody);
        if (token.length > 20)
            chrome.storage.session.set({ poToken: token, poTokenAt: Date.now() });
    } catch {
        // not a request we understand
    }
}, { urls: ["*://*.googlevideo.com/videoplayback*"] }, ["requestBody"]);

function isOwnPage(sender) {
    return sender.id === chrome.runtime.id && !sender.tab && sender.url && sender.url.startsWith(chrome.runtime.getURL(""));
}

function isYouTubeTab(sender) {
    if (sender.id !== chrome.runtime.id || !sender.tab || !sender.url)
        return false;
    const host = new URL(sender.url).hostname;
    return host === "youtube.com" || host.endsWith(".youtube.com");
}

chrome.runtime.onMessage.addListener((message, sender, sendResponse) => {
    if (message && message.kind === "pageInfo" && isYouTubeTab(sender)) {
        chrome.storage.session.set({ visitorData: message.visitorData, loggedIn: message.loggedIn, pageSeenAt: Date.now() });
        return false;
    }

    if (!isOwnPage(sender))
        return false;

    // The popup.
    (async () => {
        switch (message && message.kind) {
        case "pair":
            pairError = "";
            send({ type: "pair", code: String(message.code || "").trim() });
            break;
        case "setShareCookies":
            await chrome.storage.local.set({ shareCookies: message.value === true });
            break;
        case "reconnect":
            retryDelay = MIN_RETRY_MS;
            connect();
            break;
        }
        sendResponse(await getStatus());
    })();
    return true;
});

async function getStatus() {
    const { shareCookies } = await chrome.storage.local.get("shareCookies");
    const credentials = await getCredentials();
    const { pageSeenAt } = await chrome.storage.session.get("pageSeenAt");
    return {
        connected: socket !== null,
        port,
        paired,
        needsPairing,
        pairError,
        lastError,
        shareCookies: shareCookies === true,
        loggedIn: credentials.loggedIn,
        poTokenAgeSec: credentials.poTokenAgeSec,
        youtubeSeen: Boolean(pageSeenAt),
    };
}

// ─── Lifetime ────────────────────────────────────────────

// A sleeping service worker is woken once a minute to look for the app again.
chrome.alarms.create("soundlink-reconnect", { periodInMinutes: 1 });
chrome.alarms.onAlarm.addListener((alarm) => {
    if (alarm.name === "soundlink-reconnect")
        connect();
});
chrome.runtime.onStartup.addListener(connect);
chrome.runtime.onInstalled.addListener(connect);
connect();
