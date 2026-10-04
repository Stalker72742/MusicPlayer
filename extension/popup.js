const $ = (id) => document.getElementById(id);

function ago(seconds) {
    if (seconds < 90) return "just now";
    if (seconds < 90 * 60) return `${Math.round(seconds / 60)} min ago`;
    return `${Math.round(seconds / 3600)} h ago`;
}

function render(status) {
    if (!status)
        return;

    const appDot = $("appDot");
    if (!status.connected) {
        appDot.className = "dot";
        $("appText").textContent = "SoundLink is not running";
        $("appText").title = status.lastError || "";
    } else if (status.needsPairing) {
        appDot.className = "dot warn";
        $("appText").textContent = "Found SoundLink, pairing needed";
    } else {
        appDot.className = "dot ok";
        $("appText").textContent = "Connected to SoundLink";
    }

    const tokenDot = $("tokenDot");
    if (status.poTokenAgeSec >= 0 && status.poTokenAgeSec < 6 * 3600) {
        tokenDot.className = "dot ok";
        $("tokenText").textContent = `YouTube token from ${ago(status.poTokenAgeSec)}`
            + (status.loggedIn ? " (logged in)" : "");
    } else {
        tokenDot.className = "dot warn";
        $("tokenText").textContent = "No YouTube token yet: play any video on YouTube";
    }

    $("pairing").hidden = !status.needsPairing;
    $("pairError").textContent = status.pairError || "";
    $("offline").hidden = status.connected;
    $("shareCookies").checked = status.shareCookies;
}

async function call(message) {
    render(await chrome.runtime.sendMessage(message));
}

$("pair").addEventListener("click", () => call({ kind: "pair", code: $("code").value }));
$("code").addEventListener("keydown", (event) => {
    if (event.key === "Enter")
        call({ kind: "pair", code: $("code").value });
});
$("shareCookies").addEventListener("change", (event) => call({ kind: "setShareCookies", value: event.target.checked }));
$("reconnect").addEventListener("click", () => call({ kind: "reconnect" }));

call({ kind: "status" });
// Pairing answers and reconnects arrive asynchronously.
setInterval(() => call({ kind: "status" }), 1000);
