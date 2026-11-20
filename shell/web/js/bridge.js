(function (g) {
  var ws = null;
  var listeners = {};
  var state = { packages: [], windows: [], widgets: {}, memoryMb: 0, gfx: "", clock: "" };
  var IPC = "ws://127.0.0.1:9081/ws";

  function emit(type, data) {
    (listeners[type] || []).forEach(function (fn) { try { fn(data); } catch (e) {} });
  }

  function connect() {
    ws = new WebSocket(IPC);
    ws.onopen = function () {
      send({ type: "hello", client: "chromium-shell" });
      send({ type: "subscribe", channels: ["packages", "windows", "widgets", "system"] });
    };
    ws.onmessage = function (ev) {
      var msg;
      try { msg = JSON.parse(ev.data); } catch (e) { return; }
      if (!msg || !msg.type) return;
      if (msg.type === "packages") {
        state.packages = msg.apps || [];
        emit("packages", state.packages);
      } else if (msg.type === "windows") {
        state.windows = msg.windows || [];
        emit("windows", state.windows);
      } else if (msg.type === "widgets") {
        state.widgets = Object.assign(state.widgets, msg.widgets || {});
        emit("widgets", state.widgets);
      } else if (msg.type === "system") {
        if (msg.memoryMb != null) state.memoryMb = msg.memoryMb;
        if (msg.gfx) state.gfx = msg.gfx;
        if (msg.clock) state.clock = msg.clock;
        emit("system", msg);
      } else if (msg.type === "event") {
        emit("event", msg);
      }
    };
    ws.onclose = function () { setTimeout(connect, 800); };
    ws.onerror = function () { try { ws.close(); } catch (e) {} };
  }

  function send(obj) {
    if (ws && ws.readyState === 1) ws.send(JSON.stringify(obj));
  }

  g.VilaBridge = {
    connect: connect,
    on: function (type, fn) {
      listeners[type] = listeners[type] || [];
      listeners[type].push(fn);
    },
    send: send,
    state: function () { return state; },
    openApp: function (id) { send({ type: "cmd", op: "openApp", id: id }); },
    windowCmd: function (op, id, extra) {
      send(Object.assign({ type: "cmd", op: op, id: id }, extra || {}));
    },
    uiEvent: function (id, action, value) {
      send({ type: "event", id: id || "", action: action || "", value: value || "" });
    },
    refreshPackages: function () { send({ type: "cmd", op: "pkg.refresh" }); },
  };
})(window);
