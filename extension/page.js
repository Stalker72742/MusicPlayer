// Runs in the page's own world on YouTube: reads the session the page already has (visitor data,
// whether someone is logged in) and hands it to relay.js. Nothing is changed on the page.
(() => {
    const report = () => {
        const config = window.ytcfg;
        if (!config || typeof config.get !== "function")
            return;

        // The visitor data moved around over time; the InnerTube context is what the player itself sends.
        const context = config.get("INNERTUBE_CONTEXT");
        const visitorData = (context && context.client && context.client.visitorData)
            || config.get("EOM_VISITOR_DATA") || config.get("VISITOR_DATA") || "";

        window.postMessage({
            source: "soundlink-page",
            visitorData,
            loggedIn: config.get("LOGGED_IN") === true,
        }, window.location.origin);
    };

    report();
    // YouTube is a single-page app: the session can change without a reload.
    document.addEventListener("yt-navigate-finish", report);
    setInterval(report, 60_000);
})();
