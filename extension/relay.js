// Isolated world: forwards what page.js reports to the background script.
window.addEventListener("message", (event) => {
    if (event.source !== window || event.origin !== window.location.origin)
        return;

    const data = event.data;
    if (!data || data.source !== "soundlink-page")
        return;

    chrome.runtime.sendMessage({
        kind: "pageInfo",
        visitorData: String(data.visitorData || "").slice(0, 512),
        loggedIn: data.loggedIn === true,
    }).catch(() => {});
});
