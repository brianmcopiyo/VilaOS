(function (g) {
  var root = null;
  var startOpen = false;
  var filter = "";

  function workArea() {
    var tb = 48;
    return { x: 0, y: 0, w: window.innerWidth, h: Math.max(64, window.innerHeight - tb) };
  }

  function fillWindow(win, client) {
    client.innerHTML = "";
    var title = document.createElement("div");
    title.className = "ui-text";
    title.style.padding = "12px";
    title.style.fontWeight = "700";
    title.textContent = win.title;
    var sub = document.createElement("div");
    sub.className = "ui-text";
    sub.style.padding = "0 12px 12px";
    sub.style.color = "#57534E";
    sub.textContent = "Package " + (win.appId || win.id) + " · React view host";
    client.appendChild(title);
    client.appendChild(sub);
    var widgets = g.VilaBridge.state().widgets || {};
    Object.keys(widgets).forEach(function (id) {
      if (String(id).indexOf(String(win.appId)) !== 0 && String(id).indexOf(String(win.id)) !== 0) return;
      var row = document.createElement("div");
      row.className = "ui-row";
      row.style.padding = "4px 12px";
      row.textContent = id + ": " + (widgets[id].text || widgets[id].subtitle || "");
      client.appendChild(row);
    });
  }

  function render() {
    if (!root) return;
    var st = g.VilaBridge.state();
    var work = workArea();
    root.innerHTML = "";
    var desk = document.createElement("div");
    desk.className = "desktop-root";
    var workspace = document.createElement("div");
    workspace.className = "workspace";
    workspace.id = "workspace";

    var header = document.createElement("div");
    header.className = "ui-panel";
    header.style.marginBottom = "12px";
    header.innerHTML = "";
    var hgrid = document.createElement("div");
    hgrid.className = "ui-grid";
    hgrid.style.gridTemplateColumns = "1fr auto";
    var titles = document.createElement("div");
    var t1 = document.createElement("div");
    t1.className = "ui-text";
    t1.style.fontSize = "18px";
    t1.style.fontWeight = "700";
    t1.id = "WorkspaceTitle";
    t1.textContent = "Desktop Workspace";
    var t2 = document.createElement("div");
    t2.className = "ui-text";
    t2.style.color = "#57534E";
    t2.id = "WorkspaceSubtitle";
    t2.textContent = (st.packages.length || 0) + " packages · " + (g.VilaWM.list().length || 0) + " open windows";
    titles.appendChild(t1);
    titles.appendChild(t2);
    var right = document.createElement("div");
    right.style.display = "flex";
    right.style.gap = "8px";
    right.style.alignItems = "center";
    var badge = document.createElement("span");
    badge.className = "ui-badge";
    badge.id = "SessionBadge";
    badge.textContent = (st.memoryMb || 0) + " MB";
    var refresh = document.createElement("button");
    refresh.className = "ui-button";
    refresh.style.background = "#EA580C";
    refresh.style.color = "#FFFFFF";
    refresh.style.border = "none";
    refresh.textContent = "Refresh";
    refresh.onclick = function () { g.VilaBridge.refreshPackages(); };
    right.appendChild(badge);
    right.appendChild(refresh);
    hgrid.appendChild(titles);
    hgrid.appendChild(right);
    header.appendChild(hgrid);
    workspace.appendChild(header);

    var body = document.createElement("div");
    body.className = "ui-grid";
    body.style.gridTemplateColumns = "1fr 260px";
    body.style.gap = "12px";
    body.style.height = "calc(100% - 84px)";

    var appsCard = document.createElement("div");
    appsCard.className = "ui-card";
    appsCard.style.height = "100%";
    appsCard.style.overflow = "auto";
    var appsTitle = document.createElement("div");
    appsTitle.className = "ui-row";
    appsTitle.innerHTML = "<div class='title'>Installed Applications</div><div class='subtitle'>Discovered from installed .vpkg packages</div>";
    appsCard.appendChild(appsTitle);
    var search = document.createElement("input");
    search.className = "ui-input";
    search.placeholder = "Filter installed apps...";
    search.value = filter;
    search.oninput = function () {
      filter = search.value || "";
      render();
    };
    appsCard.appendChild(search);
    var grid = document.createElement("div");
    grid.className = "app-grid";
    grid.id = "DynamicAppGrid";
    grid.style.marginTop = "10px";
    var q = filter.toLowerCase();
    (st.packages || []).forEach(function (app) {
      var name = app.name || app.id || "";
      if (q && name.toLowerCase().indexOf(q) < 0 && String(app.id || "").toLowerCase().indexOf(q) < 0) return;
      var tile = document.createElement("button");
      tile.className = "app-tile";
      tile.type = "button";
      tile.innerHTML = "<div class='name'></div><div class='sub'></div>";
      tile.querySelector(".name").textContent = name;
      tile.querySelector(".sub").textContent = app.id || "";
      tile.onclick = function () { g.VilaBridge.openApp(app.id); };
      grid.appendChild(tile);
    });
    appsCard.appendChild(grid);
    body.appendChild(appsCard);

    var sys = document.createElement("div");
    sys.className = "ui-card";
    sys.innerHTML = "";
    function addRow(title, sub, id) {
      var r = document.createElement("div");
      r.className = "ui-row";
      if (id) r.id = id;
      r.innerHTML = "<div class='title'></div><div class='subtitle'></div>";
      r.querySelector(".title").textContent = title;
      r.querySelector(".subtitle").textContent = sub;
      sys.appendChild(r);
    }
    addRow("System Overview", st.gfx || "Live host and compositor state", null);
    addRow("Memory", (st.memoryMb || 0) + " MB configured", "RamRow");
    addRow("Packages", (st.packages.length || 0) + " installed", "PackagesRow");
    addRow("Open Windows", (g.VilaWM.list().length || 0) + " active", "WindowsRow");
    body.appendChild(sys);
    workspace.appendChild(body);

    var layer = document.createElement("div");
    layer.id = "window-layer";
    layer.style.position = "absolute";
    layer.style.left = "0";
    layer.style.top = "0";
    layer.style.right = "0";
    layer.style.bottom = "0";
    layer.style.pointerEvents = "none";
    workspace.style.position = "relative";
    workspace.appendChild(layer);
    desk.appendChild(workspace);

    var tb = document.createElement("div");
    tb.className = "taskbar";
    tb.id = "DesktopTaskbar";
    var start = document.createElement("button");
    start.className = "start";
    start.textContent = "Start";
    start.onclick = function () {
      startOpen = !startOpen;
      render();
    };
    tb.appendChild(start);
    var slots = document.createElement("div");
    slots.style.display = "flex";
    slots.style.gap = "6px";
    slots.style.flex = "1";
    g.VilaWM.list().forEach(function (w) {
      var b = document.createElement("button");
      b.className = "slot" + (w.focused && !w.minimized ? " active" : "");
      b.textContent = w.title;
      b.onclick = function () {
        if (w.minimized) {
          w.minimized = false;
          g.VilaBridge.windowCmd("focus", w.id);
        } else if (w.focused) {
          w.minimized = true;
          g.VilaBridge.windowCmd("minimize", w.id);
        } else {
          g.VilaWM.focus(w.id);
        }
        render();
      };
      slots.appendChild(b);
    });
    tb.appendChild(slots);
    var deskBtn = document.createElement("button");
    deskBtn.className = "slot";
    deskBtn.textContent = "Desktop";
    deskBtn.onclick = function () {
      g.VilaWM.showDesktop();
      render();
    };
    tb.appendChild(deskBtn);
    var health = document.createElement("div");
    health.className = "health";
    health.id = "sys_health";
    health.textContent = (st.memoryMb || 0) + "MB";
    tb.appendChild(health);
    var clock = document.createElement("div");
    clock.className = "clock";
    clock.id = "sys_clock";
    clock.textContent = st.clock || "";
    tb.appendChild(clock);
    desk.appendChild(tb);

    if (startOpen) {
      var menu = document.createElement("div");
      menu.className = "start-menu";
      (st.packages || []).forEach(function (app) {
        var b = document.createElement("button");
        b.textContent = (app.name || app.id) + "  (" + (app.id || "") + ")";
        b.onclick = function () {
          startOpen = false;
          g.VilaBridge.openApp(app.id);
          render();
        };
        menu.appendChild(b);
      });
      desk.appendChild(menu);
    }

    root.appendChild(desk);
    layer.style.pointerEvents = "auto";
    g.VilaWM.paint(layer, work);
  }

  function boot() {
    root = document.getElementById("root");
    g.DesktopShell = { render: render, fillWindow: fillWindow };
    g.VilaBridge.on("packages", function () { render(); });
    g.VilaBridge.on("windows", function (list) {
      g.VilaWM.sync(list);
      render();
    });
    g.VilaBridge.on("system", function () { render(); });
    g.VilaBridge.on("widgets", function () { render(); });
    window.addEventListener("resize", function () { render(); });
    g.VilaBridge.connect();
    render();
    setInterval(function () {
      g.VilaBridge.send({ type: "cmd", op: "system.tick" });
    }, 1000);
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", boot);
  else boot();
})(window);
