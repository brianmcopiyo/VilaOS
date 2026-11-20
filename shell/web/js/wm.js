(function (g) {
  var wins = [];
  var zTop = 10;
  var drag = null;

  function find(id) {
    for (var i = 0; i < wins.length; i++) if (wins[i].id === id) return wins[i];
    return null;
  }

  function syncFromBridge(list) {
    var map = {};
    (list || []).forEach(function (w) { map[w.id] = w; });
    wins = wins.filter(function (w) { return map[w.id]; });
    (list || []).forEach(function (w) {
      var cur = find(w.id);
      if (!cur) {
        wins.push({
          id: w.id,
          title: w.title || w.app_id || "App",
          appId: w.app_id || "",
          x: w.x || 80,
          y: w.y || 60,
          w: w.w || 640,
          h: w.h || 420,
          z: ++zTop,
          minimized: !!w.minimized,
          maximized: !!w.maximized,
          focused: !!w.focused,
        });
      } else {
        cur.title = w.title || cur.title;
        cur.minimized = !!w.minimized;
        cur.maximized = !!w.maximized;
        cur.focused = !!w.focused;
        if (!drag || drag.id !== cur.id) {
          if (w.x != null) cur.x = w.x;
          if (w.y != null) cur.y = w.y;
          if (w.w != null) cur.w = w.w;
          if (w.h != null) cur.h = w.h;
        }
      }
    });
  }

  function focus(id) {
    wins.forEach(function (w) { w.focused = w.id === id; });
    var w = find(id);
    if (w) {
      w.z = ++zTop;
      w.minimized = false;
      g.VilaBridge.windowCmd("focus", id);
    }
  }

  function paint(container, work) {
    container.innerHTML = "";
    wins.slice().sort(function (a, b) { return a.z - b.z; }).forEach(function (w) {
      if (w.minimized) return;
      var el = document.createElement("div");
      el.className = "window" + (w.focused ? " focused" : "");
      el.style.left = (w.maximized ? work.x : w.x) + "px";
      el.style.top = (w.maximized ? work.y : w.y) + "px";
      el.style.width = (w.maximized ? work.w : w.w) + "px";
      el.style.height = (w.maximized ? work.h : w.h) + "px";
      el.style.zIndex = String(w.z);
      var bar = document.createElement("div");
      bar.className = "titlebar";
      var title = document.createElement("div");
      title.className = "title";
      title.textContent = w.title;
      bar.appendChild(title);
      ["min", "max", "close"].forEach(function (op) {
        var b = document.createElement("button");
        b.textContent = op === "min" ? "–" : op === "max" ? "□" : "×";
        b.onclick = function (ev) {
          ev.stopPropagation();
          if (op === "min") {
            w.minimized = true;
            g.VilaBridge.windowCmd("minimize", w.id);
          } else if (op === "max") {
            w.maximized = !w.maximized;
            g.VilaBridge.windowCmd("maximize", w.id);
          } else {
            g.VilaBridge.windowCmd("close", w.id);
          }
          g.DesktopShell.render();
        };
        bar.appendChild(b);
      });
      bar.onmousedown = function (ev) {
        if (ev.button !== 0) return;
        focus(w.id);
        if (w.maximized) return;
        drag = { id: w.id, mode: "move", ox: ev.clientX - w.x, oy: ev.clientY - w.y };
      };
      el.appendChild(bar);
      var client = document.createElement("div");
      client.className = "client";
      client.id = "win-client-" + w.id;
      client.onclick = function () { focus(w.id); };
      el.appendChild(client);
      ["e", "s", "se", "w", "n", "ne", "nw", "sw"].forEach(function (edge) {
        var r = document.createElement("div");
        r.className = "resize " + edge;
        r.onmousedown = function (ev) {
          ev.preventDefault();
          ev.stopPropagation();
          focus(w.id);
          drag = { id: w.id, mode: "resize", edge: edge, sx: ev.clientX, sy: ev.clientY, bx: w.x, by: w.y, bw: w.w, bh: w.h };
        };
        el.appendChild(r);
      });
      container.appendChild(el);
      if (g.DesktopShell && g.DesktopShell.fillWindow) g.DesktopShell.fillWindow(w, client);
    });
  }

  function onMove(ev) {
    if (!drag) return;
    var w = find(drag.id);
    if (!w) return;
    if (drag.mode === "move") {
      w.x = Math.max(0, ev.clientX - drag.ox);
      w.y = Math.max(0, ev.clientY - drag.oy);
      g.VilaBridge.windowCmd("move", w.id, { x: w.x, y: w.y });
    } else if (drag.mode === "resize") {
      var dx = ev.clientX - drag.sx;
      var dy = ev.clientY - drag.sy;
      var edge = drag.edge;
      var x = drag.bx, y = drag.by, ww = drag.bw, hh = drag.bh;
      if (edge.indexOf("e") >= 0) ww = Math.max(180, drag.bw + dx);
      if (edge.indexOf("s") >= 0) hh = Math.max(140, drag.bh + dy);
      if (edge.indexOf("w") >= 0) {
        ww = Math.max(180, drag.bw - dx);
        x = drag.bx + (drag.bw - ww);
      }
      if (edge.indexOf("n") >= 0) {
        hh = Math.max(140, drag.bh - dy);
        y = drag.by + (drag.bh - hh);
      }
      w.x = x; w.y = y; w.w = ww; w.h = hh;
      g.VilaBridge.windowCmd("resize", w.id, { x: x, y: y, w: ww, h: hh });
    }
    g.DesktopShell.render();
  }

  function onUp() {
    drag = null;
  }

  window.addEventListener("mousemove", onMove);
  window.addEventListener("mouseup", onUp);

  g.VilaWM = {
    sync: syncFromBridge,
    list: function () { return wins; },
    paint: paint,
    focus: focus,
    showDesktop: function () {
      wins.forEach(function (w) { w.minimized = true; });
      g.VilaBridge.windowCmd("show_desktop", "");
    },
  };
})(window);
